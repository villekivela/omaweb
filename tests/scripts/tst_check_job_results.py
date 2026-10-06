#!/usr/bin/env python3
"""The verdict CI's required `arch-linux` check gives over the jobs it stands for.

A failed or cancelled job fails it. A skipped one passes, because the build jobs
are skipped on a change that is prose alone (docs/development.md).
"""

from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "check_job_results.py"


def verdict(results: dict[str, str]) -> subprocess.CompletedProcess[str]:
    """What the script answers for jobs with `results`, as `toJSON(needs)` has them."""
    needs = {job: {"result": result, "outputs": {}} for job, result in results.items()}
    return subprocess.run(
        (sys.executable, str(SCRIPT)),
        input=json.dumps(needs),
        capture_output=True,
        text=True,
    )


class CheckJobResultsTest(unittest.TestCase):
    def test_jobs_that_all_passed_pass(self) -> None:
        answer = verdict({"arch-linux-clang": "success", "arch-linux-budget": "success"})
        self.assertEqual(answer.returncode, 0, answer.stderr)

    def test_jobs_skipped_for_a_change_of_prose_pass(self) -> None:
        answer = verdict({"arch-linux-clang": "skipped", "arch-linux-budget": "skipped"})
        self.assertEqual(answer.returncode, 0, answer.stderr)

    def test_one_failed_job_fails_and_is_named(self) -> None:
        answer = verdict({"arch-linux-clang": "success", "arch-linux-budget": "failure"})
        self.assertEqual(answer.returncode, 1)
        self.assertIn("arch-linux-budget", answer.stderr)
        self.assertNotIn("arch-linux-clang", answer.stderr)

    def test_a_cancelled_job_fails(self) -> None:
        answer = verdict({"arch-linux-clang": "cancelled", "arch-linux-release": "success"})
        self.assertEqual(answer.returncode, 1)
        self.assertIn("arch-linux-clang", answer.stderr)

    def test_no_jobs_at_all_fails(self) -> None:
        # A gate that stands for nothing would pass whatever broke.
        answer = verdict({})
        self.assertEqual(answer.returncode, 1)


if __name__ == "__main__":
    unittest.main()
