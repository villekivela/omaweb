#!/usr/bin/env python3
"""The verdict CI's `arch-linux` check gives over the jobs it stands for.

`arch-linux` is the required check, and the clang build, the runtime budget and
the release build run as jobs of their own under it. A job that depends on a
failed one is skipped rather than failed by default, and a skipped required
check lets a pull request merge, so the verdict is read from the results
instead. A skipped job passes, because the build jobs are skipped on a change
that is prose alone.
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
        answer = verdict({"arch-linux-test": "success", "arch-linux-budget": "success"})
        self.assertEqual(answer.returncode, 0, answer.stderr)

    def test_jobs_skipped_for_a_change_of_prose_pass(self) -> None:
        answer = verdict({"arch-linux-test": "skipped", "arch-linux-budget": "skipped"})
        self.assertEqual(answer.returncode, 0, answer.stderr)

    def test_one_failed_job_fails_and_is_named(self) -> None:
        answer = verdict({"arch-linux-test": "success", "arch-linux-budget": "failure"})
        self.assertEqual(answer.returncode, 1)
        self.assertIn("arch-linux-budget", answer.stderr)
        self.assertNotIn("arch-linux-test", answer.stderr)

    def test_a_cancelled_job_fails(self) -> None:
        answer = verdict({"arch-linux-test": "cancelled", "arch-linux-release": "success"})
        self.assertEqual(answer.returncode, 1)
        self.assertIn("arch-linux-test", answer.stderr)

    def test_no_jobs_at_all_fails(self) -> None:
        # A gate that stands for nothing would pass whatever broke.
        answer = verdict({})
        self.assertEqual(answer.returncode, 1)


if __name__ == "__main__":
    unittest.main()
