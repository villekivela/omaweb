#!/usr/bin/env python3
"""Check the approved engine security baseline against what upstream ships.

`security/baseline.json` names the QtWebEngine an Omaweb build is supported on
and the Chromium release whose security fixes that engine carries. Both go stale
on someone else's schedule: Qt publishes a patch release, Chromium publishes a
security fix, and nothing in this repository changes. So this looks weekly and
leaves one issue behind.

What it opens an issue for is a newer QtWebEngine than the approved one, because
that is the actionable half: a security-bearing Qt patch has to produce a tested
Omaweb update within seven days (SECURITY.md), and until the baseline is raised
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
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "security" / "baseline.json"

ARCH_PACKAGE = ("https://archlinux.org/packages/search/json/"
                "?name=qt6-webengine&repo=Extra")
CHROMIUM_STABLE = ("https://chromiumdash.appspot.com/fetch_releases"
                   "?channel=Stable&platform=Linux&num=1")

LABEL = "engine-security-baseline"
LABEL_COLOR = "b60205"
LABEL_DESCRIPTION = "The approved QtWebEngine baseline is behind upstream"

TRIAGE_LABEL = "ready-for-human"

MARKER = "<!-- omaweb:engine-security-baseline -->"

RESPONSE_DAYS = 7


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


def baseline_report(baseline: dict, packaged_engine: str,
                    chromium_stable: str) -> dict:
    """What the approved baseline is, what upstream has, and whether the
    baseline has to move. Pure, so the decision can be tested without the
    network the two versions come from."""
    approved_engine = baseline.get("qtwebengine", "")
    approved_patch = baseline.get("chromiumSecurityPatch", "")
    engine_behind = newer(packaged_engine, approved_engine)
    chromium_behind = newer(chromium_stable, approved_patch)
    reasons = []
    if engine_behind:
        reasons.append(
            f"QtWebEngine {packaged_engine} is packaged; the approved baseline "
            f"is {approved_engine}.")
    if chromium_behind:
        reasons.append(
            f"The approved engine carries Chromium security fixes up to "
            f"{approved_patch}; Chromium stable is {chromium_stable}.")
    if not packaged_engine or not chromium_stable:
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
            "qtwebengine": packaged_engine,
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
        f"Packaged: QtWebEngine {available['qtwebengine'] or 'unknown'}.",
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
        f"A newer QtWebEngine is packaged than the one "
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
                f"{report['approved']['qtwebengine']}, which is the packaged "
                f"engine.")
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
        "--label", LABEL, "--label", TRIAGE_LABEL)
    print(f"Opened an issue: {title}")
    return 0


def fetch_json(url: str):
    with urllib.request.urlopen(url, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def packaged_engine_version() -> str:
    results = fetch_json(ARCH_PACKAGE).get("results") or []
    return results[0].get("pkgver", "") if results else ""


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
    report = baseline_report(baseline, packaged_engine_version(),
                             chromium_stable_version())
    print(report_summary(report))
    if arguments.report:
        arguments.report.write_text(json.dumps(report, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
