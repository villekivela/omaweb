#!/usr/bin/env python3
"""Turn a `sync_omarchy_ui.py --check-upstream --report` file into one issue.

Upstream's `quattro` is an active branch and the pin only moves when someone
runs the sync, so drift is invisible until someone thinks to look. This is the
prompt to look: one open issue, updated in place while the pin is behind and
closed once it catches up. It never syncs anything — an upstream API change
lands on Tanto's adapters, so a human reads the diff.

Usage:

    scripts/report_omarchy_drift.py drift.json
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

from sync_omarchy_ui import report_changes

LABEL = "omarchy-drift"
LABEL_COLOR = "5319e7"
LABEL_DESCRIPTION = "The vendored Omarchy kit is behind upstream"

# Reading upstream's diff is the work, and it is not an agent's to do.
TRIAGE_LABEL = "ready-for-human"

# Kept in the body so the issue can be found again even if the label is lost.
MARKER = "<!-- tanto:omarchy-kit-drift -->"


def gh(*arguments: str) -> str:
    result = subprocess.run(["gh", *arguments], capture_output=True, text=True,
                            check=False)
    if result.returncode != 0:
        # gh says why on stderr; a traceback would say only that it failed.
        sys.exit(f"gh {' '.join(arguments[:2])} failed: {result.stderr.strip()}")
    return result.stdout


def issue_title(report: dict) -> str:
    return f"Omarchy kit is behind {report['branch']} ({report['head'][:12]})"


def issue_body(report: dict) -> str:
    lines = [
        MARKER,
        f"The vendored kit is pinned at `{report['pinned'][:12]}` and "
        f"`{report['branch']}` is at `{report['head'][:12]}`.",
        "",
    ]

    changes = report_changes(report)
    if changes:
        lines += ["### Vendored files", "",
                  *(f"- {kind}: `{path}`" for kind, path in changes), ""]
    else:
        lines += ["No vendored file changed — only the pin is behind.", ""]

    lines += [
        f"[Compare {report['pinned'][:12]}...{report['head'][:12]}]"
        f"({report['compare']})",
        "",
        "Read the diff before syncing: an upstream API change lands on Tanto's",
        "adapters in `src/ui`, not on the vendored copies.",
        "",
        "```sh",
        report["sync_command"],
        "```",
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


def sync_issue(report: dict, run=gh) -> int:
    existing = open_issue(run)

    if not report["behind"]:
        if existing is not None:
            run("issue", "close", str(existing), "--comment",
                f"The pin is back at `{report['branch']}` head "
                f"(`{report['head'][:12]}`).")
            print(f"Closed #{existing}: the pin is at {report['branch']} head.")
        else:
            print(f"Pinned at {report['branch']} head. No issue to open.")
        return 0

    title, body = issue_title(report), issue_body(report)
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


def main() -> int:
    if len(sys.argv) != 2:
        sys.exit(f"Usage: {Path(sys.argv[0]).name} <report.json>")
    return sync_issue(json.loads(Path(sys.argv[1]).read_text()))


if __name__ == "__main__":
    sys.exit(main())
