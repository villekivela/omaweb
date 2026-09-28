#!/usr/bin/env python3
"""The Chromium comparison's plan, arithmetic, history and plot, without a browser.

What `scripts/benchmark_chromium.py` reports is decided by code that runs no browser: the order the
runs happen in, the ratio each suite comes to, which Chromium build is the engine's match, what a
history line holds and what the plot draws from it. Those are the parts that can be wrong and still
print a number. The runs themselves need a Wayland session and both browsers.
"""

from __future__ import annotations

import json
import os
import re
import socket
import sys
import tempfile
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import benchmark_chromium as compare  # noqa: E402
import performance_history as history  # noqa: E402

BROWSERS = [history.OMAWEB, history.LATEST, history.MATCHED]

ENGINE = {"library": "/usr/lib/omaweb/lib/libQt6WebEngineCore.so.6", "package":
          "omaweb-qtwebengine", "version": "6.11.2-3", "qtwebengine": "6.11.2",
          "chromium": "140.0.7339.225"}


def comparison(date, machine="vm", engine=ENGINE, matched="140.0.7339.186", no_gpu=False,
               scores=None):
    browsers = {
        history.OMAWEB: {"version": "0.8.0", "flags": "", "gpu": {}, "no_gpu": no_gpu},
        history.LATEST: {"version": "153.0.8010.36", "flags": "", "gpu": {}, "no_gpu": False},
        history.MATCHED: {"version": matched, "flags": "", "gpu": {}, "no_gpu": False},
    }
    scores = scores or {"speedometer": {history.OMAWEB: [22.0, 23.0],
                                        history.LATEST: [24.0, 25.0],
                                        history.MATCHED: [23.0, 23.0]},
                        "motionmark": {history.OMAWEB: [100.0], history.LATEST: [200.0],
                                       history.MATCHED: [150.0]}}
    return compare.comparison_record(scores, browsers, suites=list(scores), engine=engine,
                                     commit="abc", omaweb="0.8.0", machine=machine, date=date,
                                     approved="140.0.7339.225")


def budget(date, machine="vm", engine=ENGINE, startup=0.7, ceiling=3.0):
    return history.budget_record(
        {"startup_seconds": startup},
        {"measurements": {"startup_seconds": {"ceiling": ceiling, "recorded": 0.5}}},
        date=date, commit="abc", machine=machine, engine=engine, omaweb="0.8.0")


class PlanTest(unittest.TestCase):

    def setUp(self):
        self.plan = compare.plan(list(compare.SUITES), BROWSERS, 3)

    def test_every_browser_runs_every_suite_the_asked_number_of_times(self):
        for suite in compare.SUITES:
            for browser in BROWSERS:
                runs = [run for run in self.plan if run.suite == suite and run.browser == browser]
                self.assertEqual([run.number for run in runs], [0, 1, 2])

    def test_no_browser_runs_twice_in_a_row(self):
        for suite in compare.SUITES:
            order = [run.browser for run in self.plan if run.suite == suite]
            self.assertTrue(all(a != b for a, b in zip(order, order[1:])), order)

    # A fixed order would put the same browser straight after the same one in every round, and a
    # browser that leaves the machine busy would always charge it to the same neighbour.
    def test_each_round_starts_with_a_different_browser(self):
        firsts = [run.browser for run in self.plan
                  if run.suite == "speedometer" and run == next(
                      other for other in self.plan
                      if other.suite == run.suite and other.number == run.number)]
        self.assertEqual(firsts, BROWSERS)

    def test_the_suites_run_one_after_another(self):
        suites = [run.suite for run in self.plan]
        self.assertEqual(suites, sorted(suites, key=list(compare.SUITES).index))


class RatioTest(unittest.TestCase):

    def test_a_ratio_is_the_mean_of_omawebs_runs_over_the_mean_of_chromiums(self):
        self.assertAlmostEqual(history.ratio([22.0, 23.0], [24.0, 26.0]), 22.5 / 25.0)

    def test_each_suite_has_a_ratio_against_each_baseline(self):
        ratios = history.comparison_ratios(comparison("2026-09-28T10:00:00Z"))
        self.assertAlmostEqual(ratios["speedometer"][history.LATEST], 22.5 / 24.5)
        self.assertAlmostEqual(ratios["speedometer"][history.MATCHED], 22.5 / 23.0)
        self.assertAlmostEqual(ratios["motionmark"][history.MATCHED], 100.0 / 150.0)

    def test_a_baseline_that_did_not_run_has_no_ratio(self):
        record = comparison("2026-09-28T10:00:00Z", scores={
            "speedometer": {history.OMAWEB: [22.0], history.LATEST: [24.0]}})
        self.assertEqual(set(history.comparison_ratios(record)["speedometer"]), {history.LATEST})

    def test_the_spread_is_the_range_over_the_mean(self):
        self.assertAlmostEqual(history.spread([24.0, 25.0, 26.0]), 2.0 / 25.0)
        self.assertEqual(history.spread([24.5]), 0.0)


class ReleaseTest(unittest.TestCase):
    """The matched Chromium is the newest Playwright release on the engine's major."""

    RELEASES = {"v1.54.0": "139.0.7258.5", "v1.54.2": "139.0.7258.154",
                "v1.55.0": "140.0.7339.16", "v1.55.1": "140.0.7339.186",
                "v1.56.0": "141.0.7390.37", "v1.9.0": None, "v1.57.0": "142.0.7444.0"}

    def setUp(self):
        self.asked = []

    def chromium_of(self, tag):
        self.asked.append(tag)
        version = self.RELEASES[tag]
        return None if version is None else {"browserVersion": version, "revision": tag}

    def test_the_newest_release_of_the_major_is_chosen(self):
        tag, entry = compare.find_release(list(self.RELEASES), 140, self.chromium_of)
        self.assertEqual((tag, entry["browserVersion"]), ("v1.55.1", "140.0.7339.186"))

    def test_releases_sort_by_version_rather_than_as_text(self):
        self.assertEqual(sorted(["v1.10.0", "v1.9.0"], key=compare.release_order),
                         ["v1.9.0", "v1.10.0"])

    def test_a_major_no_release_carries_is_none(self):
        self.assertIsNone(compare.find_release(list(self.RELEASES), 120, self.chromium_of))
        self.assertIsNone(compare.find_release(list(self.RELEASES), 150, self.chromium_of))

    def test_it_reads_a_handful_of_manifests_rather_than_all_of_them(self):
        tags = [f"v1.{minor}.0" for minor in range(200)]

        def chromium_of(tag):
            self.asked.append(tag)
            return {"browserVersion": f"{80 + int(tag.split('.')[1]) // 3}.0", "revision": "1"}

        tag, _ = compare.find_release(tags, 140, chromium_of)
        self.assertEqual(tag, "v1.182.0")
        self.assertLessEqual(len(self.asked), 10)

    def test_the_engine_major_is_read_from_its_version(self):
        self.assertEqual(compare.major("140.0.7339.225"), 140)


class NotesTest(unittest.TestCase):

    def test_an_engine_off_the_approved_baseline_is_named(self):
        record = comparison("2026-09-28T10:00:00Z")
        record["engine"] = dict(ENGINE, chromium="141.0.1.2")
        self.assertTrue(any("security/baseline.json approves 140.0.7339.225" in note
                            for note in compare.notes(record, "140.0.7339.225")))

    def test_an_engine_on_the_approved_baseline_has_no_note(self):
        record = comparison("2026-09-28T10:00:00Z")
        record["no_gpu"] = []
        self.assertEqual(compare.notes(record, "140.0.7339.225"), [])

    def test_a_matched_chromium_of_another_major_is_named(self):
        record = comparison("2026-09-28T10:00:00Z", matched="139.0.1.1")
        self.assertTrue(any("not the engine's major" in note
                            for note in compare.notes(record, "140.0.7339.225")))


class GpuTest(unittest.TestCase):

    def test_motionmark_without_gpu_compositing_is_marked(self):
        self.assertEqual(comparison("2026-09-28T10:00:00Z", no_gpu=True)["no_gpu"],
                         ["motionmark"])
        self.assertEqual(comparison("2026-09-28T10:00:00Z")["no_gpu"], [])

    def test_the_report_says_a_marked_run_is_not_a_result(self):
        lines = compare.report_lines(comparison("2026-09-28T10:00:00Z", no_gpu=True))
        self.assertTrue(any("MotionMark" in line and "not a result" in line for line in lines))

    def test_flags_that_turn_the_gpu_off_count_as_software(self):
        self.assertTrue(compare.is_software({}, "--use-gl=egl --disable-gpu-compositing"))
        self.assertTrue(compare.is_software({"gpu_compositing": "enabled"}, "--disable-gpu"))
        self.assertFalse(compare.is_software({}, "--use-gl=egl"))

    def test_chromiums_own_word_decides_when_the_flags_do_not(self):
        self.assertTrue(compare.is_software({"gpu_compositing": "disabled_software"}, ""))
        self.assertFalse(compare.is_software({"gpu_compositing": "enabled"}, ""))
        self.assertFalse(compare.is_software({}, ""))


class LaunchTest(unittest.TestCase):

    def test_omaweb_gets_scratch_roots_and_its_own_debugging_option(self):
        spec = compare.BrowserSpec(history.OMAWEB, "/usr/bin/omaweb", "--use-gl=egl")
        command, environment = compare.command_line(spec, "/tmp/run", 9300, "http://x/")
        self.assertIn("--remote-debugging=9300", command)
        self.assertEqual(environment["OMAWEB_DATA_ROOT"], "/tmp/run/data")
        self.assertEqual(environment["OMAWEB_CONFIG_ROOT"], "/tmp/run/config")
        self.assertEqual(environment["QTWEBENGINE_CHROMIUM_FLAGS"], "--use-gl=egl")
        self.assertEqual(command[:2], ["dbus-run-session", "--"])

    # Arch's launcher reads chromium-flags.conf from the configuration home, and a reader's can
    # load extensions into what is meant to be a fresh profile.
    def test_chromium_reads_no_flags_file_of_the_readers(self):
        spec = compare.BrowserSpec(history.LATEST, "/usr/bin/chromium", "--use-gl=egl")
        command, environment = compare.command_line(spec, "/tmp/run", 9301, "http://x/")
        self.assertEqual(environment["XDG_CONFIG_HOME"], "/tmp/run/config")
        self.assertIn("--user-data-dir=/tmp/run/profile", command)
        self.assertIn("--remote-debugging-port=9301", command)
        self.assertIn("--use-gl=egl", command)
        self.assertEqual(command[-1], "http://x/")


class HistoryTest(unittest.TestCase):

    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.path = Path(directory.name) / "history.jsonl"

    def test_each_run_is_one_line_that_reads_on_its_own(self):
        history.append(comparison("2026-09-28T10:00:00Z"), self.path)
        history.append(budget("2026-09-28T11:00:00Z"), self.path)
        lines = self.path.read_text().splitlines()
        self.assertEqual(len(lines), 2)
        for line in lines:
            record = json.loads(line)
            for field in ("kind", "date", "commit", "machine", "engine"):
                self.assertIn(field, record)
            self.assertEqual(record["engine"]["package"], "omaweb-qtwebengine")

    def test_a_comparison_carries_every_score_and_each_chromiums_version(self):
        record = comparison("2026-09-28T10:00:00Z")
        self.assertEqual(record["kind"], history.KIND_COMPARISON)
        self.assertEqual(record["scores"]["speedometer"][history.LATEST], [24.0, 25.0])
        self.assertEqual(record["browsers"][history.MATCHED]["version"], "140.0.7339.186")
        self.assertEqual(record["browsers"][history.LATEST]["version"], "153.0.8010.36")
        self.assertIn("flags", record["browsers"][history.OMAWEB])

    def test_a_budget_line_keeps_the_ceiling_it_was_held_to(self):
        record = budget("2026-09-28T10:00:00Z", startup=0.6912)
        self.assertEqual(record["kind"], history.KIND_BUDGET)
        self.assertEqual(record["measurements"]["startup_seconds"],
                         {"value": 0.691, "ceiling": 3.0})

    def test_lines_are_read_back_oldest_first(self):
        history.append(budget("2026-09-29T10:00:00Z"), self.path)
        history.append(budget("2026-09-28T10:00:00Z"), self.path)
        self.assertEqual([record["date"] for record in history.read(self.path)],
                         ["2026-09-28T10:00:00Z", "2026-09-29T10:00:00Z"])

    def test_a_damaged_line_fails_rather_than_leaving_a_gap(self):
        self.path.write_text('{"kind": "budget"\n')
        with self.assertRaises(ValueError):
            history.read(self.path)

    def test_a_missing_history_is_empty(self):
        self.assertEqual(history.read(self.path), [])

    def test_the_committed_history_reads(self):
        for record in history.read():
            self.assertIn(record["kind"], (history.KIND_BUDGET, history.KIND_COMPARISON))
            self.assertTrue(record["machine"])


class PlotDataTest(unittest.TestCase):

    def setUp(self):
        updated = dict(ENGINE, version="6.12.0-1", chromium="142.0.1.1")
        self.records = [
            comparison("2026-09-28T10:00:00Z"),
            comparison("2026-09-28T12:00:00Z", machine="laptop"),
            budget("2026-09-28T13:00:00Z"),
            comparison("2026-10-10T10:00:00Z", engine=updated, matched="142.0.7444.0"),
            budget("2026-10-10T11:00:00Z", engine=updated, startup=0.8, ceiling=2.0),
        ]

    def test_each_suite_has_a_series_per_machine_and_baseline(self):
        series = history.comparison_series(self.records)["speedometer"]
        self.assertEqual({(line["machine"], line["baseline"]) for line in series},
                         {("vm", history.LATEST), ("vm", history.MATCHED),
                          ("laptop", history.LATEST), ("laptop", history.MATCHED)})

    def test_a_point_is_the_runs_ratio_with_its_raw_scores(self):
        vm = next(line for line in history.comparison_series(self.records)["speedometer"]
                  if line["machine"] == "vm" and line["baseline"] == history.LATEST)
        self.assertEqual(len(vm["points"]), 2)
        self.assertAlmostEqual(vm["points"][0]["value"], 22.5 / 24.5)
        self.assertEqual(vm["points"][0]["scores"][history.OMAWEB], [22.0, 23.0])

    def test_the_matched_baseline_names_the_version_each_point_was_measured_against(self):
        vm = next(line for line in history.comparison_series(self.records)["speedometer"]
                  if line["machine"] == "vm" and line["baseline"] == history.MATCHED)
        self.assertEqual([point["chromium"] for point in vm["points"]],
                         ["140.0.7339.186", "142.0.7444.0"])

    def test_an_engine_update_is_marked_where_it_happened_on_its_machine(self):
        marks = history.engine_updates(self.records)
        self.assertEqual(marks, [{"date": "2026-10-10T10:00:00Z", "machine": "vm",
                                  "engine": "omaweb-qtwebengine 6.12.0-1"}])

    def test_a_second_machines_first_line_is_not_an_update(self):
        self.assertFalse(any(mark["machine"] == "laptop"
                             for mark in history.engine_updates(self.records)))

    def test_each_budget_measurement_has_its_value_beside_its_ceiling_over_time(self):
        series = history.budget_series(self.records)["startup_seconds"]
        self.assertEqual(len(series), 1)
        self.assertEqual([(point["value"], point["ceiling"]) for point in series[0]["points"]],
                         [(0.7, 3.0), (0.8, 2.0)])

    def test_a_motionmark_point_without_a_gpu_is_marked(self):
        records = [comparison("2026-09-28T10:00:00Z", no_gpu=True)]
        line = history.comparison_series(records)["motionmark"][0]
        self.assertTrue(line["points"][0]["no_gpu"])
        line = history.comparison_series(records)["speedometer"][0]
        self.assertFalse(line["points"][0]["no_gpu"])


class PageTest(unittest.TestCase):

    def setUp(self):
        self.page = history.render([
            comparison("2026-09-28T10:00:00Z"),
            comparison("2026-09-28T12:00:00Z", machine="laptop"),
            budget("2026-09-28T13:00:00Z"),
            budget("2026-10-01T13:00:00Z", engine=dict(ENGINE, version="6.12.0-1")),
        ])

    def test_it_draws_a_chart_per_suite_and_per_measurement(self):
        self.assertEqual(self.page.count("<svg class=\"chart\""), 3)
        for heading in ("speedometer", "motionmark", "startup_seconds"):
            self.assertIn(f"<h3>{heading}</h3>", self.page)

    def test_it_fetches_nothing(self):
        self.assertNotIn("<script", self.page)
        self.assertIsNone(re.search(r'(src|href)="(https?:)?//', self.page))

    def test_each_point_says_its_scores(self):
        self.assertIn("scores: omaweb 22, 23; latest 24, 25; matched 23, 23", self.page)

    def test_both_machines_are_in_the_legend(self):
        self.assertIn("laptop, latest stable Chromium", self.page)
        self.assertIn("vm, Chromium the engine is based on", self.page)

    def test_an_engine_update_is_drawn(self):
        self.assertIn("engine update: omaweb-qtwebengine 6.12.0-1 on vm", self.page)

    def test_an_empty_history_is_still_a_page(self):
        self.assertIn("no runs yet", history.render([]))


class ProfileTest(unittest.TestCase):

    def test_a_report_by_library(self):
        # The column is padded to its widest entry, and a JIT entry's name has spaces in it.
        text = ("# Samples: 30K of event 'task-clock:uppp'\n#\n"
                "    93.52%  chromium                \n"
                "     2.38%  [JIT] tid 734491        \n"
                "     0.68%  libharfbuzz.so.0.61450.0\n")
        self.assertEqual(compare.parse_perf_report(text), [
            (93.52, "chromium", ""),
            (2.38, "[JIT] tid 734491", ""),
            (0.68, "libharfbuzz.so.0.61450.0", ""),
        ])

    def test_a_report_by_library_and_symbol(self):
        text = ("     0.99%  libQt6WebEngineCore.so.6  [.] cppgc::internal::"
                "ConcurrentSweepTask::VisitNormalPage\n"
                "     2.05%  [kernel.kallsyms]         [k] preempt_count_sub\n"
                "     0.40%  [JIT] tid 99              [.] 0x0000ffff8a2c1000\n")
        self.assertEqual(compare.parse_perf_report(text), [
            (0.99, "libQt6WebEngineCore.so.6",
             "cppgc::internal::ConcurrentSweepTask::VisitNormalPage"),
            (2.05, "[kernel.kallsyms]", "preempt_count_sub"),
            (0.40, "[JIT] tid 99", "0x0000ffff8a2c1000"),
        ])


class RendererTest(unittest.TestCase):

    # The zygote rewrites a renderer's title into one argument, which is how /proc shows it.
    def test_a_renamed_renderer_is_found(self):
        self.assertTrue(compare.is_page_renderer(
            "/usr/lib/omaweb/lib/qt6/QtWebEngineProcess --type=renderer --lang=en "))

    def test_chromiums_own_interface_is_not_a_page(self):
        self.assertFalse(compare.is_page_renderer(
            "/usr/lib/chromium/chromium --type=renderer --top-chrome-webui --lang=en"))
        self.assertFalse(compare.is_page_renderer("/usr/lib/chromium/chromium --type=zygote"))


class DevToolsTest(unittest.TestCase):
    """The small part of WebSocket the DevTools client speaks."""

    def test_a_masked_frame_reads_back_as_its_text(self):
        for text in ("{}", "x" * 300, "y" * 70000):
            frame = compare.encode_frame(text, b"\x01\x02\x03\x04")
            buffer = bytearray(frame)

            def receive(count, buffer=buffer):
                taken = bytes(buffer[:count])
                del buffer[:count]
                return taken

            opcode, final, payload = compare.read_frame(receive)
            self.assertEqual((opcode, final, payload.decode()), (1, True, text))

    def test_a_call_is_answered_by_its_own_id(self):
        server = socket.socket()
        server.bind(("127.0.0.1", 0))
        server.listen(1)
        self.addCleanup(server.close)

        def answer():
            connection, _ = server.accept()
            with connection:
                request = b""
                while b"\r\n\r\n" not in request:
                    request += connection.recv(4096)
                connection.sendall(b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                                   b"Connection: Upgrade\r\n\r\n")
                buffer = bytearray()

                def receive(count):
                    while len(buffer) < count:
                        buffer.extend(connection.recv(65536))
                    taken = bytes(buffer[:count])
                    del buffer[:count]
                    return taken

                _, _, payload = compare.read_frame(receive)
                message = json.loads(payload)
                for reply in ({"method": "Page.loadEventFired"},
                              {"id": message["id"], "result": {"result": {"value": 24.5}}}):
                    body = json.dumps(reply).encode()
                    connection.sendall(bytes([0x81, len(body)]) + body)

        thread = threading.Thread(target=answer)
        thread.start()
        client = compare.DevTools(f"ws://127.0.0.1:{server.getsockname()[1]}/devtools/page/1")
        self.assertEqual(client.evaluate("score"), 24.5)
        client.close()
        thread.join()


class SuiteTest(unittest.TestCase):

    def test_every_suite_is_pinned_to_a_commit_and_a_digest(self):
        for suite in compare.SUITES.values():
            self.assertRegex(suite.commit, r"^[0-9a-f]{40}$")
            self.assertRegex(suite.sha256, r"^[0-9a-f]{64}$")
            self.assertIn(suite.commit, suite.archive)

    def test_the_suites_are_served_from_loopback(self):
        with tempfile.TemporaryDirectory() as root:
            server = compare.SuiteServer(Path(root))
            try:
                address = server.address(compare.SUITES["speedometer"])
            finally:
                server.stop()
        self.assertTrue(address.startswith("http://127.0.0.1:"))
        self.assertIn("/speedometer-1386415be8fe/index.html?startAutomatically", address)

    def test_a_zip_keeps_the_executable_bit(self):
        import zipfile
        with tempfile.TemporaryDirectory() as root:
            archive = Path(root) / "chrome.zip"
            with zipfile.ZipFile(archive, "w") as bundle:
                info = zipfile.ZipInfo("chrome-linux/chrome")
                info.external_attr = 0o755 << 16
                bundle.writestr(info, "#!/bin/sh\n")
            compare.extract_zip(archive, Path(root) / "out")
            self.assertTrue(os.access(Path(root) / "out" / "chrome-linux" / "chrome", os.X_OK))


if __name__ == "__main__":
    unittest.main()
