#!/usr/bin/env python3
"""Measure Omaweb's startup, memory and page loads against the budget in `performance/budget.json`.

What the build costs is held to a number by `scripts/benchmark_build.sh`, and what one selector
match costs by `tests/benchmarks/`. How long the browser takes to appear, and what it costs to keep
Spaces open, were held to nothing, so this measures them and fails when a recorded ceiling is
crossed.

Five measurements, each its own subcommand so a developer can run the one they are working on:

- `startup` launches the browser and times process start to the window mapping.
- `memory` reads the resident memory of the process tree with one Space and one page.
- `spaces` opens Spaces one at a time and reports what each adds, which is the price of the engine
  profile per Space that ADR 0008 buys.
- `freezing` puts away a Space whose page allocates on a timer and reports how much it went on
  taking, which is the claim ADR 0033 makes and nothing checked.
- `pageload` loads the same pages with Content blocking on and with the site switched off, and
  reports what blocking added to each, which is the cost ADR 0050 measured once by hand.

This writes nothing outside the throwaway directories it launches its own browser on, `--record`
aside, which writes the measurements into the budget in this repository. It launches that browser on
a private session bus, so unlike the theme and default-browser checks it needs no opt-in guard: it
puts nothing back because it put nothing anywhere. It does take the keyboard focus while it runs.
`pageload` runs its browser in a network namespace of its own, with its own resolver files bound
over the machine's, so the DNS server it starts answers that browser and nothing else.

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
That second half is why the keyboard here is not `omaweb_session`'s: that one drives a live Hyprland
and reads `hyprctl clients` back, and CI has neither.

Usage:

    scripts/benchmark_runtime.py
    scripts/benchmark_runtime.py startup --browser build/dev/omaweb
    scripts/benchmark_runtime.py spaces --spaces 4
    scripts/benchmark_runtime.py pageload --require-dns
    scripts/benchmark_runtime.py --record
"""

from __future__ import annotations

import argparse
import base64
import dataclasses
import datetime
import http.server
import json
import os
import random
import re
import shutil
import signal
import socket
import statistics
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUDGET = ROOT / "performance" / "budget.json"
FILTER_LISTS = ROOT / "third_party" / "filter-lists"

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

# The page ADR 0050 measured: forty images, which no rule refuses, so a load with blocking on
# fetches everything the load with it off does and the difference is what blocking cost.
PAGELOAD_IMAGES = 40
PAGELOAD_LOADS = 10

# Loads kept in reserve for each case and mode. On CI's runner the engine cancels an image now and
# then as its answer arrives, with blocking on and off alike, and a page missing an image is not
# the page the other mode loaded. Such a load is not counted and a spare takes its place; the spares
# are in the plan from the start because the DNS zone is written before the browser runs.
PAGELOAD_SPARES = 6

# How many hosts the forty images come from. The worst case is a page that reaches every host for
# the first time, so every request waits on its lookups; the common case is a page whose few hosts
# the profile has resolved in the last minute.
PAGELOAD_CASES = {"fresh": 40, "known": 4}

# Every name is under `.test`, which HTTPS-only mode treats as a local development host and leaves
# on plain HTTP. A name anywhere else would be upgraded to HTTPS and fail against the plain server.
#
# The pages sit on one site and the images on another, because a tracker is a third party. The two
# page hosts are the per-site switch's two positions: blocking is switched off for the second.
PAGELOAD_ON_HOST = "on.pageload.test"
PAGELOAD_OFF_HOST = "off.pageload.test"
PAGELOAD_IMAGE_DOMAIN = "pageload-cdn.test"
PAGELOAD_EDGE_HOST = f"edge.{PAGELOAD_IMAGE_DOMAIN}"

# The rules compile after the browser is up, and a load measured before they are in force measures
# a browser with nothing to check. One user rule refuses this host, so an image from it failing to
# load while one from its neighbour loads is the rules arriving.
PAGELOAD_PROBE_HOST = f"refused.{PAGELOAD_IMAGE_DOMAIN}"
PAGELOAD_CONTROL_HOST = f"control.{PAGELOAD_IMAGE_DOMAIN}"

# Between one load reporting and the next starting, so the page going away is not in the timing of
# the page arriving.
PAGELOAD_SETTLE_MILLISECONDS = 250

# One load of forty images from loopback takes a fraction of a second; one that has not reported
# in this long is a page that never finished.
PAGELOAD_TIMEOUT = 30.0

# A one-pixel PNG. What an image costs to decode is not what this measures.
PIXEL = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII=")

# What `pageload` runs on top of Python, and what each is for.
PAGELOAD_TOOLS = {
    "dnsmasq": "the DNS server the test names resolve through",
    "ip": "bringing up loopback in the private network",
    "mount": "binding the resolver files over the machine's",
    "unshare": "running the browser as its own user rather than as root",
}


class Unavailable(RuntimeError):
    """This machine cannot take the measurement, which is not a failed one.

    A desktop with no Wayland display, no built browser or no key synthesiser has nothing to say
    about the budget, and a developer running the suite on one should not be told their change
    broke something.
    """


class MeasurementFailed(RuntimeError):
    """The browser did not do what the measurement needed, which is a failed run.

    A launch that never mapped a window, Spaces that never opened, an allocator page that never
    allocated: each is either a broken browser or a number that would mean nothing, and both are
    worth a red gate. Keeping this apart from `Unavailable` is the difference between a budget that
    catches a regression and one that reports `skipped` and exits zero.
    """


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
            raise Unavailable(
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
    """One process's proportional set size, or nothing for a process that has gone.

    The engine starts and ends processes while this reads them, and one that ended between the
    scan and the read holds nothing. One that is still there and cannot be read is different: it
    holds memory this would silently leave out of the total, so it is worth the run.
    """
    try:
        with open(f"/proc/{pid}/smaps_rollup", encoding="utf-8") as handle:
            for line in handle:
                if line.startswith("Pss:"):
                    return int(line.split()[1])
    except (OSError, IndexError, ValueError) as error:
        if os.path.exists(f"/proc/{pid}"):
            raise MeasurementFailed(f"process {pid} is running and its memory cannot be read: "
                                    f"{error}") from error
    return 0


def children_by_parent() -> dict[int, list[int]]:
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
    tree = children_by_parent()
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

    def start(self, url: str, keybindings: str | None = None,
              launcher: tuple[str, ...] = ()) -> None:
        environment = dict(os.environ)
        environment["OMAWEB_DATA_ROOT"] = os.path.join(self.root, "data")
        environment["OMAWEB_CONFIG_ROOT"] = os.path.join(self.root, "config")
        environment["WAYLAND_DEBUG"] = "1"
        if keybindings:
            environment["OMAWEB_KEYBINDINGS_FILE"] = keybindings
        self.sink = open(self.log_path, "w", encoding="utf-8")
        self.started_at = time.time()
        self.process = subprocess.Popen(
            [*launcher, "dbus-run-session", "--", self.executable, url],
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
                raise MeasurementFailed("the browser under test exited before it mapped a window")
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
        raise MeasurementFailed("the browser under test never mapped a window")

    def await_title(self, fragment: str) -> None:
        """Waits for the browser to name the page in its window title.

        A mapped window is not yet a browser that answers a command: the shell is still loading and
        the page is still arriving, and a key sent into that gap lands somewhere harmless and
        silently. The title the engine hands the compositor is the first thing that says the page
        is up, and it is in the same protocol log the mapping is, so it needs no compositor either.
        """
        deadline = time.time() + READY_TIMEOUT
        wanted = f'set_title("{fragment}'
        position = 0
        while time.time() < deadline:
            assert self.process is not None
            if self.process.poll() is not None:
                raise MeasurementFailed("the browser under test exited before it loaded a page")
            with open(self.log_path, encoding="utf-8", errors="replace") as handle:
                handle.seek(position)
                if any(wanted in line for line in handle):
                    return
                position = handle.tell()
            time.sleep(0.1)
        raise MeasurementFailed(f"the browser never showed a page titled {fragment!r}")

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
        tree = children_by_parent()
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
        raise MeasurementFailed("the browser's own process is not under the launcher this started")

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
        raise MeasurementFailed(f"the browser would not open Space {expected}")
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
        return {"memory_mebibytes": browser.memory_mib()}
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
        raise MeasurementFailed(f"asked for {count} Spaces and the browser made {opened}")
    return {"space_mebibytes": (last - first) / max(count - 1, 1)}


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
        raise MeasurementFailed(
            f"this needs two Spaces to put one away, and the browser made {opened}")
    if grew <= 0:
        raise MeasurementFailed("the away Space's page never grew while it was on show, so its "
                                "not growing afterwards says nothing about Freezing")
    return {"frozen_growth_mebibytes": growth}


@dataclasses.dataclass(frozen=True)
class PageLoad:
    """One page the browser is sent to, and whether its timing counts."""

    number: int
    case: str
    mode: str
    measured: bool
    images: tuple[str, ...]
    spare: bool = False

    def address(self, port: int) -> str:
        host = PAGELOAD_ON_HOST if self.mode == "on" else PAGELOAD_OFF_HOST
        return f"http://{host}:{port}/page/{self.number}"


def host_of(address: str) -> str:
    return urllib.parse.urlsplit(address).hostname or ""


def with_port(address: str, port: int) -> str:
    parts = urllib.parse.urlsplit(address)
    return parts._replace(netloc=f"{parts.hostname}:{port}").geturl()


def pageload_plan(loads: int = PAGELOAD_LOADS) -> list[PageLoad]:
    """Every page the run loads, in the order it loads them.

    Each case opens with one load in each mode that is not counted: the first load of a case is the
    one that finds the renderer cold and, in the common case, its four hosts unresolved, which is
    the state the common case is defined not to be in. After that the modes alternate, so whatever
    the machine drifts by over a run falls on both.

    Every image address is new, so the HTTP cache has nothing to answer with. In the worst case the
    hosts are new too, so every load pays for its lookups.
    """
    plan: list[PageLoad] = []

    def add(case: str, hosts: int, mode: str, measured: bool, spare: bool) -> None:
        number = len(plan)
        images = []
        for image in range(PAGELOAD_IMAGES):
            host = f"img{image}-load{number}" if case == "fresh" else f"img{image % hosts}"
            images.append(f"http://{host}.{PAGELOAD_IMAGE_DOMAIN}/image/{image}.png"
                          f"?load={number}")
        plan.append(PageLoad(number, case, mode, measured, tuple(images), spare))

    for case, hosts in PAGELOAD_CASES.items():
        for mode, measured in [("on", False), ("off", False)] + [("on", True),
                                                                 ("off", True)] * loads:
            add(case, hosts, mode, measured, spare=False)
    for case, hosts in PAGELOAD_CASES.items():
        for mode in ("on", "off"):
            for _ in range(PAGELOAD_SPARES):
                add(case, hosts, mode, measured=True, spare=True)
    return plan


def pageload_zone(plan: list[PageLoad]) -> str:
    """The DNS server's configuration: every name the run asks for, and nothing else.

    Each image host is an alias of a name that is itself an alias of the one holding the address,
    which is the shape of a tracker behind a CDN of its own. A lookup therefore has a chain to
    return, and CNAME uncloaking a chain to check, which a host-resolver rule would not give it:
    a mapped host is an IP literal and makes no lookup at all.

    Anything else under `.test` is answered as not existing rather than forwarded, and there is
    nowhere to forward to: the run's network has loopback and nothing more.
    """
    lines = ["port=53", "listen-address=127.0.0.1", "bind-interfaces", "no-resolv", "no-hosts",
             "no-poll", "local=/test/"]
    for host in (PAGELOAD_ON_HOST, PAGELOAD_OFF_HOST, PAGELOAD_PROBE_HOST, PAGELOAD_CONTROL_HOST,
                 PAGELOAD_EDGE_HOST):
        lines.append(f"host-record={host},127.0.0.1")
    aliased: set[str] = set()
    for load in plan:
        for image in load.images:
            host = host_of(image)
            if host in aliased:
                continue
            aliased.add(host)
            cloak = f"{host.split('.')[0]}.cloak.{PAGELOAD_IMAGE_DOMAIN}"
            lines.append(f"cname={host},{cloak}")
            lines.append(f"cname={cloak},{PAGELOAD_EDGE_HOST}")
    return "\n".join(lines) + "\n"


@dataclasses.dataclass(frozen=True)
class PageLoadSummary:
    on: float
    off: float

    @property
    def added(self) -> float:
        return self.on - self.off


def summarise_pageload(on: list[float], off: list[float]) -> PageLoadSummary:
    """The two medians, whose difference is what blocking added to the page.

    A difference of medians rather than a median of differences, because the loads are alternated
    rather than paired: a pair is two loads that happened to be neighbours, not the same load twice.
    """
    return PageLoadSummary(statistics.median(on), statistics.median(off))


# The page reports its own timing, because the engine's navigation entry is the one clock that
# starts when the load does. `loadEventStart` is when every image had arrived or failed, measured
# from the navigation starting, and the images that arrived are counted so that a load a rule cut
# short cannot pass as a fast one. The server answers each report with the next page to go to.
PAGELOAD_PAGE = """<!doctype html>
<meta charset="utf-8">
<title>Omaweb page-load budget {number}</title>
<script>
  const failed = new Set();
  addEventListener("error", event => failed.add(event.target.src), true);
</script>
{images}
<script>
  addEventListener("load", () => setTimeout(async () => {{
    const entry = performance.getEntriesByType("navigation")[0];
    const missing = [...document.images].filter(image => image.naturalWidth === 0);
    const report = {{
      number: {number},
      milliseconds: entry.loadEventStart,
      missing: missing.map(image => image.src),
      failed: missing.filter(image => failed.has(image.src)).length,
    }};
    const sent = await fetch("/report", {{ method: "POST", body: JSON.stringify(report) }});
    const answer = await sent.json();
    if (answer.next) setTimeout(() => location.replace(answer.next), {settle});
  }}));
</script>
"""

# The page the browser opens on, which waits for the rules to be in force before the run starts.
PAGELOAD_READY_PAGE = """<!doctype html>
<meta charset="utf-8">
<title>Omaweb page-load budget</title>
<script>
  const load = address => new Promise(resolve => {{
    const image = new Image();
    image.onload = () => resolve(true);
    image.onerror = () => resolve(false);
    image.src = address;
  }});
  async function attempt(number) {{
    const [refused, control] = await Promise.all([
      load("http://{probe}:{port}/probe.png?attempt=" + number),
      load("http://{control}:{port}/probe.png?attempt=" + number),
    ]);
    if (control && refused) {{
      setTimeout(() => attempt(number + 1), {settle});
      return;
    }}
    const answer = await (await fetch("/report", {{
      method: "POST", body: JSON.stringify({{ ready: control }}),
    }})).json();
    if (answer.next) location.replace(answer.next);
  }}
  attempt(0);
</script>
"""


class PageLoadServer(http.server.ThreadingHTTPServer):
    # The standard library's backlog is five. Forty hosts connect at once, the kernel drops the
    # connections past the backlog, and each retries a second later, which a run first measured
    # as a second of blocking cost.
    request_queue_size = 128
    daemon_threads = True

    def __init__(self, address: tuple[str, int], handler) -> None:
        super().__init__(address, handler)
        self.errors: dict[str, int] = {}

    def handle_error(self, request, client_address) -> None:
        """Counts what went wrong answering a connection, rather than printing it.

        A page going away resets its connections, which is not a failure, and a run prints dozens
        of them. The count is reported when a page is missing images and not otherwise.
        """
        name = type(sys.exc_info()[1]).__name__
        self.errors[name] = self.errors.get(name, 0) + 1


class PageLoadSite:
    """The pages, the images and the reports, served from loopback to the browser under test.

    One server answers every host, because every name in the zone resolves to loopback. Nothing it
    serves may be cached: an image the cache answers is a request blocking was never asked about.
    """

    def __init__(self, plan: list[PageLoad]) -> None:
        self.plan = plan
        self.reports: dict[int, dict] = {}
        # Every address the server was asked for, so that an image a page is missing can be told
        # apart as one that never arrived here and one that did and went missing on the way back.
        self.requested: set[str] = set()
        self.answered: set[str] = set()
        self.ready = False
        self.problem = ""
        self.finished = False
        # The order the loads run in, which a repeat inserts a spare into, and where the run is.
        self.sequence = [load.number for load in plan if not load.spare]
        self.position = -1
        self.spares = [load for load in plan if load.spare]
        self.repeated: list[int] = []
        self.progress = threading.Condition()
        self.server = PageLoadServer(("127.0.0.1", 0), self._handler())
        self.port = self.server.server_address[1]
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    @property
    def start_address(self) -> str:
        return f"http://{PAGELOAD_ON_HOST}:{self.port}/ready"

    def start(self) -> None:
        self.thread.start()

    def stop(self) -> None:
        self.server.shutdown()
        self.server.server_close()

    def page(self, number: int) -> bytes:
        load = self.plan[number]
        # CORS mode rather than no-cors. On CI's runner the engine cancelled no-cors image
        # requests as their answers arrived, some forty a run in both modes, and CORS-mode
        # requests about three: the net log shows the response started and then the request
        # cancelled, with no network error. What is left is repeated with a spare.
        images = "\n".join(
            f'<img src="{with_port(image, self.port)}" crossorigin="anonymous" width="16" '
            'height="16" alt="">'
            for image in load.images)
        return PAGELOAD_PAGE.format(number=number, images=images,
                                    settle=PAGELOAD_SETTLE_MILLISECONDS).encode()

    def ready_page(self) -> bytes:
        return PAGELOAD_READY_PAGE.format(probe=PAGELOAD_PROBE_HOST, control=PAGELOAD_CONTROL_HOST,
                                          port=self.port,
                                          settle=PAGELOAD_SETTLE_MILLISECONDS).encode()

    def receive(self, report: dict) -> str | None:
        """Takes one report and answers with where the page goes next, if anywhere."""
        with self.progress:
            if "ready" in report:
                if not report["ready"]:
                    self.problem = ("an image no rule refuses did not load, so the pages cannot "
                                    "reach the server the zone names")
                    self.progress.notify_all()
                    return None
                self.ready = True
                following = 0
            else:
                number = int(report["number"])
                self.reports[number] = report
                load = self.plan[number]
                if report["missing"] and load.measured:
                    self.repeated.append(number)
                    spare = next((candidate for candidate in self.spares
                                  if (candidate.case, candidate.mode) == (load.case, load.mode)),
                                 None)
                    if spare is None:
                        self.problem = (f"the engine cut short more {load.case}-host loads with "
                                        f"blocking {load.mode} than there are spares for:\n    "
                                        + "\n    ".join(self.describe_missing(self.plan[short])
                                                         for short in self.repeated))
                        self.progress.notify_all()
                        return None
                    self.spares.remove(spare)
                    self.sequence.insert(self.position + 1, spare.number)
            self.position += 1
            if self.position >= len(self.sequence):
                self.finished = True
            self.progress.notify_all()
        if self.finished:
            return None
        return self.plan[self.sequence[self.position]].address(self.port)

    def counted(self, case: str, mode: str) -> list[PageLoad]:
        """The loads of one case and mode whose timings count, in the order they ran."""
        return [self.plan[number] for number in self.sequence
                if number not in self.repeated and self.plan[number].measured
                and (self.plan[number].case, self.plan[number].mode) == (case, mode)]

    def wait(self) -> None:
        """Waits for every load to report, and fails on one that goes quiet."""
        with self.progress:
            deadline = time.time() + READY_TIMEOUT
            while not self.ready and not self.problem:
                if not self.progress.wait(max(deadline - time.time(), 0)):
                    raise MeasurementFailed("Content blocking's rules never came into force")
            heard = len(self.reports)
            while not self.finished and not self.problem:
                if not self.progress.wait(PAGELOAD_TIMEOUT) and len(self.reports) == heard:
                    raise MeasurementFailed(
                        f"load {heard + 1} of {len(self.sequence)} never reported its timing")
                heard = len(self.reports)
            if self.problem:
                raise MeasurementFailed(self.problem)

    def describe_missing(self, load: PageLoad) -> str:
        """One page's missing images, and whether the server ever saw them asked for.

        Not asked for is a refusal, a lookup or a connection that failed before the request left the
        browser; asked for and still missing is an answer that did not arrive.
        """
        report = self.reports[load.number]
        missing = report["missing"]
        asked = sum(address in self.requested for address in missing)
        answered = sum(address in self.answered for address in missing)
        return (f"load {load.number} ({load.case} hosts, blocking {load.mode}, "
                f"{'counted' if load.measured else 'warm-up'}) is missing {len(missing)} of "
                f"{PAGELOAD_IMAGES} images: {asked} asked of the server, {answered} answered "
                f"by it, {report['failed']} failed in the page; the first is {missing[0]}")

    def _handler(self) -> type[http.server.BaseHTTPRequestHandler]:
        site = self

        class Handler(http.server.BaseHTTPRequestHandler):
            # Keep-alive, so the common case's four hosts reuse their connections as a real
            # page's would.
            protocol_version = "HTTP/1.1"

            def log_message(self, format: str, *arguments) -> None:  # noqa: A002
                pass

            def answer(self, body: bytes, content_type: str) -> None:
                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(body)
                self.wfile.flush()
                site.answered.add(self.address)

            @property
            def address(self) -> str:
                return f"http://{self.headers.get('Host', '')}{self.path}"

            def do_GET(self) -> None:  # noqa: N802
                site.requested.add(self.address)
                path = urllib.parse.urlsplit(self.path).path
                if path == "/ready":
                    self.answer(site.ready_page(), "text/html; charset=utf-8")
                elif path.startswith("/page/"):
                    self.answer(site.page(int(path.rsplit("/", 1)[1])), "text/html; charset=utf-8")
                elif path.endswith(".png"):
                    self.answer(PIXEL, "image/png")
                else:
                    self.send_error(404)

            def do_POST(self) -> None:  # noqa: N802
                length = int(self.headers.get("Content-Length", "0"))
                report = json.loads(self.rfile.read(length) or b"{}")
                self.answer(json.dumps({"next": site.receive(report)}).encode(),
                            "application/json")

        return Handler


def seed_content_blocking(data_root: str) -> None:
    """Content blocking as a first run leaves it, from the committed lists rather than the network.

    The lists are marked as fetched a moment ago, so the browser does not go looking for newer ones
    on a network that has nothing on it. The off page's host is the one the per-site switch has
    turned off, which is the comparison a reader makes and the one ADR 0050 made.
    """
    blocking = os.path.join(data_root, "content-blocking")
    os.makedirs(os.path.join(blocking, "lists"))
    now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    subscriptions = []
    for list_id, title in (("easylist", "EasyList"), ("easyprivacy", "EasyPrivacy")):
        shutil.copyfile(FILTER_LISTS / f"{list_id}.txt",
                        os.path.join(blocking, "lists", f"{list_id}.txt"))
        subscriptions.append({
            "id": list_id,
            "title": title,
            "source": "https://easylist.to/",
            "license": "GPLv3 or CC BY-SA 3.0",
            "updateAddress": f"https://easylist.to/easylist/{list_id}.txt",
            "updateStatus": "current",
            "lastUpdated": now,
            "enabled": True,
        })
    settings = {
        "version": 1,
        "seeded": True,
        "userRules": f"||{PAGELOAD_PROBE_HOST}^",
        "disabledSites": [PAGELOAD_OFF_HOST],
        "subscriptions": subscriptions,
    }
    with open(os.path.join(blocking, "settings.json"), "w", encoding="utf-8") as handle:
        json.dump(settings, handle)


class PrivateNetwork:
    """A network with loopback on it and a DNS server that knows the run's names, and no other.

    Entered by the process that runs the measurement, which is a child of this script's so that
    the rest of the run keeps the machine's network. The namespace is a user namespace's, so no
    privilege is needed, and the machine's `resolv.conf` and `nsswitch.conf` are covered rather
    than changed: a bind mount in a private mount namespace is gone when the last process in it
    is. `nsswitch.conf` is covered too because Arch's asks systemd-resolved first, over a socket
    that reaches the machine's own resolver from any network.

    The browser is not run as the namespace's root, which Chromium's sandbox refuses, but in a
    user namespace nested inside it that maps the reader's own user back.
    """

    def __init__(self, root: str, zone: str) -> None:
        self.root = root
        self.zone = zone
        self.user = os.getuid()
        self.group = os.getgid()
        self.dnsmasq: subprocess.Popen | None = None

    @property
    def launcher(self) -> tuple[str, ...]:
        return ("unshare", "--user", f"--map-user={self.user}", f"--map-group={self.group}", "--")

    def enter(self) -> None:
        try:
            os.unshare(os.CLONE_NEWUSER | os.CLONE_NEWNET | os.CLONE_NEWNS)
        except (AttributeError, OSError) as error:
            raise Unavailable(f"this machine would not make a private network: {error}") from error
        for name, content in (("setgroups", "deny"), ("uid_map", f"0 {self.user} 1"),
                              ("gid_map", f"0 {self.group} 1")):
            with open(f"/proc/self/{name}", "w", encoding="utf-8") as handle:
                handle.write(content)
        self._run("ip", "link", "set", "lo", "up")
        for name, content in (("resolv.conf", "nameserver 127.0.0.1\n"),
                              ("nsswitch.conf", "hosts: files dns\n")):
            path = os.path.join(self.root, name)
            with open(path, "w", encoding="utf-8") as handle:
                handle.write(content)
            self._run("mount", "--bind", path, f"/etc/{name}")
        configuration = os.path.join(self.root, "dnsmasq.conf")
        with open(configuration, "w", encoding="utf-8") as handle:
            handle.write(self.zone)
        # Root here is the reader's own user outside, and it has no other user or group to drop
        # to, so dnsmasq is told to stay as it is.
        self.dnsmasq = subprocess.Popen(
            ["dnsmasq", f"--conf-file={configuration}", "--keep-in-foreground", "--user=root",
             "--group=", "--pid-file="],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        deadline = time.time() + 10
        while time.time() < deadline:
            if self.dnsmasq.poll() is not None:
                raise MeasurementFailed(f"dnsmasq would not start: {self.dnsmasq.stderr.read()}")
            try:
                socket.getaddrinfo(PAGELOAD_EDGE_HOST, 80, socket.AF_INET)
                return
            except socket.gaierror:
                time.sleep(0.05)
        raise MeasurementFailed("dnsmasq started and never answered")

    def leave(self) -> None:
        if self.dnsmasq and self.dnsmasq.poll() is None:
            self.dnsmasq.terminate()
            self.dnsmasq.wait(timeout=10)

    @staticmethod
    def _run(*command: str) -> None:
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode:
            raise Unavailable(f"`{' '.join(command)}` failed in the private network: "
                              f"{result.stderr.strip()}")


def in_child(work) -> dict:
    """Runs `work` in a child process and hands back what it returned or raised.

    A namespace is entered by a process and not left, and the measurements after this one are the
    machine's, so the one that needs a network of its own is run in a process that ends with it.
    """
    sys.stdout.flush()
    reader, writer = os.pipe()
    child = os.fork()
    if child == 0:
        os.close(reader)
        try:
            outcome = {"result": work()}
        except Unavailable as error:
            outcome = {"unavailable": str(error)}
        except MeasurementFailed as error:
            outcome = {"failed": str(error)}
        except BaseException as error:  # noqa: BLE001 - the parent reports whatever it was
            outcome = {"failed": f"{type(error).__name__}: {error}"}
        with os.fdopen(writer, "w", encoding="utf-8") as handle:
            json.dump(outcome, handle)
        sys.stdout.flush()
        os._exit(0)
    os.close(writer)
    with os.fdopen(reader, encoding="utf-8") as handle:
        answer = handle.read()
    os.waitpid(child, 0)
    outcome = json.loads(answer or '{"failed": "the measurement ended without an answer"}')
    if "unavailable" in outcome:
        raise Unavailable(outcome["unavailable"])
    if "failed" in outcome:
        raise MeasurementFailed(outcome["failed"])
    return outcome["result"]


def run_pageload(executable: str, private: bool) -> dict:
    plan = pageload_plan()
    workspace = Workspace()
    network = PrivateNetwork(workspace.root, pageload_zone(plan)) if private else None
    site: PageLoadSite | None = None
    browser = workspace.browser(executable)
    try:
        if network:
            network.enter()
        seed_content_blocking(os.path.join(workspace.root, "data"))
        # After entering, so that its socket is on the private network's loopback.
        site = PageLoadSite(plan)
        site.start()
        browser.start(site.start_address, launcher=network.launcher if network else ())
        site.wait()
    finally:
        browser.stop()
        if site:
            site.stop()
        if network:
            network.leave()
        workspace.discard()

    if site.repeated:
        log(f"  repeated {len(site.repeated)} of {len(site.sequence) - len(site.repeated)} loads "
            "the engine cut short, which were not counted:")
        for number in site.repeated:
            log(f"    {site.describe_missing(plan[number])}")
    results = {}
    for case, hosts in PAGELOAD_CASES.items():
        timings = {mode: [float(site.reports[load.number]["milliseconds"])
                          for load in site.counted(case, mode)] for mode in ("on", "off")}
        summary = summarise_pageload(timings["on"], timings["off"])
        log(f"  {PAGELOAD_IMAGES} images from {hosts} {case} hosts: {summary.on:.1f} ms on, "
            f"{summary.off:.1f} ms off, {summary.added:.1f} ms added "
            f"(medians of {PAGELOAD_LOADS} loads each)")
        results[f"pageload_{case}_hosts_milliseconds"] = summary.added
    return results


def machine_serves_zone() -> bool:
    """Whether the machine's resolver already answers with the zone `--print-dns-zone` writes.

    That is how CI runs it. Its container will not make a user namespace, so a private network is
    out of reach, and the job, which is root there, serves the zone from `dnsmasq` and points the
    container's `resolv.conf` at it before the budget runs.
    """
    try:
        answers = socket.getaddrinfo(PAGELOAD_EDGE_HOST, 80, socket.AF_INET)
    except socket.gaierror:
        return False
    return any(answer[4][0] == "127.0.0.1" for answer in answers)


def measure_pageload(executable: str, require_dns: bool) -> dict:
    """What Content blocking as a whole adds to a page load, in the worst case and the common one.

    That is the rule check, CNAME uncloaking where the engine carries it, the Refusal tally and the
    refused-request list: everything that runs for a request with blocking on and not with the
    site switched off. Only the default DNS path is measured. Secure DNS on sends lookups to a
    public server, whose speed from a CI runner is not Omaweb's to hold to a number.
    """
    if machine_serves_zone():
        log("  the machine's own resolver answers the run's names, so it measures there")
        return run_pageload(executable, private=False)
    try:
        missing = [f"{tool}, for {purpose}" for tool, purpose in PAGELOAD_TOOLS.items()
                   if shutil.which(tool) is None]
        if missing:
            raise Unavailable(f"this needs {'; '.join(missing)}")
        return in_child(lambda: run_pageload(executable, private=True))
    except Unavailable as error:
        if require_dns:
            raise MeasurementFailed(f"{error}, and --require-dns says it may not skip") from error
        raise


MEASUREMENTS = {
    "startup": lambda arguments: measure_startup(arguments.browser, arguments.repetitions),
    "memory": lambda arguments: measure_memory(arguments.browser),
    "spaces": lambda arguments: measure_spaces(arguments.browser, arguments.spaces),
    "freezing": lambda arguments: measure_freezing(arguments.browser),
    "pageload": lambda arguments: measure_pageload(arguments.browser, arguments.require_dns),
}


def report(results: dict, budget: dict) -> int:
    """Prints every measurement beside its threshold, and answers how many crossed it.

    Passing numbers are printed too. A budget that only speaks when it is broken hides the drift
    that is about to break it.
    """
    thresholds = budget["measurements"]
    crossed = 0
    units = {"startup_seconds": "s", "pageload_fresh_hosts_milliseconds": "ms",
             "pageload_known_hosts_milliseconds": "ms"}
    log("")
    taken = budget["recorded_on"]
    log(f"ceilings recorded on: {budget['machine']}, {taken}" if taken else "ceilings: not yet")
    log("")
    for name, value in results.items():
        ceiling = thresholds[name]["ceiling"]
        unit = units.get(name, "MiB")
        over = value > ceiling
        crossed += int(over)
        log(f"{'CROSSED' if over else 'within '}  {name}: {value:.2f} {unit} "
            f"against {ceiling:.2f} {unit} ({ceiling - value:.2f} {unit} of headroom)")
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
    parser.add_argument("--require-dns", action="store_true",
                        help="fail pageload rather than skip it where its DNS server cannot run")
    parser.add_argument("--print-dns-zone", action="store_true",
                        help="print the dnsmasq configuration pageload resolves through, and exit")

    parser.add_argument("--record", action="store_true",
                        help="write the measurements into the budget as its recorded numbers")
    arguments = parser.parse_args()

    unknown = [name for name in arguments.measurement if name not in MEASUREMENTS]
    if unknown:
        parser.error(f"no such measurement: {', '.join(unknown)}")
    if arguments.repetitions < 1:
        parser.error("a median needs at least one launch")
    if arguments.spaces < 2:
        parser.error("a per-Space cost needs at least two Spaces")

    if arguments.print_dns_zone:
        sys.stdout.write(pageload_zone(pageload_plan()))
        return 0
    if not os.access(arguments.browser, os.X_OK):
        log(f"skipped: {arguments.browser} is not built")
        return 0
    if not os.environ.get("WAYLAND_DISPLAY"):
        log("skipped: this measures a browser on a Wayland display, and there is none")
        return 0

    budget = load_budget()
    results: dict[str, float] = {}
    failure = ""
    for name in arguments.measurement or list(MEASUREMENTS):
        log(f"{name}:")
        try:
            results.update(MEASUREMENTS[name](arguments))
        except Unavailable as error:
            # One measurement this machine cannot take says nothing about the others: a desktop
            # with no DNS server still has a startup time.
            log(f"skipped: {error}")
            continue
        except MeasurementFailed as error:
            # The measurements already taken are still worth printing: a run that fell over on the
            # fourth one has three numbers in it, and the report is where the drift shows.
            log(f"FAILED  {name}: {error}")
            failure = name
            break

    crossed = report(results, budget)
    if arguments.record and not failure:
        record(results, budget)
    return 1 if crossed or failure else 0


if __name__ == "__main__":
    sys.exit(main())
