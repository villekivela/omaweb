#!/usr/bin/env python3
"""The tests that measure time run alone when the suite runs in parallel.

CI runs `ctest` with a job per processor. The runtime probes and the chrome's
frame intervals hold time against thresholds set on a machine doing nothing
else, so a test running beside them would widen what they measure. Each is
`RUN_SERIAL`, and the frame-interval probes run in tests of their own rather
than inside the UI suites, which take minutes and run in parallel.

Reads what ctest would run, as `ctest --show-only=json-v1` reports it for a
build:

    tst_probes_run_alone.py <ctest> <build directory>
"""

from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path

# What docs/development.md's runtime probes and frame intervals name.
MEASURING_TIME = (
    "omaweb-startup-probes",
    "omaweb-session-store",
    "omaweb-ui-performance",
    "omaweb-ui-themed-performance",
)
PERFORMANCE_PROBES = "tst_performance.qml"
# The UI suites that run beside other tests.
PARALLEL_UI_SUITES = ("omaweb-ui", "omaweb-ui-themed")

CTEST: str
BUILD: str


def registered() -> dict[str, dict]:
    shown = subprocess.run(
        (CTEST, "--show-only=json-v1", "--test-dir", BUILD),
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return {test["name"]: test for test in json.loads(shown)["tests"]}


def properties(test: dict) -> dict[str, object]:
    return {entry["name"]: entry["value"] for entry in test.get("properties", [])}


def loaded_from(test: dict) -> Path:
    """The QML a Qt Quick test reads: what follows `-input`."""
    command = test.get("command") or []
    if "-input" not in command:
        raise AssertionError(f"{test['name']} names no -input: {command}")
    return Path(command[command.index("-input") + 1])


def probes_under(directory: Path) -> list[Path]:
    """Every copy of the performance probes a run from `directory` would load."""
    return sorted(directory.rglob(PERFORMANCE_PROBES))


class ProbesRunAloneTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.tests = registered()

    def test_each_test_that_measures_time_runs_alone(self) -> None:
        for name in MEASURING_TIME:
            with self.subTest(name):
                self.assertTrue(name in self.tests, f"{name} is not registered")
                self.assertIs(properties(self.tests[name]).get("RUN_SERIAL"), True)

    def test_the_performance_tests_run_the_frame_interval_probes(self) -> None:
        for name in ("omaweb-ui-performance", "omaweb-ui-themed-performance"):
            with self.subTest(name):
                self.assertTrue(probes_under(loaded_from(self.tests[name])))

    def test_the_parallel_ui_suites_leave_the_probes_out(self) -> None:
        for name in PARALLEL_UI_SUITES:
            with self.subTest(name):
                self.assertNotIn("RUN_SERIAL", properties(self.tests[name]))
                self.assertEqual(probes_under(loaded_from(self.tests[name])), [])


if __name__ == "__main__":
    CTEST, BUILD = sys.argv[1], sys.argv[2]
    unittest.main(argv=sys.argv[:1])
