#!/usr/bin/env python3
"""The page-load budget's plan, zone and arithmetic, without a browser.

What `scripts/benchmark_runtime.py pageload` measures is decided before any browser starts: which
pages load in which order, which hosts each one reaches, and what the local DNS server answers for
them. Those are the parts that can be wrong and still produce a number, so they are checked here.
The launch itself needs a Wayland session and a DNS server and runs in CI's budget step.
"""

from __future__ import annotations

import json
import os
import statistics
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import benchmark_runtime as runtime  # noqa: E402


def measured(plan, case):
    return [load for load in plan if load.case == case and load.measured and not load.spare]


def sequence(plan, case):
    return [load for load in plan if load.case == case and not load.spare]


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
            loads = sequence(self.plan, case)
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

    # The procedural case is the common one with rules to apply: the same four hosts, so the two
    # differ by what the rules cost and nothing else.
    def test_the_procedural_case_reaches_the_common_cases_four_hosts(self):
        known = {runtime.host_of(image)
                 for image in next(l for l in self.plan if l.case == "known").images}
        for load in (load for load in self.plan if load.case == "procedural"):
            self.assertEqual({runtime.host_of(image) for image in load.images}, known)

    def test_every_case_and_mode_has_spares_to_repeat_a_load_with(self):
        for case in runtime.PAGELOAD_CASES:
            for mode in ("on", "off"):
                spares = [load for load in self.plan
                          if load.spare and load.case == case and load.mode == mode]
                self.assertEqual(len(spares), runtime.PAGELOAD_SPARES)
                self.assertTrue(all(load.measured for load in spares))

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


class RepeatTest(unittest.TestCase):
    """A load the engine cut short is not counted, and a spare of the same kind takes its place."""

    def setUp(self):
        self.plan = runtime.pageload_plan(loads=2)
        self.site = runtime.PageLoadSite(self.plan)
        self.addCleanup(self.site.server.server_close)

    def number_of(self, address):
        return int(address.rsplit("/", 1)[1])

    def report(self, number, missing=()):
        return self.site.receive({"number": number, "milliseconds": 50.0,
                                  "missing": list(missing), "failed": len(missing)})

    def test_a_complete_load_moves_on_to_the_next(self):
        self.assertEqual(self.number_of(self.site.receive({"ready": True})), 0)
        self.assertEqual(self.number_of(self.report(0)), 1)

    def test_a_short_load_is_repeated_with_a_spare_of_its_case_and_mode(self):
        self.site.receive({"ready": True})
        self.report(0)
        self.report(1)
        spare = self.plan[self.number_of(self.report(2, ["http://lost/image.png"]))]
        self.assertTrue(spare.spare)
        self.assertEqual((spare.case, spare.mode), (self.plan[2].case, self.plan[2].mode))
        self.assertEqual(self.number_of(self.report(spare.number)), 3)
        self.assertEqual(self.site.repeated, [2])
        self.assertEqual([load.number for load in self.site.counted("fresh", "on")],
                         [spare.number, 4])

    def test_running_out_of_spares_fails_the_run(self):
        self.site.receive({"ready": True})
        self.report(0)
        self.report(1)
        answer = self.report(2, ["http://lost/image.png"])
        for _ in range(runtime.PAGELOAD_SPARES):
            answer = self.report(self.number_of(answer), ["http://lost/image.png"])
        self.assertIsNone(answer)
        self.assertIn("spare", self.site.problem)


class SeedTest(unittest.TestCase):

    def test_blocking_is_switched_off_for_the_off_host_only(self):
        with tempfile.TemporaryDirectory() as root:
            runtime.seed_content_blocking(root)
            with open(os.path.join(root, "content-blocking", "settings.json"),
                      encoding="utf-8") as handle:
                settings = json.load(handle)
            self.assertEqual(settings["disabledSites"], [runtime.PAGELOAD_OFF_HOST])

    # The readiness probe still refuses its host, and the procedural rules are the fixture's, one
    # per operator and action, written for both page hosts so the off page carries them too and
    # only the per-site switch tells the two apart.
    def test_the_user_rules_add_the_procedural_fixture_to_the_probe(self):
        with tempfile.TemporaryDirectory() as root:
            runtime.seed_content_blocking(root)
            with open(os.path.join(root, "content-blocking", "settings.json"),
                      encoding="utf-8") as handle:
                rules = json.load(handle)["userRules"].splitlines()
        fixture = json.loads(runtime.PROCEDURAL_RULES.read_text(encoding="utf-8"))
        self.assertEqual(rules[0], f"||{runtime.PAGELOAD_PROBE_HOST}^")
        self.assertEqual(len(rules), 1 + len(fixture))
        sites = f"{runtime.PAGELOAD_ON_HOST},{runtime.PAGELOAD_OFF_HOST}"
        for rule, row in zip(rules[1:], fixture):
            self.assertTrue(rule.startswith(sites + "#"), rule)
            self.assertTrue(row["rule"].endswith(rule[len(sites):]), rule)


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

    def test_every_name_is_one_https_only_mode_leaves_on_plain_http(self):
        for line in self.zone.splitlines():
            key, _, value = line.partition("=")
            if key in ("cname", "host-record"):
                self.assertTrue(value.split(",")[0].endswith(".test"), line)

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
