#!/usr/bin/env python3
"""Measure Omaweb's startup, memory and page loads against the budget in `performance/budget.json`.

What the build costs is held to a number by `scripts/benchmark_build.sh`, and what one selector
match costs by `tests/benchmarks/`. How long the browser takes to appear, and what it costs to keep
Spaces open, were held to nothing, so this measures them and fails when a recorded ceiling is
crossed.

Six measurements, each its own subcommand so a developer can run the one they are working on:

- `startup` launches the browser and times process start to the window mapping.
- `memory` reads the resident memory of the process tree with one Space and one page.
- `spaces` opens Spaces one at a time and reports what each adds, which is the price of the engine
  profile per Space that ADR 0008 buys.
- `freezing` puts away a Space whose page allocates on a timer and reports how much it went on
  taking, which is the claim ADR 0033 makes and nothing checked.
- `pageload` loads the same pages with Content blocking on and with the site switched off, and
  reports what blocking added to each, which is the cost ADR 0050 measured once by hand. From the
  same loads it reports how soon the fresh-host and known-host pages first painted, blocking on.
- `livetabs` opens five Spaces, then twenty tabs and then fifty across them, each from a local
  site of its own, and reports what each tab past a Space's first added at each count, and how much
  of a core the tree uses over thirty seconds left alone.

This writes nothing outside the throwaway directories it launches its own browser on, `--record`
aside, which writes the measurements into the budget in this repository and appends them to
`performance/history.jsonl`. It launches that browser on a private session bus, so unlike the
theme and default-browser checks it needs no opt-in guard: it puts nothing back because it put
nothing anywhere. It does take the keyboard focus while it runs.
`pageload` runs its browser in a network namespace of its own, with its own resolver files bound
over the machine's, so the DNS server it starts answers that browser and nothing else. `livetabs`
serves its pages from the loopback addresses it binds, for as long as it runs.

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
    scripts/benchmark_runtime.py livetabs
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
import sqlite3
import statistics
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse
from pathlib import Path

import performance_history as history

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

# The reading the away window is measured from waits for the engine's processes to stop moving,
# because a Space switch leaves them busy for a moment: the Space now on show settling its page and
# its tabs' stored favicons being looked up. Three readings in a row, each within the tolerance of
# the one before, are still; one agreeing pair can be a pause in work that starts again. A page
# still running while away is throttled to one timer tick a second, so the allocator page then
# takes a megabyte a second, eight times the tolerance per interval, and never settles. The limit
# ends the wait for such a page, and the away window then reads it as growth: ten mebibytes, twice
# the ceiling, and still far from its 500 MiB cap when the window closes.
STEADY_INTERVAL = 2.0
STEADY_READINGS = 3
STEADY_TOLERANCE = 0.25
STEADY_LIMIT = 30.0

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

# A line of `WAYLAND_DEBUG` output, which opens on a timestamp: milliseconds in older libwayland,
# the time of day in newer. Chromium's own lines open on a bracketed process id and a colon.
WAYLAND_LINE = re.compile(r"\[\s*(\d+|\d\d:\d\d:\d\d)\.\d+\]")

# The page ADR 0050 measured: forty images, which no rule refuses, so a load with blocking on
# fetches everything the load with it off does and the difference is what blocking cost.
PAGELOAD_IMAGES = 40
PAGELOAD_LOADS = 10

# Loads kept in reserve for each case and mode. A page missing an image is not the page the other
# mode loaded, so such a load is not counted and a spare takes its place; the spares are in the plan
# from the start because the DNS zone is written before the browser runs. No cause of a short load
# is known since /dev/shm was given room for the response bodies (#600). Two are kept so that one
# stray short load on a shared runner does not fail the run, and each one used is listed, while a
# cause that comes back runs them out within a few loads rather than hiding behind them.
PAGELOAD_SPARES = 2

# How many hosts the forty images come from. The worst case is a page that reaches every host for
# the first time, so every request waits on its lookups; the common case is a page whose few hosts
# the profile has resolved in the last minute. The procedural case is the common one with a page
# that procedural cosmetic rules are written against (ADR 0052), so the difference between the two
# is what the rules cost.
PAGELOAD_CASES = {"fresh": 40, "known": 4, "procedural": 4}

# The cases whose pages are also held to how soon they first paint, with blocking on. A reader's
# page is one of these two; the procedural case is there to price its rules, not to be a page.
FIRST_CONTENTFUL_PAINT_CASES = ("fresh", "known")

# One rule per operator and action the pinned parser reads, with the markup each is written against:
# the fixture the matcher and the engine are tested with. The rules are rewritten for both page
# hosts, so the page with the site switched off carries the same markup and the same rules, and only
# blocking being on runs them.
PROCEDURAL_RULES = ROOT / "tests" / "content-blocking" / "procedural-rules.json"


# How many live tabs the live-tabs step reads the memory at, and over how many Spaces. Five Spaces
# hold the fifty evenly, and a reader's tabs are spread over a few rather than kept in one.
LIVE_TAB_COUNTS = (20, 50)
LIVE_TAB_SPACES = 5
# How long a new tab is waited for in its Space's database: the browser records its tabs 400 ms
# after they change.
TAB_RECORDED_WAIT = 3.0
# How long the tree is left alone, at the last count, while its CPU is read.
LIVE_TAB_IDLE_WINDOW = 30.0
# How many of the processes that used CPU in that window the log names.
LIVE_TAB_IDLE_NAMED = 8


def procedural_fixture() -> tuple[list[str], str]:
    """The procedural rules for the page hosts, and the markup they are written against.

    Every row of the fixture names its elements `match` and `miss`, and some carry a style for
    `#match`. On one page those would all reach each other: the `::after` content one row gives its
    `#match` landed on the blocks two other rows hide, and the page with blocking off came out 70 ms
    slower than with it on, which measured the fixture rather than the rules. So each row's names
    are its own. No rule names an element by its id, so the rules are unchanged.
    """
    rows = json.loads(PROCEDURAL_RULES.read_text(encoding="utf-8"))
    sites = f"{PAGELOAD_ON_HOST},{PAGELOAD_OFF_HOST}"
    rules = [sites + row["rule"][row["rule"].index("#"):] for row in rows]
    markup = "\n".join(
        "<section>" + re.sub(r'(id="|#)(match|miss)\b',
                             lambda found, row=index: f"{found[1]}{found[2]}-{row}",
                             row["page"]) + "</section>"
        for index, row in enumerate(rows))
    return rules, markup

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

# How often a page that has not gone on to the next one tells the server how far it got. A load
# that reports goes on in well under a second, so only one that went quiet ever says it.
PAGELOAD_STALL_MILLISECONDS = 5000

# How long a page waits after its `load` event for the first contentful paint to reach its
# timeline. The entry is added when the frame is presented, which on a slow runner can be after the
# load event; a page that has not painted by this long reports none and fails the run.
PAGELOAD_PAINT_MILLISECONDS = 5000

# How much of the browser's own output a failed step prints: enough to reach back past the page
# before the one that failed, which is a title and a few messages.
BROWSER_MESSAGES = 40

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

# The least /dev/shm a run may have. The engine writes each response body into a data pipe of
# 2 MiB there, and the worst case's forty images can all be in flight at once: a run with room to
# spare peaked at 74 MiB. Docker gives a container 64 MiB, and there the engine cancelled the last
# images of a load as their answers arrived, because no pipe could be made for them (#600). A
# desktop's /dev/shm is half its memory.
PAGELOAD_SHARED_MEMORY_MEBIBYTES = 256


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
        # By its status, not its output: with no Hyprland to reach, as under a cage started from a
        # Hyprland session, it says so on stdout, and every key would go to a dispatcher that is
        # not there.
        self.hyprland = shutil.which("hyprctl") is not None and subprocess.run(
            ["hyprctl", "version"], capture_output=True, text=True, check=False).returncode == 0
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

    def enter_address(self, binding: str, address: str) -> None:
        """Opens the Omnibar with `binding`, types `address` and goes there."""
        self.press(binding)
        self.write(address)
        self.press("Return")

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


def process_states(root: int) -> dict[int, tuple[str, str, float, int]]:
    """Each process in a tree: its name, its state, the CPU seconds it and the children it reaped
    have used, and its RSS in KiB."""
    tree = children_by_parent()
    ticks = os.sysconf("SC_CLK_TCK")
    page_kib = os.sysconf("SC_PAGE_SIZE") // 1024
    states = {}
    pending = [root]
    while pending:
        pid = pending.pop()
        pending += tree.get(pid, [])
        try:
            with open(f"/proc/{pid}/stat", encoding="utf-8") as handle:
                stat = handle.read()
        except OSError:
            continue
        states[pid] = parse_stat(stat, ticks, page_kib)
    return states


def parse_stat(stat: str, ticks: int, page_kib: int) -> tuple[str, str, float, int]:
    """One line of `/proc/<pid>/stat`: the name, the state, the CPU seconds and the RSS in KiB.

    The CPU is the process's own and that of the children it has reaped, so a child that ended
    between two readings is still in its parent's.
    """
    name = stat[stat.index("(") + 1:stat.rindex(")")]
    fields = stat.rsplit(")", 1)[1].split()
    cpu = sum(int(field) for field in fields[11:15]) / ticks
    return (name, fields[0], cpu, int(fields[21]) * page_kib)


def describe_processes(root: int) -> list[str]:
    """What each process in a tree is doing: its state, its RSS, and the CPU it used over a second.

    A second apart, because what a process has used since it started does not say whether it is
    using any now.
    """
    before = process_states(root)
    time.sleep(1)
    after = process_states(root)
    return [f"{pid} {name} {state} {rss / 1024:.0f} MiB, "
            f"{cpu - before.get(pid, (name, state, cpu, rss))[2]:.2f} CPU s in the last second"
            for pid, (name, state, cpu, rss) in sorted(after.items())]


@dataclasses.dataclass(frozen=True)
class ProcessCpu:
    """The CPU seconds one process used over a window."""

    pid: int
    name: str
    seconds: float


@dataclasses.dataclass(frozen=True)
class IdleCpu:
    """What a tree used over a window: its share of one core, who used it, and who went.

    `busiest` is each process that used any, busiest first, a parent's seconds holding those of
    the children it reaped. `ended` names the processes that were gone at the end of the window:
    their parent's reading holds what they used, so the share counts it, and the log says they
    went.
    """

    percent: float
    busiest: list[ProcessCpu]
    ended: list[str]


def idle_cpu(before: dict[int, tuple[str, str, float, int]],
             after: dict[int, tuple[str, str, float, int]], seconds: float) -> IdleCpu:
    """The CPU between two `process_states` readings, a process that started between them counted
    from nothing.

    A process that ended was reaped by its parent in the tree, whose reading then holds all it ever
    used, so what it had used by the first reading is taken off the total.
    """
    used = []
    for pid, (name, _, cpu, _) in after.items():
        previous = before.get(pid)
        spent = cpu - previous[2] if previous and previous[0] == name else cpu
        if spent > 0:
            used.append(ProcessCpu(pid, name, spent))
    used.sort(key=lambda process: process.seconds, reverse=True)
    gone = {pid: state for pid, state in before.items() if pid not in after}
    ended = [f"{pid} {name}" for pid, (name, *_) in sorted(gone.items())]
    spent = sum(process.seconds for process in used) - sum(cpu for _, _, cpu, _ in gone.values())
    return IdleCpu(100.0 * spent / seconds, used, ended)


@dataclasses.dataclass(frozen=True)
class SettledReading:
    """A reading taken once the tree stopped moving, or at the limit if it never did.

    The first reading is kept beside it, so what the tree did during the wait can be told apart
    from how long the wait took: a Freezing that starts late looks like a tree still growing.
    """

    first: float
    mebibytes: float
    seconds: float
    settled: bool


def settled_reading(read, sleep=time.sleep, clock=time.monotonic) -> SettledReading:
    """Reads until `STEADY_READINGS` in a row agree within the tolerance, or the limit passes.

    Agreement is on the size, not the direction: a tree that shrank is still moving, because what
    shrank it is the browser still at work.
    """
    start = clock()
    first = previous = read()
    agreeing = 1
    while True:
        sleep(STEADY_INTERVAL)
        current = read()
        waited = clock() - start
        agreeing = agreeing + 1 if abs(current - previous) <= STEADY_TOLERANCE else 1
        if agreeing >= STEADY_READINGS:
            return SettledReading(first, current, waited, True)
        if waited >= STEADY_LIMIT:
            return SettledReading(first, current, waited, False)
        previous = current


def tree_mib(root: int) -> float:
    """Proportional set size of a process and everything below it, in mebibytes."""
    return read_pss_kib(root) / KIB_PER_MIB + below_mib(root)


def below_mib(root: int) -> float:
    """Proportional set size of everything below a process, without the process, in mebibytes."""
    tree = children_by_parent()
    total = 0
    pending = list(tree.get(root, []))
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
            [*launcher, "dbus-run-session", "--", self.executable, *([url] if url else [])],
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

    def messages(self) -> list[str]:
        """The titles the browser gave its window and what else it wrote to stderr, in order.

        The rest of the log is the Wayland protocol, which says nothing about a page.
        """
        with open(self.log_path, encoding="utf-8", errors="replace") as handle:
            return [line.rstrip() for line in handle
                    if ".set_title(" in line or not WAYLAND_LINE.match(line.lstrip())]

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
        # `omaweb` is the small client, which becomes the browser under the browser's own name.
        names = {os.path.basename(self.executable)[:15], "omaweb-browser"}
        tree = children_by_parent()
        pending = list(tree.get(self.pid, []))
        while pending:
            candidate = pending.pop(0)
            try:
                with open(f"/proc/{candidate}/comm", encoding="utf-8") as handle:
                    if handle.read().strip() in names:
                        return candidate
            except OSError:
                pass
            pending += tree.get(candidate, [])
        raise MeasurementFailed("the browser's own process is not under the launcher this started")

    def memory_mib(self) -> float:
        return tree_mib(self.pid)

    def engine_mib(self) -> float:
        """What the engine's processes hold, the browser's own process left out.

        A page runs in a renderer, so this is where a page still running shows. The browser process
        is where the shell's own work lands, and on CI it takes a step of about 6 MiB at no moment
        a measurement can wait out, which is more than the whole freezing budget.
        """
        return below_mib(self.window_pid)

    def stop(self) -> None:
        """Ends the whole process group.

        Terminating the launcher alone leaves the engine's zygote and renderers holding their
        memory, which on a machine with a few gigabytes is enough for the next launch to be killed
        by the kernel rather than by us.
        """
        if self.process:
            # The launcher leads a session of its own, so its process id is the group's, and that
            # holds after the launcher has gone. The launcher going says nothing about the rest of
            # the group: whatever outlives SIGTERM is killed rather than left running into the next
            # measurement, where a page-load run after one that went quiet found its rules never
            # came into force.
            group = self.process.pid
            for stage in (signal.SIGTERM, signal.SIGKILL):
                try:
                    os.killpg(group, stage)
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

    def tab_count(self) -> int:
        """How many tabs the browser has actually made, read from each Space's own database.

        A tab whose keys went elsewhere is a page loaded into a tab that already existed, and a
        title says nothing about which tab it is in. Opened read-only, because the browser is still
        writing.
        """
        spaces = os.path.join(self.root, "data", "spaces")
        count = 0
        for entry in os.scandir(spaces) if os.path.isdir(spaces) else ():
            path = os.path.join(entry.path, "browser.sqlite")
            if not os.path.exists(path):
                continue
            database = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
            try:
                count += database.execute("SELECT COUNT(*) FROM tabs").fetchone()[0]
            finally:
                database.close()
        return count

    def discard(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)


def log_messages(browser: Browser) -> None:
    """Prints the last of what the browser said, for a step that failed waiting on it."""
    log("  the browser's titles and messages:")
    for line in browser.messages()[-BROWSER_MESSAGES:]:
        log(f"    {line}")


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
    keyboard.enter_address("Primary+L", url or workspace.page_url)
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

    So the page in the away Space allocates on a timer, and the engine's processes are read twice
    while that Space is on show and twice more once it is away. The browser's own process is left
    out: a page runs in a renderer, and the browser process takes steps of its own that have
    nothing to do with any page. The first pair is the control: a
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
        running = browser.engine_mib()
        time.sleep(ON_SHOW_WINDOW)
        grew = browser.engine_mib() - running
        log(f"  the away Space's page grew {grew:.1f} MiB in {ON_SHOW_WINDOW:.0f} s on show")
        keyboard.press("Primary+1")
        frozen = settled_reading(browser.engine_mib)
        since_switch = SETTLE + frozen.seconds
        moved = frozen.mebibytes - frozen.first
        state = "settled" if frozen.settled else "were still moving"
        log(f"  the engine's processes {state} {since_switch:.1f} s after the switch, "
            f"having moved {moved:+.1f} MiB while it was waited for")
        time.sleep(AWAY_WINDOW)
        growth = browser.engine_mib() - frozen.mebibytes
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
#
# Its first contentful paint is on the same clock. The entry reaches the timeline when the frame is
# presented, which can be after the load event, so the page watches for it rather than reading the
# timeline once, and reports `null` if it has not come by the time it gives up.
PAGELOAD_PAGE = """<!doctype html>
<meta charset="utf-8">
<title>Omaweb page-load budget {number}</title>
<script>
  const painted = new Promise(resolve => new PerformanceObserver(list => {{
    const entry = list.getEntriesByName("first-contentful-paint")[0];
    if (entry) resolve(entry.startTime);
  }}).observe({{ type: "paint", buffered: true }}));
  const failed = new Set();
  addEventListener("error", event => failed.add(event.target.src), true);
  let reporting = "not yet";
  // A page still here after this long has gone quiet, and says how far it got.
  setInterval(() => fetch("/stalled", {{ method: "POST", body: JSON.stringify({{
    number: {number},
    readyState: document.readyState,
    loadEventStart: performance.getEntriesByType("navigation")[0]?.loadEventStart ?? null,
    incomplete: [...document.images].filter(image => !image.complete).map(image => image.src),
    reporting,
  }}) }}), {stall});
</script>
{markup}
{images}
<script>
  addEventListener("load", () => setTimeout(async () => {{
    const entry = performance.getEntriesByType("navigation")[0];
    const missing = [...document.images].filter(image => image.naturalWidth === 0);
    reporting = "waiting for its first contentful paint";
    const firstContentfulPaint = await Promise.race([
      painted, new Promise(resolve => setTimeout(() => resolve(null), {paint})),
    ]);
    const report = {{
      number: {number},
      milliseconds: entry.loadEventStart,
      firstContentfulPaint,
      missing: missing.map(image => image.src),
      failed: missing.filter(image => failed.has(image.src)).length,
    }};
    reporting = "sent";
    const sent = await fetch("/report", {{ method: "POST", body: JSON.stringify(report) }});
    reporting = "answered";
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
        # What a page that went quiet last said of itself.
        self.stalled: dict[int, dict] = {}
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
        # CORS mode rather than no-cors, which the ceilings were recorded with. On CI's runner the
        # engine cancelled no-cors image requests as their answers arrived, some forty a run in
        # both modes, and CORS-mode requests about three. The CORS-mode ones were /dev/shm too
        # small for their response bodies (#600); no-cors has not been measured since.
        images = "\n".join(
            f'<img src="{with_port(image, self.port)}" crossorigin="anonymous" width="16" '
            'height="16" alt="">'
            for image in load.images)
        markup = procedural_fixture()[1] if load.case == "procedural" else ""
        return PAGELOAD_PAGE.format(number=number, images=images, markup=markup,
                                    settle=PAGELOAD_SETTLE_MILLISECONDS,
                                    stall=PAGELOAD_STALL_MILLISECONDS,
                                    paint=PAGELOAD_PAINT_MILLISECONDS).encode()

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
                        f"load {heard + 1} of {len(self.sequence)} never reported its timing: "
                        + self.describe_quiet(self.plan[self.sequence[self.position]]))
                heard = len(self.reports)
            if self.problem:
                raise MeasurementFailed(self.problem)

    def describe_quiet(self, load: PageLoad) -> str:
        """How far a page that never reported got, as far as the server can tell."""
        if load.address(self.port) not in self.answered:
            return f"load {load.number} was never served"
        images = [with_port(image, self.port) for image in load.images]
        asked = sum(address in self.requested for address in images)
        answered = sum(address in self.answered for address in images)
        errors = ", ".join(f"{count} {name}" for name, count in sorted(self.server.errors.items()))
        stalled = self.stalled.get(load.number)
        said = ("it never said how far it got" if stalled is None else
                f"it last said: document {stalled['readyState']}, load event at "
                f"{stalled['loadEventStart']} ms, {len(stalled['incomplete'])} images incomplete "
                f"({', '.join(stalled['incomplete'][:3]) or 'none'}), "
                f"report {stalled['reporting']}")
        return (f"load {load.number} ({load.case} hosts, blocking {load.mode}) was served, and "
                f"{asked} of its {PAGELOAD_IMAGES} images were asked of the server and {answered} "
                f"answered by it; the server's errors: {errors or 'none'}; {said}")

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
                if self.path == "/stalled":
                    site.stalled[int(report["number"])] = report
                    self.answer(b"{}", "application/json")
                    return
                self.answer(json.dumps({"next": site.receive(report)}).encode(),
                            "application/json")

        return Handler


def seed_content_blocking(data_root: str, user_rules: list[str] | None = None,
                          disabled_sites: list[str] | None = None) -> None:
    """Content blocking as a first run leaves it, from the committed lists rather than the network.

    The lists are marked as fetched a moment ago, so the browser does not go looking for newer ones
    on a network that has nothing on it. By default the rules are the page-load run's and the off
    page's host is the one the per-site switch has turned off, which is the comparison a reader
    makes and the one ADR 0050 made.
    """
    if user_rules is None:
        user_rules = [f"||{PAGELOAD_PROBE_HOST}^", *procedural_fixture()[0]]
    if disabled_sites is None:
        disabled_sites = [PAGELOAD_OFF_HOST]
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
        "userRules": "\n".join(user_rules),
        "disabledSites": disabled_sites,
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
        try:
            site.wait()
        except MeasurementFailed:
            log("  the browser's processes when the load went quiet:")
            for line in describe_processes(browser.pid):
                log(f"    {line}")
            browser.stop()
            log_messages(browser)
            raise
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
    return pageload_results(site)


def pageload_results(site: PageLoadSite) -> dict:
    """What blocking added to each case's page, and how soon the fresh and known pages painted."""
    results = {}
    for case, hosts in PAGELOAD_CASES.items():
        timings = {mode: [float(site.reports[load.number]["milliseconds"])
                          for load in site.counted(case, mode)] for mode in ("on", "off")}
        summary = summarise_pageload(timings["on"], timings["off"])
        log(f"  {PAGELOAD_IMAGES} images from {hosts} {case} hosts: {summary.on:.1f} ms on, "
            f"{summary.off:.1f} ms off, {summary.added:.1f} ms added "
            f"(medians of {PAGELOAD_LOADS} loads each)")
        results[f"pageload_{case}_hosts_milliseconds"] = summary.added
    for case in FIRST_CONTENTFUL_PAINT_CASES:
        paints = []
        for load in site.counted(case, "on"):
            paint = site.reports[load.number].get("firstContentfulPaint")
            if paint is None:
                raise MeasurementFailed(
                    f"load {load.number} ({case} hosts, blocking on) has no first contentful "
                    f"paint in its timeline after {PAGELOAD_PAINT_MILLISECONDS} ms")
            paints.append(float(paint))
        median = statistics.median(paints)
        log(f"  first contentful paint, {PAGELOAD_CASES[case]} {case} hosts, blocking on: "
            f"{median:.1f} ms (median of {len(paints)} loads, {min(paints):.1f} to "
            f"{max(paints):.1f} ms)")
        results[f"first_contentful_paint_{case}_hosts_milliseconds"] = median
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


def check_shared_memory() -> None:
    """Fails a run whose /dev/shm is too small for the page's response bodies to be in flight.

    A load short of images there is the machine's limit rather than the engine's behaviour, and the
    spares would hide it until they ran out. A machine with no /dev/shm keeps its shared memory
    elsewhere and is measured.
    """
    try:
        filesystem = os.statvfs("/dev/shm")
    except OSError:
        return
    mebibytes = filesystem.f_blocks * filesystem.f_frsize // (1024 * 1024)
    if mebibytes < PAGELOAD_SHARED_MEMORY_MEBIBYTES:
        raise MeasurementFailed(
            f"/dev/shm is {mebibytes} MiB, and the page's response bodies need "
            f"{PAGELOAD_SHARED_MEMORY_MEBIBYTES} MiB of it: give the container more with "
            "--shm-size")


def measure_pageload(executable: str, require_dns: bool) -> dict:
    """What Content blocking as a whole adds to a page load, in the worst case and the common one.

    That is the rule check, CNAME uncloaking where the engine carries it, the Refusal tally and the
    refused-request list: everything that runs for a request with blocking on and not with the
    site switched off. Only the default DNS path is measured. Secure DNS on sends lookups to a
    public server, whose speed from a CI runner is not Omaweb's to hold to a number.
    """
    if machine_serves_zone():
        log("  the machine's own resolver answers the run's names, so it measures there")
        check_shared_memory()
        return run_pageload(executable, private=False)
    try:
        missing = [f"{tool}, for {purpose}" for tool, purpose in PAGELOAD_TOOLS.items()
                   if shutil.which(tool) is None]
        if missing:
            raise Unavailable(f"this needs {'; '.join(missing)}")
        check_shared_memory()
        return in_child(lambda: run_pageload(executable, private=True))
    except Unavailable as error:
        if require_dns:
            raise MeasurementFailed(f"{error}, and --require-dns says it may not skip") from error
        raise


@dataclasses.dataclass(frozen=True)
class LiveTab:
    """One tab the live-tabs step opens, and the Space it opens in."""

    number: int
    space: int


def live_tab_plan() -> list[list[LiveTab]]:
    """The tabs to open before each reading, each Space's together and in Space order.

    The first stage is each Space's first tab, so the reading the tabs are measured against is a
    browser with every Space it has at twenty and fifty. Every Space then holds an equal share at
    every count, so what is read at twenty is the same browser as at fifty with fewer tabs in each
    Space, not one with more Spaces.
    """
    stages = []
    number = 0
    held = 0
    for count in (LIVE_TAB_SPACES, *LIVE_TAB_COUNTS):
        stage = []
        for space in range(1, LIVE_TAB_SPACES + 1):
            for _ in range(held, count // LIVE_TAB_SPACES):
                number += 1
                stage.append(LiveTab(number, space))
        held = count // LIVE_TAB_SPACES
        stages.append(stage)
    return stages


def live_tab_title(number: int) -> str:
    """A tab's title, ended so that no tab's title begins another's: tab 1 is not tab 12."""
    return f"Omaweb live tab {number}: field notes"


def live_tab_space(space: int) -> str:
    """The name of the Space numbered `space`: a fresh profile's first, and those the step made."""
    return "Personal" if space == 1 else f"space{space}"


def live_tab_shown(tab: LiveTab) -> str:
    """The start of the window's title once `tab` is on show: its page's, then its Space's."""
    return f"{live_tab_title(tab.number)} — {live_tab_space(tab.space)} — "


# What a reader's tab mostly is: an article with a stylesheet, a picture, a table, a form, and a
# script that builds part of the page once. Nothing on it runs after it has loaded, because the
# idle reading is the browser's and a page still working would put its own CPU there.
LIVE_TAB_PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>@TITLE@</title>
<link rel="stylesheet" href="/style.css">
</head><body>
<header><nav><a href="/">Home</a> <a href="/">Archive</a> <a href="/">About</a></nav></header>
<main><article>
<h1>@TITLE@</h1>
<p class="byline">Notes kept on page @NUMBER@, for a browser with many tabs open.</p>
<img src="/picture.svg" alt="" width="640" height="240">
@PARAGRAPHS@
<table><thead><tr><th>Entry</th><th>Reading</th><th>Remark</th></tr></thead>
<tbody>@ROWS@</tbody></table>
<ul id="index"></ul>
<form><label>Comment <textarea name="comment" rows="3"></textarea></label>
<label>Name <input name="name"></label> <button type="button">Send</button></form>
</article></main>
<footer>Page @NUMBER@.</footer>
<script>
const sections = Array.from(document.querySelectorAll("article p"), (paragraph, index) =>
  ({ id: `part-${index + 1}`, words: paragraph.textContent.split(/\\s+/).length }));
const index = document.getElementById("index");
for (const section of sections) {
  const item = document.createElement("li");
  item.textContent = `${section.id}: ${section.words} words`;
  index.append(item);
}
</script>
</body></html>
"""

LIVE_TAB_STYLE = b"""\
body { margin: 0; font: 16px/1.6 sans-serif; color: #222; background: #fafaf7; }
header, footer { padding: 0.5rem 1rem; background: #e8e6df; }
nav { display: flex; gap: 1rem; }
main { display: grid; grid-template-columns: minmax(0, 42rem); justify-content: center; }
article { padding: 1rem; }
img { max-width: 100%; height: auto; border-radius: 4px; }
table { width: 100%; border-collapse: collapse; margin: 1rem 0; }
th, td { padding: 0.25rem 0.5rem; border-bottom: 1px solid #ccc; text-align: left; }
form { display: grid; gap: 0.5rem; }
"""

LIVE_TAB_PICTURE = b"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 640 240">
<rect width="640" height="240" fill="#cfd8dc"/>
<circle cx="120" cy="120" r="80" fill="#90a4ae"/>
<path d="M0 200 L200 120 L360 180 L520 90 L640 160 L640 240 L0 240 Z" fill="#607d8b"/>
</svg>
"""

LIVE_TAB_WORDS = ("browser tab space page memory process renderer engine site reader window "
                  "frame paint layout script style image table form note field").split()


def live_tab_page(number: int) -> bytes:
    """Tab `number`'s page, whose text differs from every other tab's, as distinct sites' do."""
    generator = random.Random(number)

    def sentence() -> str:
        words = [generator.choice(LIVE_TAB_WORDS) for _ in range(generator.randint(8, 16))]
        return " ".join(words).capitalize() + "."

    paragraphs = "\n".join(f"<p>{' '.join(sentence() for _ in range(5))}</p>" for _ in range(12))
    rows = "".join(f"<tr><td>{row}</td><td>{generator.randint(1, 999)}</td>"
                   f"<td>{sentence()}</td></tr>" for row in range(1, 31))
    page = (LIVE_TAB_PAGE.replace("@TITLE@", live_tab_title(number))
            .replace("@NUMBER@", str(number))
            .replace("@PARAGRAPHS@", paragraphs)
            .replace("@ROWS@", rows))
    return page.encode()


def live_tab_results(first: float, totals: dict[int, float], idle_percent: float) -> dict:
    """What each tab past its Space's first added at each count, and the tree's idle CPU.

    `first` is the tree with every Space open and one tab in each. Read against a browser with
    fewer Spaces, a tab would carry a share of what a Space costs, and dividing the whole tree by
    the tab count would spread the browser's own cost over the tabs.
    """
    results = {f"live_tab_at_{count}_mebibytes": (total - first) / (count - LIVE_TAB_SPACES)
               for count, total in totals.items()}
    results["live_tabs_idle_cpu_percent"] = idle_percent
    return results


class LiveTabSite:
    """The live-tabs step's pages, each tab's from a server of its own on a loopback address.

    The engine gives each site its own renderer, and a site is a host, the port left out, so tabs
    served from one address would share processes that a reader's tabs on different sites do not.
    Linux routes all of 127.0.0.0/8 to the loopback interface, so every tab gets its own address
    there and its pages need no network and no name.
    """

    def __init__(self, tabs: int) -> None:
        self.servers = [PageLoadServer((f"127.0.0.{number + 1}", 0), self._handler(number))
                        for number in range(1, tabs + 1)]

    def address(self, number: int) -> str:
        """What is typed for tab `number`: the Omnibar sends an address literal over plain HTTP."""
        host, port = self.servers[number - 1].server_address[:2]
        return f"{host}:{port}/"

    def start(self) -> None:
        for server in self.servers:
            threading.Thread(target=server.serve_forever, daemon=True).start()

    def stop(self) -> None:
        # Together, because each waits out its own half-second poll, and fifty in turn is half a
        # minute.
        stopping = [threading.Thread(target=server.shutdown) for server in self.servers]
        for thread in stopping:
            thread.start()
        for thread in stopping:
            thread.join()
        for server in self.servers:
            server.server_close()

    @staticmethod
    def _handler(number: int) -> type[http.server.BaseHTTPRequestHandler]:
        resources = {
            "/": (live_tab_page(number), "text/html; charset=utf-8"),
            "/style.css": (LIVE_TAB_STYLE, "text/css"),
            "/picture.svg": (LIVE_TAB_PICTURE, "image/svg+xml"),
        }

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, format: str, *arguments) -> None:  # noqa: A002
                pass

            def do_GET(self) -> None:  # noqa: N802
                resource = resources.get(urllib.parse.urlsplit(self.path).path)
                if resource is None:
                    self.send_error(404)
                    return
                body, content_type = resource
                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        return Handler


def open_live_tab(keyboard: Keyboard, workspace: Workspace, browser: Browser, site: LiveTabSite,
                  tab: LiveTab, shown: int) -> int:
    """Opens `tab` as a reader would, waits for its page, and returns the Space now on show.

    `shown` is the Space on show before it. A Space's first tab is the empty one it opens with,
    given its address, and every other is the new-tab key and an address typed into the Omnibar,
    so each one has its own engine from the moment it is shown. The browser is launched with no
    address for the same reason: a fresh profile opens with an empty tab, and an address on the
    command line would be a second tab beside it that the new-tab key then reuses rather than
    adding one.

    A new tab is counted in its Space's database before its page is waited for. The browser makes
    the tab when the address is entered, not at the new-tab key, so the count is read then, and
    keys that went elsewhere are sent again as `open_space` sends its own.
    """
    address = site.address(tab.number)
    if workspace.space_count() < tab.space:
        open_space(keyboard, workspace, live_tab_space(tab.space), tab.space, f"http://{address}")
    elif tab.number == 1:
        keyboard.focus()
        keyboard.enter_address("Primary+L", address)
    else:
        keyboard.focus()
        if tab.space != shown:
            keyboard.press(f"Primary+{tab.space}")
        expected = workspace.tab_count() + 1
        for _ in range(OPEN_SPACE_ATTEMPTS):
            keyboard.enter_address("Primary+T", address)
            if tab_recorded(workspace, expected):
                break
            # Whatever took the keys, this closes, so the next attempt starts where the first did.
            keyboard.press("Escape")
        else:
            raise MeasurementFailed(f"the browser would not open tab {tab.number}")
    browser.await_title(live_tab_shown(tab))
    return tab.space


def tab_recorded(workspace: Workspace, expected: int) -> bool:
    """Whether the browser has recorded `expected` tabs within `TAB_RECORDED_WAIT`."""
    deadline = time.monotonic() + TAB_RECORDED_WAIT
    while workspace.tab_count() != expected:
        if time.monotonic() >= deadline:
            return False
        time.sleep(0.1)
    return True


def measure_livetabs(executable: str) -> dict:
    """What each live tab costs at twenty tabs and at fifty, and what the tree uses left alone.

    A live tab is one with its engine: a tab of a restored Space that was never shown has none, so
    each tab here is opened and shown. Every tab but the one on show is then frozen, as a reader's
    are, so the memory is what a reader's open tabs hold and the CPU is what a window of them costs
    while nobody touches it. Each tab is its own site, served from its own loopback address, so
    nothing here depends on the network and no two tabs share a renderer that two sites would not.
    """
    if not sys.platform.startswith("linux"):
        raise Unavailable("its tabs' sites are loopback addresses only Linux routes")
    stages = live_tab_plan()
    site = LiveTabSite(sum(len(stage) for stage in stages))
    workspace = Workspace()
    browser = workspace.browser(executable)
    try:
        site.start()
        browser.start("", keybindings=workspace.keybindings)
        browser.await_title("New tab")
        keyboard = Keyboard(browser.window_pid)
        shown = 1
        readings = []
        for stage, count in zip(stages, (LIVE_TAB_SPACES, *LIVE_TAB_COUNTS)):
            for tab in stage:
                try:
                    shown = open_live_tab(keyboard, workspace, browser, site, tab, shown)
                except MeasurementFailed:
                    log(f"  {workspace.tab_count()} tabs were made")
                    log_messages(browser)
                    raise
            reading = settled_reading(browser.memory_mib)
            state = "settled" if reading.settled else "was still moving"
            log(f"  {count} tabs in {workspace.space_count()} Spaces: {reading.mebibytes:.1f} MiB, "
                f"which {state} {reading.seconds:.0f} s after the last tab opened")
            made = workspace.tab_count()
            if made != count:
                raise MeasurementFailed(f"asked for {count} tabs and the browser made {made}")
            readings.append(reading.mebibytes)
        before = process_states(browser.pid)
        started = time.monotonic()
        time.sleep(LIVE_TAB_IDLE_WINDOW)
        idle = idle_cpu(before, process_states(browser.pid), time.monotonic() - started)
    finally:
        browser.stop()
        site.stop()
        workspace.discard()
    log(f"  left alone for {LIVE_TAB_IDLE_WINDOW:.0f} s, the tree used {idle.percent:.2f}% "
        "of a core")
    for process in idle.busiest[:LIVE_TAB_IDLE_NAMED]:
        log(f"    {process.pid} {process.name}: {process.seconds:.2f} CPU s")
    if idle.ended:
        log(f"    and these ended in the window, their CPU counted in their parent's: "
            f"{', '.join(idle.ended)}")
    first, *totals = readings
    return live_tab_results(first, dict(zip(LIVE_TAB_COUNTS, totals)), idle.percent)


MEASUREMENTS = {
    "startup": lambda arguments: measure_startup(arguments.browser, arguments.repetitions),
    "memory": lambda arguments: measure_memory(arguments.browser),
    "spaces": lambda arguments: measure_spaces(arguments.browser, arguments.spaces),
    "freezing": lambda arguments: measure_freezing(arguments.browser),
    "pageload": lambda arguments: measure_pageload(arguments.browser, arguments.require_dns),
    "livetabs": lambda arguments: measure_livetabs(arguments.browser),
}


def report(results: dict, budget: dict) -> int:
    """Prints every measurement beside its threshold, and answers how many crossed it.

    Passing numbers are printed too. A budget that only speaks when it is broken hides the drift
    that is about to break it.
    """
    thresholds = budget["measurements"]
    crossed = 0
    units = {"_seconds": "s", "_milliseconds": "ms", "_mebibytes": "MiB", "_percent": "%"}
    log("")
    taken = budget["recorded_on"]
    log(f"ceilings recorded on: {budget['machine']}, {taken}" if taken else "ceilings: not yet")
    log("")
    for name, value in results.items():
        ceiling = thresholds[name]["ceiling"]
        unit = next((unit for suffix, unit in units.items() if name.endswith(suffix)), "MiB")
        over = value > ceiling
        crossed += int(over)
        log(f"{'CROSSED' if over else 'within '}  {name}: {value:.2f} {unit} "
            f"against {ceiling:.2f} {unit} ({ceiling - value:.2f} {unit} of headroom)")
    log("")
    return crossed


def record(results: dict, budget: dict, executable: str, machine: str) -> None:
    """Writes the measurements back as what the budget was recorded at, and adds them to the
    history.

    The ceilings themselves are not touched. What counts as too slow is a decision, reviewed like
    any other, and a script that moved it every time a machine ran slower would be a budget that
    ratchets itself out of existence.

    The budget keeps only this recording, so the history is where a number drifting towards its
    ceiling shows. The line is written first, from the ceilings the run was held to. The machine
    is written with the measurements, because a number recorded on one machine under the name of
    another says that machine was faster or slower than it was.
    """
    version = history.read_version(executable).get("omaweb", "")
    history.append(history.budget_record(
        results, budget, date=history.now(), commit=history.omaweb_commit(executable, version),
        machine=machine, engine=history.describe_engine(executable), omaweb=version))
    for name, value in results.items():
        budget["measurements"][name]["recorded"] = round(value, 2)
    budget["machine"] = machine
    budget["recorded_on"] = datetime.date.today().isoformat()
    with open(BUDGET, "w", encoding="utf-8") as handle:
        json.dump(budget, handle, indent=2)
        handle.write("\n")
    log(f"recorded into {BUDGET.relative_to(ROOT)} and {history.HISTORY.relative_to(ROOT)}")


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
                        help="write the measurements into the budget as its recorded numbers and "
                        "append them to performance/history.jsonl")
    parser.add_argument("--machine", default="",
                        help="what the history calls this machine, described from the hardware "
                        "by default")
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
        record(results, budget, arguments.browser,
               arguments.machine or history.describe_machine())
    return 1 if crossed or failure else 0


if __name__ == "__main__":
    sys.exit(main())
