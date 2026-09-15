#!/usr/bin/env python3
"""Measure Omaweb's startup and memory against the budget in `performance/budget.json`.

What the build costs is held to a number by `scripts/benchmark_build.sh`, and what one selector
match costs by `tests/benchmarks/`. How long the browser takes to appear, and what it costs to keep
Spaces open, were held to nothing, so this measures them and fails when a recorded ceiling is
crossed.

Four measurements, each its own subcommand so a developer can run the one they are working on:

- `startup` launches the browser and times process start to the window mapping.
- `memory` reads the resident memory of the process tree with one Space and one page.
- `spaces` opens Spaces one at a time and reports what each adds, which is the price of the engine
  profile per Space that ADR 0008 buys.
- `freezing` puts away a Space whose page allocates on a timer and reports how much it went on
  taking, which is the claim ADR 0033 makes and nothing checked.

This writes nothing outside the throwaway directories it launches its own browser on, and it
launches that browser on a private session bus, so unlike the theme and default-browser checks it
needs no opt-in guard: it puts nothing back because it put nothing anywhere. It does take the
keyboard focus while it runs.

The window mapping is read from the browser's own Wayland protocol log rather than from a
compositor, because CI's compositor is not the reader's. `WAYLAND_DEBUG=1` costs about a thousand
lines of stderr over a launch, which is inside the noise of the thing being measured and is paid
identically on every run.

Memory is the process tree's, not one process's: QtWebEngine runs renderers as children, and a
number that omits them measures nothing that matters. It is proportional set size rather than
resident set size, because the engine's processes share a great deal and adding their RSS counts
the shared pages once per process.

Keys reach the browser through Hyprland's own dispatcher where there is one, which aims them at the
window under test, and through `wtype` otherwise, which aims them wherever focus is. A live desktop
therefore keeps its keystrokes; a headless compositor with one window has nowhere else to put them.

Usage:

    scripts/benchmark_runtime.py
    scripts/benchmark_runtime.py startup --browser build/dev/omaweb
    scripts/benchmark_runtime.py spaces --spaces 4
    scripts/benchmark_runtime.py --record
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import random
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUDGET = ROOT / "performance" / "budget.json"

# A blank document with a title to wait on. Startup is not the place to measure a network round
# trip, and a Space's cost is not the place to measure whatever a real site loaded today.
PROBE_TITLE = "Omaweb runtime budget probe"

PROBE_PAGE = f"""<!doctype html>
<meta charset="utf-8">
<title>{PROBE_TITLE}</title>
<h1>runtime budget probe</h1>
"""

ALLOCATOR_TITLE = "Omaweb runtime budget allocator"

# What Freezing is checked against. ADR 0033 says a frozen page keeps everything it holds and runs
# no timers, animations or script, so a page whose script takes a megabyte every fifth of a second
# grows while it is the Space on show and stops the moment that Space is put away. The cap is there
# because a run that finds Freezing broken should report it rather than fill the machine.
ALLOCATOR_PAGE = f"""<!doctype html>
<meta charset="utf-8">
<title>{ALLOCATOR_TITLE}</title>
<h1>runtime budget allocator</h1>
<script>
  const held = [];
  setInterval(() => {{
    if (held.length < 500) held.push(new Uint8Array(1024 * 1024).fill(1));
  }}, 200);
</script>
"""

# `new-space` has no default binding, and a benchmark that opened the Space dialog by pointing at
# the sidebar would be measuring where a button happens to sit.
NEW_SPACE_BINDING = "Primary+Alt+N"

# The first attempt is the one that races the shell finishing its load, and on a machine under
# memory pressure it loses. Each attempt after it costs a few seconds and the run is minutes.
OPEN_SPACE_ATTEMPTS = 6

# Long enough for the shell to answer a command, short enough that a run is a wait rather than a
# coffee break. The same value `omaweb_session` settles on, for the same reason.
SETTLE = 0.8

# A page load crosses a process boundary and reads a file, so it gets its own.
LOAD_SETTLE = 3.0

# A character typed into a field that is already open is not a command, and waiting a command's
# settle between the forty-odd keys of a `file:` address is most of a run's wall clock.
TYPE_SETTLE = 0.08

# Long enough for the allocator page to put a measurable amount behind it, against a reading whose
# own noise is a megabyte or two.
ON_SHOW_WINDOW = 6.0

# Longer, because this is the window the page has to be shown not to have used. Freezing is not
# instant either, so the reading it is measured from is taken after the Space has gone.
AWAY_WINDOW = 10.0

# How long a launch may take before the run is a failure rather than a slow measurement. Well above
# any ceiling worth recording, because crossing a ceiling has to be reported rather than timed out.
LAUNCH_TIMEOUT = 120.0

# How long the browser has to become a browser that answers a command. A window that has mapped is
# not yet a shell with its bindings loaded, and on a machine under memory pressure the gap between
# the two is seconds rather than milliseconds.
READY_TIMEOUT = 60.0

KIB_PER_MIB = 1024.0

# What a binding writes against what the compositor calls the same key.
KEYSYMS = {"=": "equal", "-": "minus", ".": "period", "/": "slash", ",": "comma", ";": "semicolon"}

# Hyprland's dispatcher sends the key rather than the character, and applies no modifier it was not
# given, so a character that lives on a shifted key has to say so. Only the ones the strings typed
# here contain are named; `Workspace` keeps the rest out of them.
SHIFTED = {":": "semicolon"}

# The toplevel's surface, learned from the request that makes it a window.
TOPLEVEL_SURFACE = re.compile(r"-> xdg_wm_base#\d+\.get_xdg_surface\(new id xdg_surface#\d+, "
                              r"wl_surface#(\d+)\)")


class BenchmarkError(RuntimeError):
    """The machine cannot run this measurement, which is not a crossed budget."""


def log(message: str) -> None:
    print(message, flush=True)


def load_budget() -> dict:
    with open(BUDGET, encoding="utf-8") as handle:
        return json.load(handle)


class Keyboard:
    """Synthesised keys, aimed at the window under test where the compositor allows it.

    Hyprland's `send_shortcut` takes a window, so a run on a live desktop cannot type into whatever
    the reader was doing. `wtype` speaks the virtual-keyboard protocol any wlroots compositor
    offers and types wherever focus is, which is safe on the headless compositor CI runs and is
    why the Hyprland path is preferred rather than dropped.
    """

    def __init__(self, pid: int) -> None:
        self.pid = pid
        self.hyprland = shutil.which("hyprctl") is not None and bool(
            subprocess.run(["hyprctl", "version"], capture_output=True, text=True,
                           check=False).stdout.strip())
        if not self.hyprland and shutil.which("wtype") is None:
            raise BenchmarkError(
                "neither Hyprland nor wtype is here, and these measurements need a keyboard")

    def focus(self) -> None:
        """Gives the window under test the keyboard, where the compositor has a way to be asked.

        A text field takes no character while another window holds the focus, which is how a run
        came to report the memory of a browser with one Space as a browser with four. The headless
        compositor CI runs has one window and hands it the focus already.
        """
        if self.hyprland:
            subprocess.run(["hyprctl", "dispatch", f'hl.dsp.focus{{window="pid:{self.pid}"}}'],
                           capture_output=True, check=False)
            time.sleep(SETTLE)

    def press(self, binding: str, settle: float = SETTLE) -> None:
        parts = binding.split("+")
        key = KEYSYMS.get(parts[-1], parts[-1])
        modifiers = [{"Primary": "ctrl", "Shift": "shift", "Alt": "alt"}.get(part, part.lower())
                     for part in parts[:-1]]
        if self.hyprland:
            expression = (f'hl.dsp.send_shortcut{{mods="{" ".join(m.upper() for m in modifiers)}", '
                          f'key="{key}", window="pid:{self.pid}"}}')
            subprocess.run(["hyprctl", "dispatch", expression], capture_output=True, check=False)
        else:
            command = ["wtype"]
            for modifier in modifiers:
                command += ["-M", modifier]
            command += ["-k", key]
            for modifier in reversed(modifiers):
                command += ["-m", modifier]
            subprocess.run(command, capture_output=True, check=False)
        time.sleep(settle)

    def write(self, text: str) -> None:
        if self.hyprland:
            # One dispatch per character, because the dispatcher sends a key rather than a string.
            for character in text:
                if character in SHIFTED:
                    self.press(f"Shift+{SHIFTED[character]}", TYPE_SETTLE)
                elif character.isupper():
                    self.press(f"Shift+{character.lower()}", TYPE_SETTLE)
                else:
                    self.press(KEYSYMS.get(character, character), TYPE_SETTLE)
            return
        subprocess.run(["wtype", text], capture_output=True, check=False)
        time.sleep(SETTLE)


def read_pss_kib(pid: int) -> int:
    try:
        with open(f"/proc/{pid}/smaps_rollup", encoding="utf-8") as handle:
            for line in handle:
                if line.startswith("Pss:"):
                    return int(line.split()[1])
    except (OSError, IndexError, ValueError):
        return 0
    return 0


def children_of() -> dict[int, list[int]]:
    tree: dict[int, list[int]] = {}
    for entry in os.scandir("/proc"):
        if not entry.name.isdigit():
            continue
        try:
            with open(f"/proc/{entry.name}/stat", encoding="utf-8") as handle:
                parent = int(handle.read().rsplit(")", 1)[1].split()[1])
        except (OSError, IndexError, ValueError):
            continue
        tree.setdefault(parent, []).append(int(entry.name))
    return tree


def tree_mib(root: int) -> float:
    """Proportional set size of a process and everything below it, in mebibytes."""
    tree = children_of()
    total = 0
    pending = [root]
    while pending:
        pid = pending.pop()
        total += read_pss_kib(pid)
        pending += tree.get(pid, [])
    return total / KIB_PER_MIB


class Browser:
    """A browser of its own, measured rather than read.

    Its session bus is private so a launch does not hand its address to the browser the reader
    already has open and exit, and its data and configuration roots are thrown away, so nothing it
    does is theirs and nothing theirs is in the number.
    """

    def __init__(self, executable: str, root: str) -> None:
        self.executable = executable
        self.root = root
        self.log_path = os.path.join(root, "wayland.log")
        self.process: subprocess.Popen | None = None
        self.sink = None
        self.started_at = 0.0
        self.mapped_at = 0.0

    def start(self, url: str, keybindings: str | None = None) -> None:
        environment = dict(os.environ)
        environment["OMAWEB_DATA_ROOT"] = os.path.join(self.root, "data")
        environment["OMAWEB_CONFIG_ROOT"] = os.path.join(self.root, "config")
        environment["WAYLAND_DEBUG"] = "1"
        if keybindings:
            environment["OMAWEB_KEYBINDINGS_FILE"] = keybindings
        self.sink = open(self.log_path, "w", encoding="utf-8")
        self.started_at = time.time()
        self.process = subprocess.Popen(
            ["dbus-run-session", "--", self.executable, url],
            env=environment,
            stdout=subprocess.DEVNULL,
            stderr=self.sink,
            start_new_session=True,
        )
        self.mapped_at = self._await_mapping()

    def _await_mapping(self) -> float:
        """When the toplevel's surface first carried a buffer.

        The alternative is a compositor's window list, and the two compositors this has to run
        under do not share one. The protocol is the same under both.

        The log carries a timestamp of its own and this ignores it. Which clock libwayland writes
        there is libwayland's business, and reading it wrong is a whole timezone of error; the poll
        below costs a couple of milliseconds against a measurement in seconds.
        """
        deadline = self.started_at + LAUNCH_TIMEOUT
        surface = ""
        attached = False
        position = 0
        while time.time() < deadline:
            assert self.process is not None
            if self.process.poll() is not None:
                raise BenchmarkError("the browser under test exited before it mapped a window")
            with open(self.log_path, encoding="utf-8", errors="replace") as handle:
                handle.seek(position)
                for line in handle:
                    if not surface:
                        match = TOPLEVEL_SURFACE.search(line)
                        if match:
                            surface = match.group(1)
                        continue
                    if f"-> wl_surface#{surface}.attach(wl_buffer#" in line:
                        attached = True
                    elif attached and f"-> wl_surface#{surface}.commit()" in line:
                        return time.time()
                position = handle.tell()
            time.sleep(0.002)
        raise BenchmarkError("the browser under test never mapped a window")

    def await_title(self, fragment: str) -> None:
        """Waits for the browser to name the page in its window title.

        A mapped window is not yet a browser that answers a command: the shell is still loading and
        the page is still arriving, and a key sent into that gap lands somewhere harmless and
        silently. The title the engine hands the compositor is the first thing that says the page
        is up, and it is in the same protocol log the mapping is, so it needs no compositor either.
        """
        deadline = time.time() + READY_TIMEOUT
        wanted = f'set_title("{fragment}'
        while time.time() < deadline:
            assert self.process is not None
            if self.process.poll() is not None:
                raise BenchmarkError("the browser under test exited before it loaded a page")
            with open(self.log_path, encoding="utf-8", errors="replace") as handle:
                if any(wanted in line for line in handle):
                    return
            time.sleep(0.1)
        raise BenchmarkError(f"the browser never showed a page titled {fragment!r}")

    @property
    def startup_seconds(self) -> float:
        return self.mapped_at - self.started_at

    @property
    def pid(self) -> int:
        assert self.process is not None
        return self.process.pid

    @property
    def window_pid(self) -> int:
        """The browser's own process, which is not the one this started.

        What this starts is `dbus-run-session`, and the compositor has never heard of it. A key
        aimed at a window by the launcher's process id matches no window and lands wherever the
        focus already was, which on a live desktop is the reader's own work and in a measurement is
        a browser that did nothing it was asked. Breadth first, so the browser is found before the
        engine's children, which carry the same name.
        """
        name = os.path.basename(self.executable)[:15]
        tree = children_of()
        pending = list(tree.get(self.pid, []))
        while pending:
            candidate = pending.pop(0)
            try:
                with open(f"/proc/{candidate}/comm", encoding="utf-8") as handle:
                    if handle.read().strip() == name:
                        return candidate
            except OSError:
                pass
            pending += tree.get(candidate, [])
        raise BenchmarkError("the browser's own process is not under the launcher this started")

    def memory_mib(self) -> float:
        return tree_mib(self.pid)

    def stop(self) -> None:
        """Ends the whole process group.

        Terminating the launcher alone leaves the engine's zygote and renderers holding their
        memory, which on a machine with a few gigabytes is enough for the next launch to be killed
        by the kernel rather than by us.
        """
        if self.process:
            for stage in (signal.SIGTERM, signal.SIGKILL):
                if self.process.poll() is not None:
                    break
                try:
                    os.killpg(os.getpgid(self.process.pid), stage)
                except (ProcessLookupError, PermissionError):
                    break
                try:
                    self.process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    pass
            self.process = None
        if self.sink:
            self.sink.close()
            self.sink = None


class Workspace:
    """The throwaway root a run browses in, and the files it needs there."""

    def __init__(self) -> None:
        # Not `mkdtemp`, whose suffix carries uppercase letters and underscores. The probe page's
        # address is typed a key at a time, and every shifted character in it is a keysym this has
        # to know; a lowercase directory leaves the colon as the only one.
        self.root = os.path.join(
            tempfile.gettempdir(),
            "omaweb-budget-" + "".join(random.choices("abcdefghijklmnopqrstuvwxyz0123456789", k=8)))
        os.mkdir(self.root, 0o700)
        self.page_url = self._write("probe.html", PROBE_PAGE)
        self.allocator_url = self._write("allocator.html", ALLOCATOR_PAGE)
        self.keybindings = os.path.join(self.root, "keybindings.json")
        with open(ROOT / "assets" / "keybindings" / "default.json", encoding="utf-8") as handle:
            bindings = json.load(handle)
        bindings["browser"][NEW_SPACE_BINDING] = "new-space"
        with open(self.keybindings, "w", encoding="utf-8") as handle:
            json.dump(bindings, handle)

    def _write(self, name: str, body: str) -> str:
        path = os.path.join(self.root, name)
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(body)
        return f"file://{path}"

    def browser(self, executable: str) -> Browser:
        return Browser(executable, self.root)

    def space_count(self) -> int:
        """How many Spaces the browser has actually made, one directory each.

        A run whose keys went somewhere other than the Space dialog would otherwise report the
        memory of a browser with one Space as the memory of a browser with five, which is the one
        way this measurement can be wrong and say nothing.
        """
        spaces = os.path.join(self.root, "data", "spaces")
        if not os.path.isdir(spaces):
            return 0
        return sum(1 for entry in os.scandir(spaces) if entry.is_dir())

    def discard(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)


def open_space(keyboard: Keyboard, workspace: Workspace, name: str, expected: int,
               url: str = "") -> None:
    """Creates a Space, switches to it, and loads the probe page in its first tab.

    A Space nobody has selected has no engine to pay for, which is the whole of ADR 0033, so a
    Space counted here is one a reader has actually opened a page in.

    The Space is counted before the page is loaded rather than assumed. Keys sent to a window that
    was not ready for them land somewhere harmless and silently, and the measurement that follows
    would be of a browser that did nothing it was asked.
    """
    for _ in range(OPEN_SPACE_ATTEMPTS):
        keyboard.focus()
        keyboard.press(NEW_SPACE_BINDING)
        keyboard.write(name)
        keyboard.press("Return")
        time.sleep(SETTLE)
        if workspace.space_count() == expected:
            break
        # Whatever took the keys, this closes, so the next attempt starts where the first did.
        keyboard.press("Escape")
    else:
        raise BenchmarkError(f"the browser would not open Space {expected}")
    keyboard.press("Primary+L")
    keyboard.write(url or workspace.page_url)
    keyboard.press("Return")
    time.sleep(LOAD_SETTLE)


def measure_startup(executable: str, repetitions: int) -> dict:
    """The median of several launches, because one launch is a cold cache and a coin toss."""
    workspace = Workspace()
    timings = []
    try:
        for index in range(repetitions):
            browser = workspace.browser(executable)
            try:
                browser.start(workspace.page_url)
                timings.append(browser.startup_seconds)
                log(f"  launch {index + 1}: {browser.startup_seconds:.3f} s")
            finally:
                browser.stop()
                # The engine's processes have to be gone before the next launch, or the next
                # launch competes with them for the memory it is about to be measured holding.
                time.sleep(SETTLE)
    finally:
        workspace.discard()
    timings.sort()
    return {"startup_seconds": timings[len(timings) // 2]}


def measure_memory(executable: str) -> dict:
    workspace = Workspace()
    browser = workspace.browser(executable)
    try:
        browser.start(workspace.page_url)
        browser.await_title(PROBE_TITLE)
        time.sleep(LOAD_SETTLE)
        return {"resident_megabytes": browser.memory_mib()}
    finally:
        browser.stop()
        workspace.discard()


def measure_spaces(executable: str, count: int) -> dict:
    """What each Space after the first adds, averaged over the ones this opened."""
    workspace = Workspace()
    browser = workspace.browser(executable)
    try:
        browser.start(workspace.page_url, keybindings=workspace.keybindings)
        browser.await_title(PROBE_TITLE)
        keyboard = Keyboard(browser.window_pid)
        time.sleep(LOAD_SETTLE)
        keyboard.focus()
        first = browser.memory_mib()
        log(f"  1 Space: {first:.1f} MiB")
        for index in range(2, count + 1):
            open_space(keyboard, workspace, f"space{index}", index)
            log(f"  {index} Spaces: {browser.memory_mib():.1f} MiB")
        last = browser.memory_mib()
        opened = workspace.space_count()
    finally:
        browser.stop()
        workspace.discard()
    if opened != count:
        raise BenchmarkError(f"asked for {count} Spaces and the browser made {opened}")
    return {"space_megabytes": (last - first) / max(count - 1, 1)}


def measure_freezing(executable: str) -> dict:
    """What an away Space's pages go on spending once they are frozen.

    ADR 0033 keeps a frozen page's document and process and stops its timers, animations and
    script. The memory it holds is therefore bought deliberately and is not what this looks for;
    what it looks for is the page still running, which is what a broken Freezing looks like from
    outside and what the reader pays for in a Space they are not reading.

    So the page in the away Space allocates on a timer, and the same process tree is read twice
    while that Space is on show and twice more once it is away. The first pair is the control: a
    page that did not grow while it was being read proves nothing by not growing afterwards, and
    this says so rather than reporting a flat line as a pass.
    """
    workspace = Workspace()
    browser = workspace.browser(executable)
    try:
        browser.start(workspace.page_url, keybindings=workspace.keybindings)
        browser.await_title(PROBE_TITLE)
        keyboard = Keyboard(browser.window_pid)
        time.sleep(LOAD_SETTLE)
        keyboard.focus()
        open_space(keyboard, workspace, "away", 2, workspace.allocator_url)
        browser.await_title(ALLOCATOR_TITLE)
        running = browser.memory_mib()
        time.sleep(ON_SHOW_WINDOW)
        grew = browser.memory_mib() - running
        log(f"  the away Space's page grew {grew:.1f} MiB in {ON_SHOW_WINDOW:.0f} s on show")
        keyboard.press("Primary+1")
        time.sleep(SETTLE)
        frozen = browser.memory_mib()
        time.sleep(AWAY_WINDOW)
        growth = browser.memory_mib() - frozen
        log(f"  and {growth:.1f} MiB in {AWAY_WINDOW:.0f} s away")
        opened = workspace.space_count()
    finally:
        browser.stop()
        workspace.discard()
    if opened != 2:
        raise BenchmarkError(f"this needs two Spaces to put one away, and the browser made {opened}")
    if grew <= 0:
        raise BenchmarkError("the away Space's page never grew while it was on show, so its not "
                             "growing afterwards says nothing about Freezing")
    return {"frozen_growth_megabytes": growth}


MEASUREMENTS = {
    "startup": lambda arguments: measure_startup(arguments.browser, arguments.repetitions),
    "memory": lambda arguments: measure_memory(arguments.browser),
    "spaces": lambda arguments: measure_spaces(arguments.browser, arguments.spaces),
    "freezing": lambda arguments: measure_freezing(arguments.browser),
}


def report(results: dict, budget: dict) -> int:
    """Prints every measurement beside its threshold, and answers how many crossed it.

    Passing numbers are printed too. A budget that only speaks when it is broken hides the drift
    that is about to break it.
    """
    thresholds = budget["measurements"]
    crossed = 0
    units = {"startup_seconds": "s"}
    log("")
    log(f"budget recorded on: {budget['machine']}")
    log("")
    for name, value in results.items():
        threshold = thresholds[name]
        unit = units.get(name, "MiB")
        if "ceiling" in threshold:
            over = value > threshold["ceiling"]
            against = f"ceiling {threshold['ceiling']:.1f} {unit}"
        else:
            over = value < threshold["floor"]
            against = f"floor {threshold['floor']:.1f} {unit}"
        crossed += int(over)
        headroom = abs(value - threshold.get("ceiling", threshold.get("floor")))
        log(f"{'CROSSED' if over else 'within '}  {name}: {value:.2f} {unit} "
            f"against {against} ({headroom:.2f} {unit} of headroom)")
    log("")
    return crossed


def record(results: dict, budget: dict) -> None:
    """Writes the measurements back as what the budget was recorded at.

    The ceilings themselves are not touched. What counts as too slow is a decision, reviewed like
    any other, and a script that moved it every time a machine ran slower would be a budget that
    ratchets itself out of existence.
    """
    for name, value in results.items():
        budget["measurements"][name]["recorded"] = round(value, 2)
    budget["recorded_on"] = datetime.date.today().isoformat()
    with open(BUDGET, "w", encoding="utf-8") as handle:
        json.dump(budget, handle, indent=2)
        handle.write("\n")
    log(f"recorded into {BUDGET.relative_to(ROOT)}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("measurement", nargs="*",
                        help=f"which of {', '.join(MEASUREMENTS)} to run, all of them by default")
    parser.add_argument("--browser", default="build/dev/omaweb", help="the browser to measure")
    parser.add_argument("--repetitions", type=int, default=3,
                        help="launches to take the median startup from")
    parser.add_argument("--spaces", type=int, default=4,
                        help="how many Spaces to open, the first one included")
    parser.add_argument("--record", action="store_true",
                        help="write the measurements into the budget as its recorded numbers")
    arguments = parser.parse_args()

    unknown = [name for name in arguments.measurement if name not in MEASUREMENTS]
    if unknown:
        parser.error(f"no such measurement: {', '.join(unknown)}")

    if not os.access(arguments.browser, os.X_OK):
        log(f"skipped: {arguments.browser} is not built")
        return 0
    if not os.environ.get("WAYLAND_DISPLAY"):
        log("skipped: this measures a browser on a Wayland display, and there is none")
        return 0

    budget = load_budget()
    results: dict[str, float] = {}
    for name in arguments.measurement or list(MEASUREMENTS):
        log(f"{name}:")
        try:
            results.update(MEASUREMENTS[name](arguments))
        except BenchmarkError as error:
            log(f"skipped: {error}")
            return 0

    crossed = report(results, budget)
    if arguments.record:
        record(results, budget)
    return 1 if crossed else 0


if __name__ == "__main__":
    sys.exit(main())
