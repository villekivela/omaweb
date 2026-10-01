#!/usr/bin/env python3
"""Check the approved engine security baseline against what upstream ships.

`security/baseline.json` names the QtWebEngine an Omaweb build is supported on
and the Chromium release whose security fixes that engine carries. Both go stale
on someone else's schedule: Qt publishes a patch release, Chromium publishes a
security fix, and nothing in this repository changes. So this looks daily and
leaves one issue behind, editing it rather than commenting on it.

What it opens an issue for is a QtWebEngine newer than the approved one, because
that is the actionable half: a security-bearing Qt patch has to produce a tested
Omaweb update within two days (SECURITY.md), and until the baseline is raised
every build below it is an unsupported preview. How far the approved engine's
fixes are behind Chromium's own stable release is reported alongside, because it
is what says whether the wait is urgent.

Usage:

    scripts/check_security_baseline.py --report report.json
    scripts/check_security_baseline.py --issue-from report.json
"""

from __future__ import annotations

import argparse
import datetime
import json
import subprocess
import sys
import re
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "security" / "baseline.json"

QT_RELEASES = "https://download.qt.io/official_releases/"
CHROMIUM_STABLE = ("https://chromiumdash.appspot.com/fetch_releases"
                   "?channel=Stable&platform=Linux&num=1")

LABEL = "engine-security-baseline"
LABEL_COLOR = "b60205"
LABEL_DESCRIPTION = "The approved QtWebEngine baseline is behind upstream"

TRIAGE_LABEL = "ready-for-human"

# An issue a workflow opens notifies nobody. Assigning it puts it where the
# person who has to act on it already looks.
ASSIGNEE = "villekivela"

MARKER = "<!-- omaweb:engine-security-baseline -->"

RESPONSE_DAYS = 2


def version(value: str) -> tuple[int, ...]:
    """A dotted version as numbers. Anything unparsable sorts below every real
    version rather than above it: an unreadable version is not evidence that
    something newer is available."""
    parts = []
    for field in str(value).strip().split("."):
        if not field.isdigit():
            return ()
        parts.append(int(field))
    return tuple(parts)


def newer(candidate: str, than: str) -> bool:
    left, right = version(candidate), version(than)
    if not left or not right:
        return False
    return left > right


def baseline_report(baseline: dict, released_engine: str,
                    chromium_stable: str) -> dict:
    """What the approved baseline is, what upstream has, and whether the
    baseline has to move. Pure, so the decision can be tested without the
    network the two versions come from."""
    approved_engine = baseline.get("qtwebengine", "")
    approved_patch = baseline.get("chromiumSecurityPatch", "")
    engine_behind = newer(released_engine, approved_engine)
    chromium_behind = newer(chromium_stable, approved_patch)
    reasons = []
    if engine_behind:
        reasons.append(
            f"Qt has released QtWebEngine {released_engine}; the approved "
            f"baseline is {approved_engine}.")
    if chromium_behind:
        reasons.append(
            f"The approved engine carries Chromium security fixes up to "
            f"{approved_patch}; Chromium stable is {chromium_stable}.")
    if not released_engine or not chromium_stable:
        reasons.append(
            "Upstream did not answer with a version for every check, so this "
            "report is incomplete.")
    return {
        "approved": {
            "qtwebengine": approved_engine,
            "chromium": baseline.get("chromium", ""),
            "chromiumSecurityPatch": approved_patch,
        },
        "available": {
            "qtwebengine": released_engine,
            "chromium": chromium_stable,
        },
        "reviewed": baseline.get("reviewed", ""),
        "engineBehind": engine_behind,
        "chromiumBehind": chromium_behind,
        "behind": engine_behind,
        "reasons": reasons,
    }


def report_summary(report: dict) -> str:
    approved, available = report["approved"], report["available"]
    lines = [
        f"Approved: QtWebEngine {approved['qtwebengine']} on Chromium "
        f"{approved['chromium']}, security fixes up to "
        f"{approved['chromiumSecurityPatch']} (reviewed {report['reviewed']}).",
        f"Released: QtWebEngine {available['qtwebengine'] or 'unknown'}.",
        f"Chromium stable: {available['chromium'] or 'unknown'}.",
    ]
    lines += [f"- {reason}" for reason in report["reasons"]]
    if not report["reasons"]:
        lines.append("- The approved baseline is the current engine.")
    return "\n".join(lines)


def gh(*arguments: str) -> str:
    result = subprocess.run(["gh", *arguments], capture_output=True, text=True,
                            check=False)
    if result.returncode != 0:
        sys.exit(f"gh {' '.join(arguments[:2])} failed: {result.stderr.strip()}")
    return result.stdout


def issue_title(report: dict) -> str:
    return (f"Raise the engine security baseline to QtWebEngine "
            f"{report['available']['qtwebengine']}")


def issue_body(report: dict, today: datetime.date | None = None) -> str:
    today = today or datetime.date.today()
    due = today + datetime.timedelta(days=RESPONSE_DAYS)
    approved = report["approved"]
    lines = [
        MARKER,
        f"Qt has released a newer QtWebEngine than the one "
        f"`security/baseline.json` approves, so every build is below the "
        f"baseline it claims until this is reviewed.",
        "",
        "### What upstream has", "",
        *(f"- {reason}" for reason in report["reasons"]),
        "",
        "### What is approved now", "",
        f"- QtWebEngine `{approved['qtwebengine']}`",
        f"- Chromium `{approved['chromium']}`, security fixes up to "
        f"`{approved['chromiumSecurityPatch']}`",
        f"- Reviewed `{report['reviewed']}`",
        "",
        "### What closes this", "",
        "1. Read the Qt release notes for the security fixes it carries.",
        "2. Build and run the suite against the new engine.",
        "3. Update `qtwebengine`, `chromium`, `chromiumSecurityPatch`, and "
        "`reviewed` in `security/baseline.json`.",
        "",
        f"A security-bearing Qt patch produces a tested Omaweb update within "
        f"{RESPONSE_DAYS} days (SECURITY.md), so this is due by `{due}`.",
    ]
    return "\n".join(lines)


def open_issue(run) -> int | None:
    """The number of the tracking issue, if one is open. The label is the
    handle; the marker in the body finds the issue again if the label is
    dropped, so a second issue is never opened alongside the first."""
    for query in (("--label", LABEL), ("--search", f"{MARKER} in:body")):
        listed = run("issue", "list", "--state", "open", *query,
                     "--json", "number", "--limit", "1")
        issues = json.loads(listed or "[]")
        if issues:
            return issues[0]["number"]
    return None


def sync_issue(report: dict, run=gh, today: datetime.date | None = None) -> int:
    existing = open_issue(run)

    if not report["behind"]:
        if existing is not None:
            run("issue", "close", str(existing), "--comment",
                f"The approved baseline is QtWebEngine "
                f"{report['approved']['qtwebengine']}, which is the engine Qt "
                f"has released.")
            print(f"Closed #{existing}: the baseline is the current engine.")
        else:
            print("The approved baseline is the current engine. "
                  "No issue to open.")
        return 0

    title, body = issue_title(report), issue_body(report, today)
    if existing is not None:
        run("issue", "edit", str(existing), "--title", title, "--body", body)
        print(f"Updated #{existing}: {title}")
        return 0

    run("label", "create", LABEL, "--force", "--color", LABEL_COLOR,
        "--description", LABEL_DESCRIPTION)
    run("issue", "create", "--title", title, "--body", body,
        "--label", LABEL, "--label", TRIAGE_LABEL, "--assignee", ASSIGNEE)
    print(f"Opened an issue: {title}")
    return 0


def fetch_json(url: str):
    with urllib.request.urlopen(url, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def fetch_text(url: str) -> str:
    with urllib.request.urlopen(url, timeout=30) as response:
        return response.read().decode("utf-8", "replace")


def fetch_listing(url: str) -> str:
    """A download.qt.io directory listing, or nothing where there is none: the
    engine's own release directory does not exist until its first release."""
    try:
        return fetch_text(url)
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return ""
        raise


def url_exists(url: str) -> bool:
    request = urllib.request.Request(url, method="HEAD")
    try:
        with urllib.request.urlopen(request, timeout=30):
            return True
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return False
        raise


def versions(listing: str, pattern: str) -> list[str]:
    """The versions a download.qt.io directory listing names, highest first."""
    return sorted(set(re.findall(pattern, listing)), key=version, reverse=True)


SERIES = r'href="(\d+\.\d+)/"'
RELEASE = r'href="(\d+\.\d+\.\d+)/"'


def released_engine_version(fetch=fetch_listing, exists=url_exists,
                            releases: str = QT_RELEASES) -> str:
    """The newest QtWebEngine Qt itself has published. Qt is the upstream that
    matters here rather than a distribution's package, because the distribution
    lags Qt by days and the response window starts when Qt publishes.

    Up to 6.11 the engine was a module of Qt's release and carried its version.
    From Qt 6.12 it is released on its own and versioned after its Chromium, so
    Qt 6.12.0 has no engine and the newest Qt release is no longer the newest
    engine (#484). Both are read: the newest Qt release that ships an engine,
    and the newest separate engine release, and the higher wins. Where separate
    releases will sit is not known before the first one, so a directory per
    series and a directory per version are both read. The series' own
    `scripts/newest-engine.sh` applies the same rule."""
    found = []
    for series in versions(fetch(f"{releases}qt/"), SERIES):
        shipped = [
            release
            for release in versions(fetch(f"{releases}qt/{series}/"), RELEASE)
            if exists(f"{releases}qt/{series}/{release}/submodules/"
                      f"qtwebengine-everywhere-src-{release}.tar.xz")]
        if shipped:
            found.append(shipped[0])
            break
    engine = f"{releases}qtwebengine/"
    listing = fetch(engine)
    found += versions(listing, RELEASE)
    for series in versions(listing, SERIES):
        found += versions(fetch(f"{engine}{series}/"), RELEASE)
    return max(found, key=version) if found else ""


def chromium_stable_version() -> str:
    releases = fetch_json(CHROMIUM_STABLE) or []
    return releases[0].get("version", "") if releases else ""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path,
                        help="write the report as JSON to this path")
    parser.add_argument("--issue-from", type=Path, metavar="REPORT",
                        help="open, update or close the tracking issue from a "
                             "report already written, without asking upstream "
                             "again")
    arguments = parser.parse_args()

    if arguments.issue_from:
        return sync_issue(json.loads(arguments.issue_from.read_text()))

    baseline = json.loads(BASELINE.read_text())
    report = baseline_report(baseline, released_engine_version(),
                             chromium_stable_version())
    print(report_summary(report))
    if arguments.report:
        arguments.report.write_text(json.dumps(report, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
