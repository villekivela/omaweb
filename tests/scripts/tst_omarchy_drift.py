#!/usr/bin/env python3
"""The drift report and the issue it turns into, without touching the network.

The two halves under test are the pure ones: `drift_report` compares a manifest
to an upstream tree listing, and `sync_issue` decides what to do with the single
tracking issue. Everything either side of them is a GitHub call.
"""

from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import report_omarchy_drift as report  # noqa: E402
import sync_omarchy_ui as sync  # noqa: E402

PINNED = "1" * 40
HEAD = "2" * 40


def manifest(**files: str) -> dict:
    return {
        "ref": PINNED,
        "files": {
            path: {"upstream": f"shell/Ui/{Path(path).name}", "blob": blob,
                   "sha256": blob}
            for path, blob in files.items()
        },
    }


def upstream(**files: str) -> dict:
    return {
        path: {"upstream": f"shell/Ui/{Path(path).name}", "blob": blob}
        for path, blob in files.items()
    }


class DriftReport(unittest.TestCase):
    def test_a_matching_pin_is_not_behind(self):
        result = sync.drift_report(manifest(**{"qs/Ui/Button.qml": "aaa"}), PINNED, None)

        self.assertFalse(result["behind"])
        self.assertEqual(result["pinned"], PINNED)
        self.assertEqual(result["head"], PINNED)
        self.assertEqual(result["added"] + result["removed"] + result["changed"], [])

    def test_it_names_added_removed_and_changed_files(self):
        result = sync.drift_report(
            manifest(**{"qs/Ui/Button.qml": "aaa", "qs/Ui/Gone.qml": "bbb"}),
            HEAD,
            upstream(**{"qs/Ui/Button.qml": "ccc", "qs/Ui/New.qml": "ddd"}),
        )

        self.assertTrue(result["behind"])
        self.assertEqual(result["added"], ["qs/Ui/New.qml"])
        self.assertEqual(result["removed"], ["qs/Ui/Gone.qml"])
        self.assertEqual(result["changed"], ["qs/Ui/Button.qml"])

    def test_it_carries_the_compare_link_and_the_sync_command(self):
        result = sync.drift_report(manifest(), HEAD, upstream())

        self.assertEqual(
            result["compare"],
            f"https://github.com/{sync.REPOSITORY}/compare/{PINNED}...{HEAD}",
        )
        self.assertIn(HEAD, result["sync_command"])
        self.assertEqual(result["branch"], sync.BRANCH)


class IssueBody(unittest.TestCase):
    def body(self, **overrides) -> str:
        drifted = sync.drift_report(
            manifest(**{"qs/Ui/Button.qml": "aaa", "qs/Ui/Gone.qml": "bbb"}),
            HEAD,
            upstream(**{"qs/Ui/Button.qml": "ccc", "qs/Ui/New.qml": "ddd"}),
        )
        drifted.update(overrides)
        return report.issue_body(drifted)

    def test_it_lists_every_change_and_the_compare_link(self):
        body = self.body()

        self.assertIn("qs/Ui/New.qml", body)
        self.assertIn("qs/Ui/Gone.qml", body)
        self.assertIn("qs/Ui/Button.qml", body)
        self.assertIn(f"compare/{PINNED}...{HEAD}", body)
        self.assertIn("--sync --ref", body)

    def test_it_says_so_when_only_the_pin_moved(self):
        body = self.body(added=[], removed=[], changed=[])

        self.assertIn("No vendored file changed", body)

    def test_it_carries_the_marker_that_finds_the_issue_again(self):
        self.assertIn(report.MARKER, self.body())


class Recorder:
    """Stands in for the `gh` CLI, returning canned stdout per subcommand."""

    def __init__(self, **responses: str):
        self.responses = responses
        self.calls: list[list[str]] = []

    def __call__(self, *arguments: str) -> str:
        self.calls.append(list(arguments))
        return self.responses.get(" ".join(arguments[:2]), "")

    def ran(self, *prefix: str) -> bool:
        return any(call[: len(prefix)] == list(prefix) for call in self.calls)


class SyncIssue(unittest.TestCase):
    def sync(self, drift: dict, gh) -> None:
        # The script narrates what it did; the test reads `gh` instead.
        with contextlib.redirect_stdout(io.StringIO()):
            report.sync_issue(drift, gh)

    def setUp(self):
        self.behind = sync.drift_report(
            manifest(**{"qs/Ui/Button.qml": "aaa"}),
            HEAD,
            upstream(**{"qs/Ui/Button.qml": "ccc"}),
        )
        self.current = sync.drift_report(manifest(), PINNED, None)

    def test_drift_with_no_open_issue_opens_one(self):
        gh = Recorder(**{"issue list": "[]"})

        self.sync(self.behind, gh)

        self.assertTrue(gh.ran("issue", "create"))
        self.assertFalse(gh.ran("issue", "edit"))
        created = next(call for call in gh.calls if call[:2] == ["issue", "create"])
        self.assertIn(report.TRIAGE_LABEL, created)

    def test_drift_with_an_open_issue_updates_it_in_place(self):
        gh = Recorder(**{"issue list": json.dumps([{"number": 42}])})

        self.sync(self.behind, gh)

        self.assertFalse(gh.ran("issue", "create"))
        self.assertTrue(gh.ran("issue", "edit", "42"))

    def test_a_caught_up_pin_closes_the_open_issue(self):
        gh = Recorder(**{"issue list": json.dumps([{"number": 42}])})

        self.sync(self.current, gh)

        self.assertTrue(gh.ran("issue", "close", "42"))
        self.assertFalse(gh.ran("issue", "create"))

    def test_a_caught_up_pin_with_no_issue_does_nothing(self):
        gh = Recorder(**{"issue list": "[]"})

        self.sync(self.current, gh)

        self.assertFalse(gh.ran("issue", "create"))
        self.assertFalse(gh.ran("issue", "close"))


class ReportFile(unittest.TestCase):
    def test_the_report_round_trips_through_the_file_the_workflow_reads(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "drift.json"
            written = sync.drift_report(manifest(), HEAD, upstream())
            sync.write_report(path, written)

            self.assertEqual(json.loads(path.read_text()), written)


if __name__ == "__main__":
    unittest.main()
