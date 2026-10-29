#!/usr/bin/env python3
"""The runtime budget measures the build a reader runs, and says so (#889).

CI's `arch-linux-budget` job once built the `ci` preset, which is Debug, while the package ships
the `release` preset's build, which read about 90 MiB more for one Space with one page. The
ceilings that guard readers must come from the build readers run, so the job builds `release` and
measures that binary, and every note in `performance/budget.json` names the build type.
"""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "ci.yml"
BUDGET = ROOT / "performance" / "budget.json"


def budget_job() -> str:
    """The text of the `arch-linux-budget` job, up to the next job."""
    text = WORKFLOW.read_text(encoding="utf-8")
    match = re.search(r"^  arch-linux-budget:\n(.*?)(?=^  [a-z][a-z0-9-]*:\n)", text,
                      re.DOTALL | re.MULTILINE)
    assert match, "ci.yml has no arch-linux-budget job"
    return match.group(1)


def commands(job: str) -> str:
    """The job's shell, without the comments that explain it."""
    return "\n".join(line for line in job.splitlines() if not line.lstrip().startswith("#"))


class BudgetBuild(unittest.TestCase):
    def test_jobConfiguresAndBuildsTheReleasePreset(self) -> None:
        shell = commands(budget_job())
        self.assertRegex(shell, r"cmake --preset release\b")
        self.assertRegex(shell, r"cmake --build --preset release\b")
        self.assertNotRegex(shell, r"--preset ci\b")

    def test_jobMeasuresTheReleaseBinary(self) -> None:
        shell = commands(budget_job())
        self.assertIn("--browser build/release/omaweb-browser", shell)
        self.assertNotIn("build/ci", shell)

    def test_everyNoteNamesTheBuildType(self) -> None:
        measurements = json.loads(BUDGET.read_text(encoding="utf-8"))["measurements"]
        for name, measurement in measurements.items():
            with self.subTest(name):
                self.assertRegex(measurement["note"], r"[Rr]elease build")


if __name__ == "__main__":
    unittest.main()
