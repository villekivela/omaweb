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
import re
import statistics
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

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


class QuietTest(unittest.TestCase):
    """A load that never reports says how far it got, as far as the server saw."""

    def setUp(self):
        self.plan = runtime.pageload_plan(loads=2)
        self.site = runtime.PageLoadSite(self.plan)
        self.addCleanup(self.site.server.server_close)
        self.load = self.plan[0]

    def test_a_page_the_browser_never_asked_for(self):
        self.assertEqual(self.site.describe_quiet(self.load), "load 0 was never served")

    def test_a_page_served_with_some_of_its_images(self):
        self.site.answered.add(self.load.address(self.site.port))
        images = [runtime.with_port(image, self.site.port) for image in self.load.images]
        self.site.requested.update(images[:3])
        self.site.answered.update(images[:2])
        self.site.server.errors["ConnectionResetError"] = 1
        described = self.site.describe_quiet(self.load)
        self.assertIn("3 of its 40 images were asked of the server and 2 answered", described)
        self.assertIn("1 ConnectionResetError", described)
        self.assertIn("it never said how far it got", described)

    def test_a_page_that_said_how_far_it_got(self):
        self.site.answered.add(self.load.address(self.site.port))
        self.site.stalled[0] = {"number": 0, "readyState": "complete", "loadEventStart": 120.5,
                                "incomplete": [], "reporting": "sent"}
        described = self.site.describe_quiet(self.load)
        self.assertIn("document complete, load event at 120.5 ms, 0 images incomplete (none), "
                      "report sent", described)


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
    # One row's style for its own `#match` must not reach another row's: on one page, that made the
    # page with blocking off slower than with it on, measuring the fixture instead of the rules.
    def test_each_procedural_row_keeps_its_element_names_to_itself(self):
        _, markup = runtime.procedural_fixture()
        ids = re.findall(r'id="([^"]+)"', markup)
        self.assertEqual(len(ids), len(set(ids)), ids)
        self.assertNotIn("#match ", markup)
        self.assertNotIn("#match:", markup)

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


class ReportTest(unittest.TestCase):
    """The budget's verdict: each page-load difference beside its ceiling, in milliseconds."""

    def setUp(self):
        self.budget = {
            "machine": "a runner",
            "recorded_on": "2026-09-28",
            "measurements": {
                "pageload_fresh_hosts_milliseconds": {"ceiling": 72.8},
                "pageload_known_hosts_milliseconds": {"ceiling": 20.0},
            },
        }

    def reported(self, results):
        with mock.patch.object(runtime, "log") as log:
            crossed = runtime.report(results, self.budget)
        return crossed, [call.args[0] for call in log.call_args_list]

    def test_a_difference_past_its_ceiling_fails_the_run(self):
        crossed, lines = self.reported({"pageload_fresh_hosts_milliseconds": 80.0,
                                        "pageload_known_hosts_milliseconds": 4.0})
        self.assertEqual(crossed, 1)
        self.assertIn("CROSSED  pageload_fresh_hosts_milliseconds: 80.00 ms against 72.80 ms "
                      "(-7.20 ms of headroom)", lines)
        self.assertIn("within   pageload_known_hosts_milliseconds: 4.00 ms against 20.00 ms "
                      "(16.00 ms of headroom)", lines)

    def test_a_difference_at_its_ceiling_passes(self):
        crossed, _ = self.reported({"pageload_known_hosts_milliseconds": 20.0})
        self.assertEqual(crossed, 0)

    # Against the budget CI holds the browser to, so a first-paint ceiling missing from it, or one
    # the step does not compare, fails here rather than as a run that never says CROSSED.
    def test_a_page_that_paints_later_than_its_ceiling_fails_the_run(self):
        self.budget = runtime.load_budget()
        for case in runtime.FIRST_CONTENTFUL_PAINT_CASES:
            name = f"first_contentful_paint_{case}_hosts_milliseconds"
            ceiling = self.budget["measurements"][name]["ceiling"]
            crossed, lines = self.reported({name: ceiling + 1.0})
            self.assertEqual(crossed, 1, name)
            self.assertIn(f"CROSSED  {name}: {ceiling + 1.0:.2f} ms against {ceiling:.2f} ms "
                          "(-1.00 ms of headroom)", lines)
            crossed, _ = self.reported({name: ceiling})
            self.assertEqual(crossed, 0, name)


class FirstPaintTest(unittest.TestCase):
    """Each page's first contentful paint, from the blocking-on loads of the fresh and known cases."""

    def setUp(self):
        self.plan = runtime.pageload_plan(loads=3)
        self.site = runtime.PageLoadSite(self.plan)
        self.addCleanup(self.site.server.server_close)

    def run_plan(self, paint):
        """Reports every load as the browser would, each painting at `paint(load)`."""
        answer = self.site.receive({"ready": True})
        while answer:
            load = self.plan[int(answer.rsplit("/", 1)[1])]
            answer = self.site.receive({"number": load.number, "milliseconds": 100.0,
                                        "firstContentfulPaint": paint(load), "missing": [],
                                        "failed": 0})
        return runtime.pageload_results(self.site)

    def test_the_first_paint_is_the_median_of_each_cases_blocking_on_loads(self):
        paints = {"fresh": iter([40.0, 90.0, 60.0]), "known": iter([30.0, 20.0, 25.0])}

        def paint(load):
            # Every other load paints far later, so a median that took one in would show it.
            counted = load.measured and load.mode == "on" and load.case in paints
            return next(paints[load.case]) if counted else 1000.0

        results = self.run_plan(paint)
        self.assertEqual(results["first_contentful_paint_fresh_hosts_milliseconds"], 60.0)
        self.assertEqual(results["first_contentful_paint_known_hosts_milliseconds"], 25.0)
        self.assertNotIn("first_contentful_paint_procedural_hosts_milliseconds", results)

    # A page whose timeline never held the entry has no paint to hold to a ceiling, and a run that
    # dropped it would report the median of the pages that did.
    def test_a_counted_page_that_never_painted_fails_the_run(self):
        unpainted = next(load for load in self.plan
                         if load.case == "known" and load.measured and load.mode == "on")
        with self.assertRaisesRegex(runtime.MeasurementFailed,
                                    f"load {unpainted.number} .*no first contentful paint"):
            self.run_plan(lambda load: None if load is unpainted else 50.0)


class RecordTest(unittest.TestCase):
    """`--record` writes the budget back as one machine's measurements, not another's."""

    def test_the_budget_names_the_machine_that_recorded_it(self):
        budget = {
            "machine": "a runner",
            "recorded_on": "2026-09-28",
            "measurements": {"startup_seconds": {"ceiling": 3.0, "recorded": 0.69}},
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "budget.json"
            # `append` binds the history's path when it is defined, so it is replaced rather than
            # pointed elsewhere, or the test writes into the repository's history. `HISTORY` and
            # `ROOT` are still moved, because the closing log line names both paths.
            with mock.patch.object(runtime, "BUDGET", path), \
                    mock.patch.object(runtime, "ROOT", Path(directory)), \
                    mock.patch.object(runtime.history, "HISTORY", Path(directory) / "h.jsonl"), \
                    mock.patch.object(runtime.history, "append") as append, \
                    mock.patch.object(runtime.history, "read_version", return_value={}), \
                    mock.patch.object(runtime.history, "describe_engine", return_value={}), \
                    mock.patch.object(runtime.history, "omaweb_commit", return_value=""), \
                    mock.patch.object(runtime, "log"):
                runtime.record({"startup_seconds": 0.55}, budget, "omaweb", "a laptop")
            written = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(append.call_args.args[0]["machine"], "a laptop")
        self.assertEqual(written["machine"], "a laptop")
        self.assertEqual(written["measurements"]["startup_seconds"]["recorded"], 0.55)


class RequireDnsTest(unittest.TestCase):
    """Where no DNS server can run, pageload skips, unless CI said it may not."""

    def measure(self, require_dns):
        with mock.patch.object(runtime, "machine_serves_zone", return_value=False), \
                mock.patch.object(runtime.shutil, "which", return_value=None):
            runtime.measure_pageload("build/dev/omaweb", require_dns)

    def test_a_machine_without_the_tools_skips(self):
        with self.assertRaises(runtime.Unavailable):
            self.measure(require_dns=False)

    def test_require_dns_turns_the_skip_into_a_failure(self):
        with self.assertRaisesRegex(runtime.MeasurementFailed, "--require-dns"):
            self.measure(require_dns=True)


if __name__ == "__main__":
    unittest.main()
