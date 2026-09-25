#!/usr/bin/env python3
"""The page-load budget's plan, zone and arithmetic, without a browser.

What `scripts/benchmark_runtime.py pageload` measures is decided before any browser starts: which
pages load in which order, which hosts each one reaches, and what the local DNS server answers for
them. Those are the parts that can be wrong and still produce a number, so they are checked here.
The launch itself needs a Wayland session and a DNS server and runs in CI's budget step.
"""

from __future__ import annotations

import statistics
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import benchmark_runtime as runtime  # noqa: E402


def measured(plan, case):
    return [load for load in plan if load.case == case and load.measured]


def chain(zone: str, host: str) -> list[str]:
    """Follows a name through the zone's CNAME lines to the name holding the address."""
    aliases = {}
    addressed = set()
    for line in zone.splitlines():
        key, _, value = line.partition("=")
        if key == "cname":
            alias, target = value.split(",")
            aliases[alias] = target
        elif key == "host-record":
            addressed.add(value.split(",")[0])
    names = [host]
    while names[-1] in aliases:
        names.append(aliases[names[-1]])
    if names[-1] not in addressed:
        raise AssertionError(f"{host} ends at {names[-1]}, which has no address")
    return names


class PlanTest(unittest.TestCase):

    def setUp(self):
        self.plan = runtime.pageload_plan(loads=10)

    def test_each_case_loads_ten_times_in_each_mode(self):
        for case in runtime.PAGELOAD_CASES:
            loads = measured(self.plan, case)
            self.assertEqual(sum(load.mode == "on" for load in loads), 10)
            self.assertEqual(sum(load.mode == "off" for load in loads), 10)

    def test_the_modes_alternate(self):
        for case in runtime.PAGELOAD_CASES:
            modes = [load.mode for load in measured(self.plan, case)]
            self.assertTrue(all(a != b for a, b in zip(modes, modes[1:])), modes)

    def test_every_page_carries_forty_images(self):
        for load in self.plan:
            self.assertEqual(len(load.images), runtime.PAGELOAD_IMAGES)

    def test_each_case_warms_up_both_modes_before_it_measures(self):
        for case in runtime.PAGELOAD_CASES:
            loads = [load for load in self.plan if load.case == case]
            warm = [load for load in loads if not load.measured]
            self.assertEqual({load.mode for load in warm}, {"on", "off"})
            first_measured = next(i for i, load in enumerate(loads) if load.measured)
            self.assertTrue(all(not load.measured for load in loads[:first_measured]))
            self.assertTrue(all(load.measured for load in loads[first_measured:]))

    def test_the_worst_case_reaches_forty_hosts_it_has_never_seen(self):
        seen = set()
        for load in (load for load in self.plan if load.case == "fresh"):
            hosts = {runtime.host_of(image) for image in load.images}
            self.assertEqual(len(hosts), 40)
            self.assertFalse(hosts & seen, "a host came back on a later load")
            seen |= hosts

    def test_the_common_case_reaches_the_same_four_hosts_every_time(self):
        loads = [load for load in self.plan if load.case == "known"]
        first = {runtime.host_of(image) for image in loads[0].images}
        self.assertEqual(len(first), 4)
        for load in loads:
            self.assertEqual({runtime.host_of(image) for image in load.images}, first)

    def test_no_two_loads_ask_for_the_same_image(self):
        addresses = [image for load in self.plan for image in load.images]
        self.assertEqual(len(addresses), len(set(addresses)))

    def test_the_images_are_on_another_site_than_the_page(self):
        for load in self.plan:
            page_site = ".".join(runtime.host_of(load.address(8000)).split(".")[-2:])
            for image in load.images:
                self.assertFalse(runtime.host_of(image).endswith(page_site), image)

    def test_the_off_page_is_the_site_blocking_is_switched_off_for(self):
        for load in self.plan:
            host = runtime.host_of(load.address(8000))
            self.assertEqual(host == runtime.PAGELOAD_OFF_HOST, load.mode == "off")


class ZoneTest(unittest.TestCase):

    def setUp(self):
        self.plan = runtime.pageload_plan(loads=10)
        self.zone = runtime.pageload_zone(self.plan)

    def test_every_image_host_resolves_through_a_cname_chain(self):
        for load in self.plan:
            for image in load.images:
                names = chain(self.zone, runtime.host_of(image))
                self.assertGreaterEqual(len(names), 3, names)

    def test_the_pages_and_the_readiness_probe_resolve(self):
        for host in (runtime.PAGELOAD_ON_HOST, runtime.PAGELOAD_OFF_HOST,
                     runtime.PAGELOAD_PROBE_HOST):
            chain(self.zone, host)

    def test_the_server_answers_nothing_it_was_not_given(self):
        self.assertIn("no-resolv", self.zone.splitlines())
        self.assertIn("local=/test/", self.zone.splitlines())


class SummaryTest(unittest.TestCase):

    def test_the_difference_is_between_the_two_medians(self):
        on = [70.0, 75.0, 90.0, 71.0, 74.0]
        off = [50.0, 52.0, 51.0, 80.0, 53.0]
        summary = runtime.summarise_pageload(on, off)
        self.assertEqual(summary.on, statistics.median(on))
        self.assertEqual(summary.off, statistics.median(off))
        self.assertEqual(summary.added, statistics.median(on) - statistics.median(off))

    def test_blocking_that_costs_nothing_can_come_out_below_zero(self):
        self.assertLess(runtime.summarise_pageload([50.0, 51.0], [52.0, 53.0]).added, 0)


if __name__ == "__main__":
    unittest.main()
