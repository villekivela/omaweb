#!/usr/bin/env python3
"""When the freezing budget takes its frozen reading, without a browser.

`scripts/benchmark_runtime.py freezing` reads the whole process tree, so whatever the browser is
still doing after a Space switch lands in the window it attributes to the frozen page. The reading
it measures from waits for the tree to stop moving, and that wait is decided by code that needs no
browser, so it is checked here against readings played back in order.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import benchmark_runtime as runtime  # noqa: E402


class Tree:
    """A process tree that reads a given series of sizes, on a clock that moves only when slept."""

    def __init__(self, readings: list[float]) -> None:
        self.readings = list(readings)
        self.now = 0.0
        self.reads = 0

    def read(self) -> float:
        self.reads += 1
        return self.readings.pop(0) if len(self.readings) > 1 else self.readings[0]

    def sleep(self, seconds: float) -> None:
        self.now += seconds

    def clock(self) -> float:
        return self.now


def settle(tree: Tree, interval: float = 2.0, tolerance: float = 0.25, limit: float = 30.0):
    return runtime.settled_reading(tree.read, interval=interval, tolerance=tolerance, limit=limit,
                                   sleep=tree.sleep, clock=tree.clock)


class SettledReadingTest(unittest.TestCase):

    def test_a_tree_that_is_already_still_is_read_after_one_interval(self):
        tree = Tree([500.0, 500.1])
        reading = settle(tree)
        self.assertTrue(reading.settled)
        self.assertEqual(reading.mebibytes, 500.1)
        self.assertEqual(reading.seconds, 2.0)

    def test_a_tree_still_settling_is_read_once_two_readings_agree(self):
        tree = Tree([500.0, 503.0, 505.5, 506.0, 506.1])
        reading = settle(tree)
        self.assertTrue(reading.settled)
        self.assertEqual(reading.mebibytes, 506.1)
        self.assertEqual(reading.seconds, 8.0)

    def test_readings_that_agree_only_by_moving_back_still_count_as_moving(self):
        tree = Tree([500.0, 497.0, 497.1])
        reading = settle(tree)
        self.assertEqual(reading.mebibytes, 497.1)
        self.assertEqual(reading.seconds, 4.0)

    # A page still running never settles, and the wait has to end so the budget can say so.
    def test_a_tree_that_keeps_growing_is_read_at_the_limit(self):
        tree = Tree([500.0 + 10.0 * step for step in range(40)])
        reading = settle(tree, limit=30.0)
        self.assertFalse(reading.settled)
        self.assertEqual(reading.seconds, 30.0)
        self.assertEqual(reading.mebibytes, 650.0)
        self.assertEqual(tree.reads, 16)


if __name__ == "__main__":
    unittest.main()
