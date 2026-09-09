#!/usr/bin/env python3
"""Confirms a theme change repaints a running browser, without restarting it.

This is the alpha gate that no headless suite reaches. `ThemeController` reads
the theme Omarchy renders and watches it, so what has to be shown is not that
the file changed but that the window did, while the same process kept running.

So this reads pixels, which the session sweep deliberately does not. There is
no other honest evidence: a repaint is a change to what is on screen, and a
check that read the rendered file back would be checking Omarchy's output
rather than Omaweb's. It captures the window under test and nothing else, and
the window belongs to this script rather than to the reader.

The oracle is the theme's own `sidebar` colour. The sidebar is chrome Omaweb
draws itself, unlike the page area, whose colour belongs to whatever is loaded.

Driving this changes the desktop's theme for as long as it runs, which is why
it asks first, and why it puts the original theme back even when a check fails.
A run killed outright cannot put anything back, so it prints the command that
does before it changes anything.

Usage:

    OMAWEB_THEME_CHECK_ALLOW_HOST=1 scripts/check_theme_repaint.py
    OMAWEB_THEME_CHECK_ALLOW_HOST=1 scripts/check_theme_repaint.py --theme "Nord"
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

from omaweb_session import (
    LOAD_SETTLE,
    PROBE_PAGE,
    Browser,
    Report,
    SessionError,
    dispatch,
    require_session,
)

STATE = os.path.expanduser("~/.local/state/omarchy/current")
# The controller polls twice a second behind its file-system events, so a
# repaint that took the slow path still has time to arrive.
REPAINT_SETTLE = 3.0
# Far enough into the sidebar to miss its border, and far enough down to miss
# the navigation strip and the tab rows.
PATCH = (150, 600)
PATCH_SIZE = 24
# The sidebar is drawn at 0.95 opacity over whatever is behind the window, so
# the colour on screen is the declared one give or take that blend.
TOLERANCE = 24


def theme_name() -> str:
    with open(f"{STATE}/theme.name", encoding="utf-8") as handle:
        return handle.read().strip()


def declared(key: str) -> str:
    with open(f"{STATE}/theme/omaweb.json", encoding="utf-8") as handle:
        return json.load(handle)[key]


def rgb(value: str) -> tuple[int, ...]:
    value = value.lstrip("#")
    return tuple(int(value[index : index + 2], 16) for index in (0, 2, 4))


def patch(at: list[int]) -> tuple[int, ...] | None:
    """The commonest colour in one small square of the window under test.

    PPM rather than PNG because it needs no decoder: a short header, then three
    bytes a pixel, which is a few lines here against a dependency otherwise.
    """
    geometry = f"{at[0] + PATCH[0]},{at[1] + PATCH[1]} {PATCH_SIZE}x{PATCH_SIZE}"
    raw = subprocess.run(
        ["grim", "-t", "ppm", "-g", geometry, "-"], capture_output=True, check=False
    ).stdout
    fields: list[bytes] = []
    body = raw
    while len(fields) < 4 and body:
        head, _, body = body.partition(b"\n")
        fields += head.split()
    pixels = [tuple(body[index : index + 3]) for index in range(0, len(body) - 2, 3)]
    return collections.Counter(pixels).most_common(1)[0][0] if pixels else None


def near(seen: tuple[int, ...] | None, declared_colour: tuple[int, ...]) -> bool:
    return bool(seen) and all(abs(a - b) <= TOLERANCE for a, b in zip(seen, declared_colour))


def set_theme(name: str) -> None:
    subprocess.run(["omarchy", "theme", "set", name], capture_output=True, check=False)
    wanted = name.lower().replace(" ", "-")
    for _ in range(40):
        time.sleep(0.5)
        if theme_name() == wanted:
            return


def require_host() -> None:
    if os.environ.get("OMAWEB_THEME_CHECK_ALLOW_HOST") != "1":
        raise SessionError(
            "this changes the desktop's theme while it runs; "
            "set OMAWEB_THEME_CHECK_ALLOW_HOST=1 to allow it"
        )
    for tool in ("omarchy", "grim"):
        if shutil.which(tool) is None:
            raise SessionError(f"{tool} is not installed")
    if not os.path.exists(f"{STATE}/theme/omaweb.json"):
        raise SessionError("Omarchy has rendered no theme for Omaweb, so there is none to change")


def main() -> int:
    # Line buffered, because this changes the theme in force and a run that is
    # killed has to have already printed the command that puts it back.
    sys.stdout.reconfigure(line_buffering=True)

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", default="build/dev/omaweb", help="the browser to drive")
    parser.add_argument(
        "--theme",
        default="Catppuccin Latte",
        help="the theme to change to, which has to differ from the one in force",
    )
    arguments = parser.parse_args()

    try:
        require_session()
        require_host()
    except SessionError as error:
        print(f"skipped: {error}")
        return 0
    if not os.access(arguments.browser, os.X_OK):
        print(f"skipped: {arguments.browser} is not built")
        return 0

    original = theme_name()
    print(f"the theme in force is {original}, and it goes back to it: ")
    print(f"    omarchy theme set {original}")

    root = tempfile.mkdtemp(prefix="omaweb-theme-check-")
    page = os.path.join(root, "probe.html")
    with open(page, "w", encoding="utf-8") as handle:
        handle.write(PROBE_PAGE)

    report = Report()
    browser = Browser(arguments.browser, root)
    try:
        browser.start(f"file://{page}")
        browser.focus()
        # Floated and raised, because a patch of a window the layout has put
        # something else in front of is a patch of that something else.
        if not browser.floating():
            browser.float_window()
        browser.reshape(1200, 900)
        dispatch(f'hl.dsp.window.bring_to_top{{window="pid:{browser.pid}"}}')
        time.sleep(LOAD_SETTLE)

        started_as = browser.pid
        before_declared = rgb(declared("sidebar"))
        before_seen = patch(browser.position())
        report.check(
            near(before_seen, before_declared),
            f"the sidebar is drawn in the theme in force ({original})",
            f"saw {before_seen}, the theme declares {before_declared}",
        )

        set_theme(arguments.theme)
        switched = theme_name()
        time.sleep(REPAINT_SETTLE)
        after_declared = rgb(declared("sidebar"))
        report.check(
            switched != original and after_declared != before_declared,
            "the theme changed under the running browser",
            f"{original} {before_declared} to {switched} {after_declared}",
        )
        after_seen = patch(browser.position())
        report.check(
            near(after_seen, after_declared),
            "a theme change repaints the running browser",
            f"saw {after_seen}, the theme declares {after_declared}",
        )
        report.check(
            browser.pid == started_as and browser.alive(),
            "the browser repainted without restarting",
            f"pid {started_as} is still the window's"
            if browser.pid == started_as
            else f"pid {started_as} became {browser.pid}",
        )
    except SessionError as error:
        print(f"skipped: {error}")
        return 0
    finally:
        browser.stop()
        shutil.rmtree(root, ignore_errors=True)
        set_theme(original)
        print(f"\nthe theme is {theme_name()} again")

    failed = report.failed()
    print(f"\n{len(report.results) - failed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
