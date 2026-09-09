#!/usr/bin/env python3
"""Confirms Omaweb can become the default browser, and answers a link as a tab.

Two things, and the second is the one worth the trouble. Registering is a
setting. Answering is the behaviour a reader notices: a link clicked in another
application has to arrive in the browser they already have open, as a tab,
rather than starting a second browser beside it.

This changes the machine that runs it, which is why #103 left it to a person
for so long. It is scriptable, but only carefully:

The entry it registers is the reader's own, in `~/.local/share/applications`,
never the packaged one in `/usr`. Nothing is installed and no root is needed.
The entry names the built browser by absolute path and carries the same
throwaway directories the browser under test runs on, so both halves of the
round trip are the one instance and neither is the reader's.

The default browser is put back even when a check fails. A run killed outright
cannot put anything back, and would leave the desktop pointing at a binary in a
build directory, so the repair is printed before anything changes.

Usage:

    OMAWEB_DEFAULT_BROWSER_CHECK_ALLOW_HOST=1 scripts/check_default_browser.py
"""

from __future__ import annotations

import argparse
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
    clients,
    require_session,
)

# The name the running browser claims, and the one a second launch looks for.
BUS_NAME = "dev.omaweb.browser"
# A launcher that blocks until the browser it started exits would otherwise
# hold this run open for as long as that browser lives.
OPEN_TIMEOUT = 30.0

APPLICATIONS = os.path.expanduser("~/.local/share/applications")
ENTRY = os.path.join(APPLICATIONS, "omaweb-under-test.desktop")
# The entry's own name, which is what xdg-settings takes and reports.
ENTRY_ID = os.path.basename(ENTRY)

ENTRY_BODY = """[Desktop Entry]
Type=Application
Name=Omaweb under test
Comment=Registered by scripts/check_default_browser.py, and removed by it
Exec=env OMAWEB_DATA_ROOT={data} OMAWEB_CONFIG_ROOT={configuration} {browser} %u
Icon=omaweb
Terminal=false
Categories=Network;WebBrowser;
MimeType=text/html;x-scheme-handler/http;x-scheme-handler/https;
StartupNotify=true
StartupWMClass=omaweb
"""


def default_browser() -> str:
    result = subprocess.run(
        ["xdg-settings", "get", "default-web-browser"], capture_output=True, text=True, check=False
    )
    return result.stdout.strip()


def set_default_browser(entry: str) -> str:
    result = subprocess.run(
        ["xdg-settings", "set", "default-web-browser", entry],
        capture_output=True,
        text=True,
        check=False,
    )
    return result.stderr.strip()


def omaweb_windows() -> set[int]:
    return {client["pid"] for client in clients() if client.get("class") == "omaweb"}


def bus_name_held() -> bool:
    result = subprocess.run(
        ["busctl", "--user", "list", "--no-legend"], capture_output=True, text=True, check=False
    )
    return any(line.split()[:1] == [BUS_NAME] for line in result.stdout.splitlines() if line)


def require_host() -> None:
    if os.environ.get("OMAWEB_DEFAULT_BROWSER_CHECK_ALLOW_HOST") != "1":
        raise SessionError(
            "this changes the desktop's default browser while it runs; "
            "set OMAWEB_DEFAULT_BROWSER_CHECK_ALLOW_HOST=1 to allow it"
        )
    for tool in ("xdg-settings", "xdg-open", "busctl"):
        if shutil.which(tool) is None:
            raise SessionError(f"{tool} is not installed")
    # The browser under test has to be the one that claims the desktop's name,
    # and only the first to ask gets it. With the reader's own Omaweb running,
    # the link under test would be handed to theirs: a tab would open in the
    # browser they are reading, and this would report a pass for it.
    if bus_name_held():
        raise SessionError(
            f"an Omaweb already holds {BUS_NAME}; close it, "
            "because this check has to be the browser that answers"
        )
    if os.path.exists(ENTRY):
        raise SessionError(f"{ENTRY} is already there, and this will not overwrite it")


def main() -> int:
    # Line buffered, because this changes the machine and a run that is killed
    # has to have already printed how far it got and what it had changed.
    sys.stdout.reconfigure(line_buffering=True)

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", default="build/dev/omaweb", help="the browser to drive")
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

    original = default_browser()
    if not original:
        print("skipped: the desktop reports no default browser, so there is none to put back")
        return 0
    print(f"the default browser is {original}, and it goes back to it: ")
    print(f"    xdg-settings set default-web-browser {original}")
    print(f"    rm -f {ENTRY}")

    browser_path = os.path.abspath(arguments.browser)
    root = tempfile.mkdtemp(prefix="omaweb-default-check-")
    page = os.path.join(root, "probe.html")
    with open(page, "w", encoding="utf-8") as handle:
        handle.write(PROBE_PAGE)

    report = Report()
    # On the session bus rather than a private one: the handover is the thing
    # being tested, and it happens over the bus the desktop uses.
    browser = Browser(arguments.browser, root, private_bus=False)
    try:
        os.makedirs(APPLICATIONS, exist_ok=True)
        with open(ENTRY, "w", encoding="utf-8") as handle:
            handle.write(
                ENTRY_BODY.format(
                    data=os.path.join(root, "data"),
                    configuration=os.path.join(root, "config"),
                    browser=browser_path,
                )
            )
        subprocess.run(["update-desktop-database", APPLICATIONS], capture_output=True, check=False)

        complaint = set_default_browser(ENTRY_ID)
        registered = default_browser()
        # `xdg-settings` refuses outright while `BROWSER` is set, and Omarchy
        # exports it. Naming it here because the tool's own message says what
        # it will not do without saying what a reader would have to change, and
        # Settings offers this through the same tool.
        because = ""
        if registered != ENTRY_ID and os.environ.get("BROWSER"):
            because = f"; BROWSER={os.environ['BROWSER']!r} is set in this session"
        report.check(
            registered == ENTRY_ID,
            "Omaweb can be made the desktop's default browser",
            f"xdg-settings reports {registered!r}"
            + (f", saying {complaint!r}" if complaint else "")
            + because,
        )

        browser.start(f"file://{page}")
        before = omaweb_windows()
        report.check(
            bool(before), "a browser is running to answer the link", f"pid {sorted(before)}"
        )

        # The link a reader clicks elsewhere. `xdg-open` is what those
        # applications call, so it is what this calls rather than the browser.
        #
        # Bounded, because a launcher waits for what it started: if the
        # handover fails, the second browser runs until something stops it and
        # `xdg-open` does not return until then. That timeout is a result
        # rather than an error, so it is recorded and the checks below still
        # read what actually happened.
        launcher_returned = True
        try:
            subprocess.run(
                ["xdg-open", f"file://{page}?opened-elsewhere"],
                capture_output=True,
                check=False,
                timeout=OPEN_TIMEOUT,
            )
        except subprocess.TimeoutExpired:
            launcher_returned = False
        time.sleep(LOAD_SETTLE * 2)
        after = omaweb_windows()
        detail = f"pid {sorted(before)} answered it"
        if after != before:
            detail = f"pid {sorted(before)} became {sorted(after)}, so a second browser started"
        elif not launcher_returned:
            detail = f"xdg-open did not return within {OPEN_TIMEOUT:.0f}s"
        report.check(after == before and launcher_returned,
            "a link from another application reaches the running browser", detail)
        # Whatever the launcher started that did not hand over is this check's
        # to end, or it outlives the run holding the desktop's name.
        for pid in sorted(after - before):
            subprocess.run(["kill", "-TERM", str(pid)], capture_output=True, check=False)
    except SessionError as error:
        print(f"skipped: {error}")
        return 0
    finally:
        browser.stop()
        set_default_browser(original)
        if os.path.exists(ENTRY):
            os.remove(ENTRY)
        subprocess.run(["update-desktop-database", APPLICATIONS], capture_output=True, check=False)
        shutil.rmtree(root, ignore_errors=True)
        print(f"\nthe default browser is {default_browser()} again")

    failed = report.failed()
    print(f"\n{len(report.results) - failed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
