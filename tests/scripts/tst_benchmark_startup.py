#!/usr/bin/env python3
"""What the startup budgets read, serve and hold a launch to, without a browser.

`scripts/benchmark_runtime.py startup` and `livedin` launch the browser and take each launch apart
into the phases it marks on its way to the first frame. Reading those marks, the pages that fill
the lived-in profile and the ceiling the lived-in launch is held to are decided by code that needs
no browser, so they are checked here.
"""

from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
import urllib.request
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import benchmark_runtime as runtime  # noqa: E402


class PhasesTest(unittest.TestCase):
    """The browser's marks share a log with the Wayland protocol and the engine's own lines."""

    LOG = [
        "[2884206:2884206:1006/190212.123:ERROR:gpu_init.cc(1)] phase main at 1791300000000\n",
        "omaweb.startup: phase main at 1791300000100\n",
        "[16:04:57.208990] {Default Queue} wl_surface#20.commit()\n",
        "omaweb.startup: phase qt-started at 1791300000175\n",
        "omaweb.startup: phase first-frame at 1791300001250\n",
        "omaweb.startup: phase first-frame at 1791300001300\n",
    ]

    def test_each_phase_is_read_against_the_launch_on_the_wall_clock(self):
        phases = runtime.startup_phases(self.LOG, 1791300000.0)
        self.assertAlmostEqual(phases["qt-started"], 0.175, places=4)

    # A frame is drawn many times; the phase is the first.
    def test_a_phase_is_its_first_mark(self):
        phases = runtime.startup_phases(self.LOG, 1791300000.0)
        self.assertAlmostEqual(phases["first-frame"], 1.25, places=4)

    # The engine writes to the same stderr, and what a page logs could say anything.
    def test_only_the_browsers_own_category_marks_a_phase(self):
        phases = runtime.startup_phases(self.LOG, 1791300000.0)
        self.assertAlmostEqual(phases["main"], 0.1, places=4)

    def test_the_table_gives_each_phase_its_median_and_spread_in_launch_order(self):
        rows = runtime.phase_table([
            {"main": 0.10, "first-frame": 1.20},
            {"main": 0.12, "first-frame": 1.40},
            {"main": 0.11, "first-frame": 1.30},
        ])
        self.assertEqual(rows[0].split(), ["main", "110", "ms", "(100", "to", "120)"])
        named = [row.split()[0] for row in rows]
        self.assertEqual(named, list(runtime.STARTUP_PHASES))
        self.assertIn("not reached", rows[named.index("rules-compiled")])

    def test_the_browser_is_asked_for_its_marks_on_stderr(self):
        with tempfile.TemporaryDirectory() as directory:
            browser = runtime.Browser("omaweb", directory)
            with mock.patch.object(runtime.subprocess, "Popen") as popen, \
                    mock.patch.object(browser, "_await_mapping", return_value=0.0), \
                    mock.patch.dict(os.environ, {"QT_LOGGING_RULES": "qt.qpa.*=true",
                                                 "QT_MESSAGE_PATTERN": "%{message}"}):
                browser.start("")
                browser.sink.close()
        environment = popen.call_args.kwargs["env"]
        self.assertEqual(environment["QT_FORCE_STDERR_LOGGING"], "1")
        self.assertNotIn("QT_MESSAGE_PATTERN", environment)
        self.assertEqual(environment["QT_LOGGING_RULES"], "qt.qpa.*=true;omaweb.startup.info=true")


class LivedInSiteTest(unittest.TestCase):
    """The lived-in profile is filled by a page the warm-up launch restores, and only then."""

    def setUp(self):
        self.site = runtime.LivedInSite()
        self.site.start()
        self.addCleanup(self.site.stop)

    def fetch(self, path):
        with urllib.request.urlopen(self.site.address + path.lstrip("/")) as response:
            return response.read(), response.headers

    def test_the_restored_tab_fills_the_profile_only_while_warming(self):
        self.site.warming = True
        warming, _ = self.fetch(runtime.LIVED_IN_WARMING_PATH)
        self.site.warming = False
        resting, _ = self.fetch(runtime.LIVED_IN_WARMING_PATH)
        self.assertIn(runtime.LIVED_IN_WARMED.encode(), warming)
        self.assertNotIn(b"<script>", resting)

    def test_the_warming_page_asks_for_each_kind_of_storage_at_its_size(self):
        page = runtime.lived_in_warming_page()
        sizes = runtime.LIVED_IN_ENGINE_MIB
        self.assertIn(f"i < {sizes['cache']}; i++) await (await fetch(`/blob/cached-", page)
        self.assertIn(f"i < {sizes['service worker']}; i++) await stored.add(`/blob/stored-", page)
        self.assertIn(f"i < {sizes['IndexedDB']}; i++)", page)
        self.assertIn("new Uint8Array(1024 * 1024)", page)

    # What the HTTP cache keeps is what a reader's would: a response it may reuse. The service
    # worker's copies are its own, so the cache is not asked to keep them twice.
    def test_the_cache_keeps_its_blobs_and_not_the_service_workers(self):
        cached, cached_headers = self.fetch("/blob/cached-1")
        _, stored_headers = self.fetch("/blob/stored-1")
        self.assertEqual(len(cached), 1024 * 1024)
        self.assertEqual(cached_headers["Cache-Control"], runtime.LIVED_IN_CACHED)
        self.assertEqual(stored_headers["Cache-Control"], "no-store")

    def test_any_other_address_is_a_page_that_asks_for_nothing(self):
        body, headers = self.fetch("/lived-in-work/away/3")
        self.assertTrue(headers["Content-Type"].startswith("text/html"))
        self.assertNotIn(b"<script", body)
        self.assertNotIn(b"src=", body)


class CeilingTest(unittest.TestCase):
    """The lived-in launch is held to the budget's ceiling, and crossing it fails the run."""

    def setUp(self):
        self.budget = runtime.load_budget()
        self.ceiling = self.budget["measurements"]["startup_lived_in_seconds"]["ceiling"]

    def test_a_launch_over_the_ceiling_is_reported_crossed(self):
        with mock.patch.object(runtime, "log"):
            crossed = runtime.report({"startup_lived_in_seconds": self.ceiling + 0.01},
                                     self.budget)
        self.assertEqual(crossed, 1)

    def test_a_launch_at_the_ceiling_is_within_it(self):
        with mock.patch.object(runtime, "log"):
            crossed = runtime.report({"startup_lived_in_seconds": self.ceiling}, self.budget)
        self.assertEqual(crossed, 0)

    def test_a_crossed_ceiling_fails_the_run(self):
        arguments = ["benchmark_runtime.py", "livedin", "--browser", sys.executable]
        with mock.patch.object(sys, "argv", arguments), \
                mock.patch.dict(os.environ, {"WAYLAND_DISPLAY": "wayland-test"}), \
                mock.patch.dict(runtime.MEASUREMENTS, {"livedin": lambda _: {
                    "startup_lived_in_seconds": self.ceiling * 2}}), \
                mock.patch.object(runtime, "log"):
            self.assertEqual(runtime.main(), 1)

    def test_the_budget_says_what_the_ceiling_was_set_from(self):
        entry = self.budget["measurements"]["startup_lived_in_seconds"]
        self.assertGreater(entry["ceiling"], entry["recorded"])
        self.assertIn("lived-in", entry["note"])


class SeederTest(unittest.TestCase):
    """The session is the store's to write, so the seeder is found rather than reimplemented."""

    def test_the_seeder_beside_the_browser_comes_first(self):
        with tempfile.TemporaryDirectory() as directory:
            seeder = Path(directory) / "omaweb-lived-in-session"
            seeder.write_text("#!/bin/sh\n", encoding="utf-8")
            seeder.chmod(0o755)
            found = runtime.lived_in_seeder(str(Path(directory) / "omaweb"), "")
        self.assertEqual(found, str(seeder))

    def test_without_one_the_measurement_is_unavailable_rather_than_failed(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(runtime, "ROOT", Path(directory)):
            with self.assertRaises(runtime.Unavailable):
                runtime.lived_in_seeder(str(Path(directory) / "omaweb"), "")


class RequireLivedInTest(unittest.TestCase):
    """CI asks for the lived-in launch, so a build without the seeder fails there rather than
    passing with one measurement fewer."""

    def test_a_missing_seeder_fails_the_measurement_when_it_is_required(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(runtime, "ROOT", Path(directory)):
            with self.assertRaises(runtime.MeasurementFailed):
                runtime.measure_lived_in(str(Path(directory) / "omaweb"), 1, "", required=True)

    def test_without_it_a_missing_seeder_is_a_skip(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(runtime, "ROOT", Path(directory)):
            with self.assertRaises(runtime.Unavailable):
                runtime.measure_lived_in(str(Path(directory) / "omaweb"), 1, "")


class EvictTest(unittest.TestCase):
    """A cold launch reads what the browser maps, so that is what is dropped."""

    def test_every_file_under_a_directory_is_dropped(self):
        with tempfile.TemporaryDirectory() as directory:
            nested = Path(directory) / "spaces" / "one"
            nested.mkdir(parents=True)
            (nested / "browser.sqlite").write_bytes(b"x")
            (Path(directory) / "state.sqlite").write_bytes(b"y")
            with mock.patch.object(runtime.os, "posix_fadvise") as advise:
                runtime.evict([directory, "/nonexistent/file"])
        self.assertEqual(advise.call_count, 2)
        self.assertTrue(all(call.args[3] == os.POSIX_FADV_DONTNEED for call in advise.mock_calls))

    def test_the_files_a_process_maps_are_its_paths_only(self):
        maps = ("7f00-7f01 r-xp 00000000 08:01 12 /usr/lib/libQt6Core.so.6.11.2\n"
                "7f01-7f02 rw-p 00000000 00:00 0 [heap]\n"
                "7f02-7f03 r--p 00000000 08:01 13 /home/reader/state.sqlite-shm (deleted)\n"
                "7f03-7f04 rw-p 00000000 00:00 0\n")
        with mock.patch.object(runtime, "children_by_parent", return_value={}), \
                mock.patch("builtins.open", mock.mock_open(read_data=maps)):
            files = runtime.mapped_files(42)
        self.assertEqual(files, {"/usr/lib/libQt6Core.so.6.11.2",
                                 "/home/reader/state.sqlite-shm"})


if __name__ == "__main__":
    unittest.main()
