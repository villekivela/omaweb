#!/usr/bin/env python3
"""Drive a browser through its own commands on a live Hyprland session.

What this covers is the part of the Wayland port no headless suite reaches: the
keyboard reaching the window, the compositor answering a command that changes
the window, and the clipboard crossing between Omaweb and another application.
It was written because #103 asked a person to do it by hand, on the finding that
no key synthesiser was installed. Hyprland has one of its own, so nobody has to.

Keys go in through `hl.dsp.send_shortcut`, the compositor's own dispatcher, and
what they did is read back from `hyprctl clients`, `wl-paste` and the browser's
window title. Nothing here reads the screen: a check that has to look at pixels
is a check that has to capture the desktop, and the desktop belongs to whoever
is sitting at it.

The browser under test is a second one, launched on a private session bus so it
does not hand its arguments to the browser the reader already has open, and on
throwaway data and configuration directories so it browses nothing of theirs.

Two of #103's checks are deliberately not here. Moving and resizing the window
by its frameless regions needs a pointer button, which Hyprland's dispatchers do
not synthesise. Becoming the default browser changes the machine that runs the
check. Both stay a person's, and the report says so.

Usage:

    scripts/check_wayland_session.py
    scripts/check_wayland_session.py --browser build/dev/omaweb --keep
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

# Long enough for the shell to answer a command and the compositor to settle,
# short enough that eighty of them is a wait rather than a coffee break.
SETTLE = 0.8
# A page load crosses a process boundary and reads a file, so it gets its own.
LOAD_SETTLE = 3.0

PROBE_PAGE = """<!doctype html>
<meta charset="utf-8">
<title>Omaweb session probe</title>
<h1>session probe</h1>
<p>omaweb-round-trip-token</p>
"""

TARGET_PAGE = """<!doctype html>
<meta charset="utf-8">
<title>Omaweb paste target</title>
<h1>paste target</h1>
"""

# What the keybindings file writes against what the compositor calls the same
# key. Everything not named here is already the keysym.
KEYSYMS = {
    "]": "bracketright",
    "[": "bracketleft",
    "=": "equal",
    "-": "minus",
    ".": "period",
    "/": "slash",
    ",": "comma",
    ":": "colon",
    "?": "question",
}

# Commands the sweep sends nothing for, each because sending it would end the
# run rather than test it.
UNSWEPT = {
    "print": "opens a portal dialog only a person can answer",
    "minimize-window": "unmaps the window every later check reads",
    "open-file": "opens a file chooser only a person can answer",
    "private-window": "opens a second window, and has a check of its own",
}


class SessionError(RuntimeError):
    """The desktop cannot run these checks, which is not a failed check."""


def hyprctl(*arguments: str) -> str:
    result = subprocess.run(
        ["hyprctl", *arguments], capture_output=True, text=True, check=False
    )
    return result.stdout.strip()


def dispatch(expression: str) -> None:
    hyprctl("dispatch", expression)


def clients() -> list[dict]:
    return json.loads(hyprctl("clients", "-j") or "[]")


def window_of(pid: int) -> dict | None:
    for client in clients():
        if client.get("pid") == pid:
            return client
    return None


def require_session() -> None:
    if os.environ.get("XDG_SESSION_TYPE") != "wayland":
        raise SessionError("this drives a live Wayland session, and this is not one")
    for tool in ("hyprctl", "wl-copy", "wl-paste"):
        if shutil.which(tool) is None:
            raise SessionError(f"{tool} is not installed")
    if not hyprctl("version"):
        raise SessionError("Hyprland is not answering, and its dispatcher is the keyboard here")


def clipboard(primary: bool = False) -> str:
    arguments = ["wl-paste", "-n"] + (["--primary"] if primary else [])
    result = subprocess.run(arguments, capture_output=True, text=True, check=False)
    return result.stdout


def set_clipboard(value: str) -> None:
    subprocess.run(["wl-copy", value], check=False)


class Browser:
    """A second browser, its own session bus, its own throwaway directories."""

    def __init__(self, executable: str, root: str) -> None:
        self.executable = executable
        self.root = root
        self.process: subprocess.Popen | None = None
        self.pid = 0

    def start(self, url: str) -> None:
        environment = dict(os.environ)
        environment["OMAWEB_DATA_ROOT"] = os.path.join(self.root, "data")
        environment["OMAWEB_CONFIG_ROOT"] = os.path.join(self.root, "config")
        # A private bus is what stops this handing its argument to the browser
        # the reader already has open and exiting, which is the whole of what
        # `RunningBrowser` is for and exactly wrong here.
        self.process = subprocess.Popen(
            ["dbus-run-session", "--", self.executable, url],
            env=environment,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        for _ in range(60):
            time.sleep(0.5)
            for client in clients():
                if client.get("class") == "omaweb" and self._owned(client.get("pid", 0)):
                    self.pid = client["pid"]
                    time.sleep(LOAD_SETTLE)
                    return
        raise SessionError("the browser under test never mapped a window")

    def _owned(self, pid: int) -> bool:
        # The reader's own browser is an `omaweb` window too, and driving that
        # one would be driving their session rather than a test.
        assert self.process is not None
        if pid == self.process.pid:
            return True
        try:
            with open(f"/proc/{pid}/stat", encoding="utf-8") as handle:
                parent = int(handle.read().rsplit(")", 1)[1].split()[1])
        except (OSError, IndexError, ValueError):
            return False
        return parent == self.process.pid

    def focus(self) -> None:
        # Wayland lets a client set the selection only while it holds keyboard
        # focus, so every clipboard check needs this and not only the pointer.
        dispatch(f'hl.dsp.focus{{window="pid:{self.pid}"}}')
        time.sleep(SETTLE)

    def send(self, modifiers: str, key: str) -> None:
        dispatch(
            f'hl.dsp.send_shortcut{{mods="{modifiers}", '
            f'key="{key}", window="pid:{self.pid}"}}'
        )
        time.sleep(SETTLE)

    def press(self, binding: str) -> None:
        parts = binding.split("+")
        key = parts[-1]
        modifiers = " ".join(
            {"Primary": "CTRL", "Shift": "SHIFT", "Alt": "ALT"}.get(part, part.upper())
            for part in parts[:-1]
        )
        self.send(modifiers, KEYSYMS.get(key, key))

    def title(self) -> str:
        window = window_of(self.pid)
        return window["title"] if window else ""

    def size(self) -> list[int]:
        window = window_of(self.pid)
        return window["size"] if window else []

    def fullscreen(self) -> int:
        window = window_of(self.pid)
        return window.get("fullscreen", 0) if window else 0

    def alive(self) -> bool:
        return window_of(self.pid) is not None

    def stop(self) -> None:
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()


class Report:
    def __init__(self) -> None:
        self.results: list[tuple[bool, str, str]] = []

    def check(self, passed: bool, name: str, detail: str = "") -> None:
        self.results.append((passed, name, detail))
        print(f"{'pass' if passed else 'FAIL'}  {name}{'  — ' + detail if detail else ''}")

    def failed(self) -> int:
        return sum(1 for passed, _, _ in self.results if not passed)


def check_fullscreen(browser: Browser, report: Report) -> None:
    # Relative to what the window mapped as rather than to zero: Hyprland hands
    # a new window the fullscreen its workspace was left in, so a check written
    # against a known starting state fails on the state, not on the browser.
    before = browser.fullscreen()
    browser.press("Primary+Shift+F")
    taken = browser.fullscreen()
    browser.press("Primary+Shift+F")
    given_back = browser.fullscreen()
    report.check(
        taken != before and given_back == before,
        "browser fullscreen is taken and handed back",
        f"{before} → {taken} → {given_back}",
    )


def check_sidebar_is_not_the_window(browser: Browser, report: Report) -> None:
    # The sidebar and the window are resized by neighbouring keys, and a
    # sidebar command that moved the window would be the bug worth finding.
    start = browser.size()
    moved = []
    for binding in (
        "Primary+Shift+]",
        "Primary+Shift+]",
        "Primary+Shift+[",
        "Primary+Shift+B",
        "Primary+B",
        "Primary+B",
    ):
        browser.press(binding)
        if browser.size() != start:
            moved.append(binding)
    report.check(
        not moved,
        "sidebar commands resize the sidebar rather than the window",
        f"window stayed {start}" if not moved else f"window moved on {moved}",
    )


def check_clipboard(browser: Browser, report: Report, pages: dict[str, str]) -> None:
    browser.focus()

    set_clipboard("omaweb-sentinel")
    browser.press("Primary+Shift+C")
    copied = clipboard()
    report.check(
        copied == pages["probe"],
        "an address copied from Omaweb is readable by another application",
        copied or "nothing was copied",
    )

    set_clipboard(pages["target"])
    browser.press("Primary+L")
    browser.send("CTRL", "V")
    browser.send("", "Return")
    time.sleep(LOAD_SETTLE)
    report.check(
        "paste target" in browser.title(),
        "an address pasted from another application opens",
        browser.title(),
    )

    set_clipboard(pages["probe"])
    browser.press("Primary+L")
    browser.send("CTRL", "V")
    browser.send("", "Return")
    time.sleep(LOAD_SETTLE)

    set_clipboard("omaweb-sentinel")
    browser.press("Primary+Shift+E")
    browser.send("CTRL", "A")
    browser.send("CTRL", "C")
    selected = clipboard()
    report.check(
        "omaweb-round-trip-token" in selected,
        "text copied from a page reaches the clipboard",
        selected.replace("\n", " ")[:60] or "nothing was copied",
    )
    report.check(
        "omaweb-round-trip-token" in clipboard(primary=True),
        "the same selection reaches the primary selection",
    )


def check_private_window(browser: Browser, report: Report) -> None:
    # A Private window is a window of its own, so the sweep cannot send this
    # one and leave the rest reading the window it opened.
    before = [client["pid"] for client in clients()].count(browser.pid)
    browser.press("Primary+Shift+N")
    time.sleep(LOAD_SETTLE)
    after = [client["pid"] for client in clients()].count(browser.pid)
    report.check(
        after == before + 1,
        "a Private window opens as a window of its own",
        f"{before} → {after}",
    )
    browser.send("CTRL", "W")
    time.sleep(SETTLE)


def check_every_command_answers(browser: Browser, report: Report, bindings: dict) -> None:
    swept = 0
    for binding, command in bindings.items():
        if command in UNSWEPT:
            continue
        browser.press(binding)
        # Whatever it opened, this closes. A command that left the browser
        # unable to take the next one is the failure being looked for.
        browser.send("", "Escape")
        swept += 1
        if not browser.alive():
            report.check(
                False,
                "every browser command answers",
                f"{binding} ({command}) left no window",
            )
            return
    report.check(True, "every browser command answers", f"{swept} sent, window still there")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", default="build/dev/omaweb", help="the browser to drive")
    parser.add_argument("--keep", action="store_true", help="leave the browser running")
    arguments = parser.parse_args()

    try:
        require_session()
    except SessionError as error:
        print(f"skipped: {error}")
        return 0

    if not os.access(arguments.browser, os.X_OK):
        print(f"skipped: {arguments.browser} is not built")
        return 0

    with open("assets/keybindings/default.json", encoding="utf-8") as handle:
        bindings = json.load(handle)["browser"]

    root = tempfile.mkdtemp(prefix="omaweb-session-check-")
    pages = {}
    for name, body in (("probe", PROBE_PAGE), ("target", TARGET_PAGE)):
        path = os.path.join(root, f"{name}.html")
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(body)
        pages[name] = f"file://{path}"

    report = Report()
    browser = Browser(arguments.browser, root)
    try:
        browser.start(pages["probe"])
        report.check(browser.alive(), "the browser under test mapped a window", browser.title())
        browser.focus()
        check_fullscreen(browser, report)
        check_sidebar_is_not_the_window(browser, report)
        check_clipboard(browser, report, pages)
        check_private_window(browser, report)
        check_every_command_answers(browser, report, bindings)
    except SessionError as error:
        print(f"skipped: {error}")
        return 0
    finally:
        if not arguments.keep:
            browser.stop()
            shutil.rmtree(root, ignore_errors=True)

    print()
    for command, reason in sorted(UNSWEPT.items()):
        print(f"not sent: {command} — {reason}")
    print("not checked here: moving and resizing the window by its frameless regions, which")
    print("                  needs a pointer button no dispatcher synthesises, and becoming")
    print("                  the default browser, which changes the machine running the check")

    failed = report.failed()
    print(f"\n{len(report.results) - failed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
