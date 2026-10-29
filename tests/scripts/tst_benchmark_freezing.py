#!/usr/bin/env python3
"""When the freezing budget takes its frozen reading, without a browser.

`scripts/benchmark_runtime.py freezing` reads the processes the away Space's page started, so
nothing the browser or the page on show does after a Space switch lands in the window it attributes
to the frozen page. The reading it measures from waits for them to stop moving. Which processes it
reads and when that wait ends are decided by code that needs no browser, so they are checked here
against process trees and readings played back in order.
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


def settle(tree: Tree):
    return runtime.settled_reading(tree.read, sleep=tree.sleep, clock=tree.clock)


# Readings a step apart that the tolerance counts as still, and a step it counts as moving.
STILL = runtime.STEADY_TOLERANCE / 2
MOVING = runtime.STEADY_TOLERANCE * 4
INTERVAL = runtime.STEADY_INTERVAL


class SettledReadingTest(unittest.TestCase):

    def test_a_tree_that_is_already_still_is_read_after_two_intervals(self):
        tree = Tree([500.0, 500.0 + STILL, 500.0 + 2 * STILL])
        reading = settle(tree)
        self.assertTrue(reading.settled)
        self.assertEqual(reading.mebibytes, 500.0 + 2 * STILL)
        self.assertEqual(reading.seconds, 2 * INTERVAL)

    def test_a_tree_still_settling_is_read_once_three_readings_agree(self):
        tree = Tree([500.0, 500.0 + MOVING, 500.0 + 2 * MOVING, 500.0 + 2 * MOVING + STILL,
                     500.0 + 2 * MOVING + 2 * STILL])
        reading = settle(tree)
        self.assertTrue(reading.settled)
        self.assertEqual(reading.first, 500.0)
        self.assertEqual(reading.mebibytes, 500.0 + 2 * MOVING + 2 * STILL)
        self.assertEqual(reading.seconds, 4 * INTERVAL)

    def test_readings_that_agree_only_by_moving_back_still_count_as_moving(self):
        tree = Tree([500.0, 500.0 - MOVING, 500.0 - MOVING + STILL, 500.0 - MOVING + 2 * STILL])
        reading = settle(tree)
        self.assertTrue(reading.settled)
        self.assertEqual(reading.seconds, 3 * INTERVAL)

    # A pause in the browser's work after a switch is not the end of it.
    def test_one_agreeing_pair_between_moves_is_not_settled(self):
        tree = Tree([500.0, 500.0 + STILL, 500.0 + STILL + MOVING, 500.0 + 2 * STILL + MOVING,
                     500.0 + 3 * STILL + MOVING])
        reading = settle(tree)
        self.assertTrue(reading.settled)
        self.assertEqual(reading.seconds, 4 * INTERVAL)

    # A page still running never settles, and the wait has to end so the budget can say so.
    def test_a_tree_that_keeps_growing_is_read_at_the_limit(self):
        steps = int(runtime.STEADY_LIMIT / INTERVAL)
        tree = Tree([500.0 + MOVING * step for step in range(steps + 10)])
        reading = settle(tree)
        self.assertFalse(reading.settled)
        self.assertEqual(reading.seconds, steps * INTERVAL)
        self.assertEqual(reading.mebibytes - reading.first, MOVING * steps)
        self.assertEqual(tree.reads, steps + 1)


class TreeReadingTest(unittest.TestCase):
    """The whole tree's reading, and the part of it below the browser's own process."""

    def setUp(self):
        # launcher 1 -> browser 2 -> engine processes 3 and 4, and 4 has a child 5.
        self.tree = {1: [2], 2: [3, 4], 4: [5]}
        self.pss_kib = {1: 1024, 2: 400 * 1024, 3: 20 * 1024, 4: 50 * 1024, 5: 2 * 1024}
        self.saved = runtime.children_by_parent, runtime.read_pss_kib
        runtime.children_by_parent = lambda: self.tree
        runtime.read_pss_kib = lambda pid: self.pss_kib[pid]

    def tearDown(self):
        runtime.children_by_parent, runtime.read_pss_kib = self.saved

    def test_the_whole_tree_counts_every_process(self):
        self.assertEqual(runtime.tree_mib(1), 473.0)

    def test_the_reading_below_a_process_leaves_it_out(self):
        self.assertEqual(runtime.below_mib(2), 72.0)


class AwayReadingTest(unittest.TestCase):
    """The frozen page is read from the processes its Space started, and from nothing else."""

    def setUp(self):
        # browser 2 -> zygote 3 -> renderer 4, the page of the Space on show.
        self.tree = {2: [3], 3: [4]}
        self.rss_kib = {3: 10 * 1024, 4: 20 * 1024}
        self.pss_kib = dict(self.rss_kib)
        self.swap_kib: dict[int, int] = {}
        self.saved = runtime.children_by_parent, runtime.read_rollup_kib
        runtime.children_by_parent = lambda: self.tree
        runtime.read_rollup_kib = self.rollup
        self.shown = runtime.processes_below(2)
        # The away Space's page starts renderer 5.
        self.tree[3].append(5)
        self.rss_kib[5] = self.pss_kib[5] = 70 * 1024

    def tearDown(self):
        runtime.children_by_parent, runtime.read_rollup_kib = self.saved

    def rollup(self, pid: int, field: str) -> int:
        return {"Rss:": self.rss_kib, "Pss:": self.pss_kib,
                "Swap:": self.swap_kib}[field].get(pid, 0)

    def away(self) -> float:
        return runtime.held_mib(runtime.started_since(2, self.shown))

    def test_the_away_reading_is_what_the_away_spaces_page_started(self):
        self.assertEqual(self.away(), 70.0)

    # What #885 found on CI: the page brought back on show runs V8's memory reducer about twelve
    # seconds after the switch, faulting in most of a mebibyte of the engine's library.
    def test_the_page_on_show_does_not_move_the_away_reading(self):
        before = self.away()
        self.rss_kib[4] += 1024
        self.pss_kib[4] += 1024
        self.assertEqual(self.away(), before)

    # What a Freezing check saw cross its ceiling before #467: a step in the browser's own process.
    def test_the_browser_process_does_not_move_the_away_reading(self):
        before = self.away()
        self.rss_kib[2] = self.pss_kib[2] = 400 * 1024
        self.assertEqual(self.away(), before)

    # A process leaving the engine's library leaves the frozen page a larger share of it, which
    # #885 saw add 2 MiB to a frozen renderer that had touched nothing.
    def test_a_larger_share_of_the_same_pages_does_not_move_it(self):
        before = self.away()
        self.pss_kib[5] += 2 * 1024
        self.assertEqual(self.away(), before)

    # On a desktop that swaps, the kernel takes a frozen page's idle memory out of residence, which
    # read alone took 56 MiB off a frozen page that had done nothing.
    def test_the_kernel_swapping_the_page_out_does_not_move_it(self):
        before = self.away()
        self.rss_kib[5] -= 8 * 1024
        self.swap_kib[5] = 8 * 1024
        self.assertEqual(self.away(), before)

    def test_the_away_page_still_running_moves_it(self):
        before = self.away()
        self.rss_kib[5] += 1024
        self.pss_kib[5] += 1024
        self.assertEqual(self.away(), before + 1.0)

    def test_a_process_that_ended_holds_nothing(self):
        del self.rss_kib[5], self.pss_kib[5]
        self.assertEqual(self.away(), 0.0)


if __name__ == "__main__":
    unittest.main()
