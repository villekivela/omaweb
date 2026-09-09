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

The frameless move and resize regions need a pointer, which arrives in two
halves: Hyprland's own `hl.dsp.cursor.move` puts the cursor on an exact layout
coordinate, and `ydotool` presses the button its dispatchers do not synthesise.
Those checks are skipped, not failed, where ydotool is absent.

`omaweb_session` holds the browser under test and the tools that drive it, and
says what they can and cannot judge.

Two checks live in scripts of their own, because each changes the machine it
runs on and has to be asked for rather than swept up: the theme repaint in
`check_theme_repaint.py`, and the default browser in `check_default_browser.py`.

Usage:

    scripts/check_wayland_session.py
    scripts/check_wayland_session.py --browser build/dev/omaweb --keep
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import tempfile
import time

from omaweb_session import (
    LOAD_SETTLE,
    PROBE_PAGE,
    SETTLE,
    Browser,
    Pointer,
    Report,
    SessionError,
    clients,
    clipboard,
    require_session,
    set_clipboard,
)


TARGET_PAGE = """<!doctype html>
<meta charset="utf-8">
<title>Omaweb paste target</title>
<h1>paste target</h1>
"""


# Commands the sweep sends nothing for, each because sending it would end the
# run rather than test it.
UNSWEPT = {
    "print": "opens a portal dialog only a person can answer",
    "minimize-window": "unmaps the window every later check reads",
    "open-file": "opens a file chooser only a person can answer",
    "private-window": "opens a second window, and has a check of its own",
}


def check_window_moves_by_its_region(browser: Browser, report: Report, pointer: Pointer) -> None:
    """The move region is judged by the request it sends, not by the window.

    Hyprland's interactive move follows relative pointer motion, and a cursor
    put on a coordinate is a warp with no deltas behind it, so the window sits
    still however faithfully the region fires. Its resize grab reads the
    cursor's absolute position instead, which is why the edges below can be
    judged by the size and this cannot be judged by the position. What belongs
    to Omaweb either way is the request, so that is what this reads.
    """
    # The strip is inset by the outline's own margin and has a button row
    # anchored at each end. The gap between those rows is what a reader grabs.
    at = browser.position()
    before = browser.requests("move")
    pointer.drag((at[0] + 130, at[1] + 26), (at[0] + 290, at[1] + 146))
    sent = browser.requests("move") - before
    report.check(
        sent > 0,
        "a drag on the sidebar's navigation strip asks the compositor to move the window",
        f"xdg_toplevel.move sent {sent}x" if sent else "no xdg_toplevel.move was sent",
    )


def check_window_resizes_by_its_edges(browser: Browser, report: Report, pointer: Pointer) -> None:
    # Each edge is a five-pixel strip, so the grab sits two pixels in: on the
    # edge itself the compositor's own border takes the press first.
    inset = 2
    # Every drag pushes the edge outward, from a window `reshape` has already
    # given room to grow into on all four sides.
    travel = 80
    for edge, grab, shift in (
        ("right", lambda a, s: (a[0] + s[0] - 1 - inset, a[1] + s[1] / 2), (travel, 0)),
        ("bottom", lambda a, s: (a[0] + s[0] / 2, a[1] + s[1] - 1 - inset), (0, travel)),
        ("left", lambda a, s: (a[0] + inset, a[1] + s[1] / 2), (-travel, 0)),
        ("top", lambda a, s: (a[0] + s[0] / 2, a[1] + inset), (0, -travel)),
    ):
        size = browser.settle()
        at = browser.position()
        if not at or not size:
            report.check(False, f"the {edge} edge resizes the window", "the window went away")
            return
        start = grab(at, size)
        pointer.drag(start, (start[0] + shift[0], start[1] + shift[1]))
        after = browser.settle()
        axis = 0 if shift[0] else 1
        report.check(
            bool(after) and abs(after[axis] - size[axis] - travel) <= 2,
            f"the {edge} edge resizes the window",
            f"{size} to {after}",
        )


def check_frameless_regions(browser: Browser, report: Report) -> None:
    try:
        pointer = Pointer()
    except SessionError as error:
        print(f"skipped: {error}")
        return

    # A tiled window is placed by the layout and does not follow a pointer, so
    # the drag would test Hyprland's tiling policy rather than Omaweb's region.
    tiled = not browser.floating()
    if tiled:
        browser.float_window()
    if not browser.floating():
        print("skipped: the window under test could not be floated, and a tiled one cannot move")
        return
    try:
        check_window_moves_by_its_region(browser, report, pointer)
        # Comfortably above Omaweb's own 840x560 minimum and well inside the
        # output, so all four edges have somewhere to travel.
        if browser.reshape(1200, 900):
            check_window_resizes_by_its_edges(browser, report, pointer)
        else:
            print(f"skipped: the window would not take a known size, and it is {browser.size()}")
    finally:
        if tiled and browser.alive():
            browser.float_window()


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

    # A browser of its own, because the move region is judged by the protocol
    # and WAYLAND_DEBUG over the sweep above would be a log nobody can read.
    regions = Browser(arguments.browser, root, protocol_log=os.path.join(root, "protocol.log"))
    try:
        regions.start(pages["probe"])
        regions.focus()
        check_frameless_regions(regions, report)
    except SessionError as error:
        print(f"skipped: {error}")
    finally:
        if not arguments.keep:
            regions.stop()
            shutil.rmtree(root, ignore_errors=True)

    print()
    for command, reason in sorted(UNSWEPT.items()):
        print(f"not sent: {command} — {reason}")
    print("not checked here: the theme repaint and the default browser, each of which")
    print("                  changes the machine that runs it and so has a script of its")
    print("                  own: check_theme_repaint.py, check_default_browser.py")

    failed = report.failed()
    print(f"\n{len(report.results) - failed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
