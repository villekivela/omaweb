#!/usr/bin/env python3
"""The engine security baseline check and the issue it turns into, offline.

The two halves under test are the pure ones: `baseline_report` compares the
approved baseline to what upstream ships, and `sync_issue` decides what to do
with the single tracking issue. Everything either side of them is a network
fetch or a GitHub call.
"""

from __future__ import annotations

import contextlib
import datetime
import io
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import check_security_baseline as baseline  # noqa: E402

APPROVED = {
    "qtwebengine": "6.11.2",
    "chromium": "140.0.7339.225",
    "chromiumSecurityPatch": "151.0.7922.71",
    "reviewed": "2026-09-04",
}


class Recorder:
    """`gh` with the network taken out: what it was asked, and what it answers
    when asked for the open issue."""

    def __init__(self, open_issues=()):
        self.calls = []
        self._open_issues = list(open_issues)

    def __call__(self, *arguments):
        self.calls.append(arguments)
        if arguments[:3] == ("issue", "list", "--state"):
            issues = [{"number": self._open_issues[0]}] if self._open_issues else []
            return json.dumps(issues)
        return ""

    def called(self, *prefix):
        return [call for call in self.calls if call[:len(prefix)] == prefix]


def report(engine="6.11.2", chromium="151.0.7922.71"):
    return baseline.baseline_report(dict(APPROVED), engine, chromium)


class BaselineReport(unittest.TestCase):
    def test_the_approved_engine_is_not_behind_itself(self):
        result = report()

        self.assertFalse(result["behind"])
        self.assertFalse(result["engineBehind"])
        self.assertFalse(result["chromiumBehind"])
        self.assertEqual(result["reasons"], [])
        self.assertEqual(result["approved"]["qtwebengine"], "6.11.2")
        self.assertEqual(result["available"]["qtwebengine"], "6.11.2")

    def test_a_newer_packaged_engine_moves_the_baseline(self):
        result = report(engine="6.11.3")

        self.assertTrue(result["engineBehind"])
        self.assertTrue(result["behind"])
        self.assertTrue(any("6.11.3" in reason for reason in result["reasons"]))

    def test_an_older_packaged_engine_is_not_drift(self):
        result = report(engine="6.11.1")

        self.assertFalse(result["behind"])
        self.assertEqual(result["reasons"], [])

    def test_chromium_moving_on_is_reported_but_opens_nothing(self):
        result = report(chromium="151.0.7922.80")

        self.assertTrue(result["chromiumBehind"])
        self.assertFalse(result["behind"])
        self.assertTrue(any("151.0.7922.80" in reason
                            for reason in result["reasons"]))

    def test_a_version_upstream_would_not_give_is_not_treated_as_newer(self):
        for unreadable in ("", "6.11.2-beta", "unknown"):
            with self.subTest(unreadable=unreadable):
                result = baseline.baseline_report(dict(APPROVED), unreadable,
                                                  "151.0.7922.71")
                self.assertFalse(result["behind"])
        self.assertTrue(any("incomplete" in reason
                            for reason in report(engine="").get("reasons", [])))

    def test_the_summary_names_both_halves_of_the_baseline(self):
        summary = baseline.report_summary(report(engine="6.11.3"))

        self.assertIn("6.11.2", summary)
        self.assertIn("6.11.3", summary)
        self.assertIn("151.0.7922.71", summary)
        self.assertIn("2026-09-04", summary)


class TrackingIssue(unittest.TestCase):
    def sync(self, result, recorder):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            code = baseline.sync_issue(result, recorder,
                                       today=datetime.date(2026, 9, 4))
        return code, output.getvalue()

    def test_a_current_baseline_opens_nothing(self):
        recorder = Recorder()
        code, output = self.sync(report(), recorder)

        self.assertEqual(code, 0)
        self.assertEqual(recorder.called("issue", "create"), [])
        self.assertIn("No issue to open", output)

    def test_it_opens_one_issue_and_labels_it_for_a_human(self):
        recorder = Recorder()
        code, output = self.sync(report(engine="6.11.3"), recorder)

        self.assertEqual(code, 0)
        created = recorder.called("issue", "create")
        self.assertEqual(len(created), 1)
        self.assertIn("--label", created[0])
        self.assertIn(baseline.TRIAGE_LABEL, created[0])
        body = created[0][created[0].index("--body") + 1]
        self.assertIn(baseline.MARKER, body)
        self.assertIn("2026-09-11", body)
        self.assertIn("security/baseline.json", body)
        self.assertIn("Raise the engine security baseline", output)

    def test_a_second_run_updates_the_issue_it_already_opened(self):
        recorder = Recorder(open_issues=[42])
        code, output = self.sync(report(engine="6.11.3"), recorder)

        self.assertEqual(code, 0)
        self.assertEqual(recorder.called("issue", "create"), [])
        edited = recorder.called("issue", "edit", "42")
        self.assertEqual(len(edited), 1)
        self.assertIn("Updated #42", output)

    def test_a_caught_up_baseline_closes_the_issue(self):
        recorder = Recorder(open_issues=[42])
        code, output = self.sync(report(), recorder)

        self.assertEqual(code, 0)
        closed = recorder.called("issue", "close", "42")
        self.assertEqual(len(closed), 1)
        self.assertIn("6.11.2", closed[0][closed[0].index("--comment") + 1])
        self.assertIn("Closed #42", output)


class BaselineFile(unittest.TestCase):
    def test_the_approved_baseline_names_every_version_it_promises(self):
        approved = json.loads(baseline.BASELINE.read_text())

        for field in ("qtwebengine", "chromium", "chromiumSecurityPatch",
                      "reviewed"):
            self.assertIn(field, approved)
            self.assertTrue(baseline.version(approved[field])
                            or field == "reviewed", field)
        self.assertTrue(baseline.newer(approved["chromiumSecurityPatch"],
                                       approved["chromium"]))


if __name__ == "__main__":
    unittest.main()
