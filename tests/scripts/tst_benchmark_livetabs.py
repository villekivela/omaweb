#!/usr/bin/env python3
"""What the live-tabs budget opens, serves and reads, without a browser.

`scripts/benchmark_runtime.py livetabs` opens tabs across Spaces, reads the process tree's memory
at each count, and reads the CPU the tree uses at rest. Which tabs go where, what each one is
served, and the arithmetic over the readings are decided by code that needs no browser, so they are
checked here.
"""

from __future__ import annotations

import sqlite3
import sys
import unittest
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import benchmark_runtime as runtime  # noqa: E402


class PlanTest(unittest.TestCase):
    """The tabs open in stages: each Space's first tab, then the rest of twenty, then of fifty,
    spread evenly over the Spaces."""

    def setUp(self):
        self.stages = runtime.live_tab_plan()

    def test_the_spaces_open_first_and_then_the_tabs_up_to_twenty_and_fifty(self):
        self.assertEqual([len(stage) for stage in self.stages], [5, 15, 30])

    # So the memory read before the tabs is a browser with every Space it is read with at twenty
    # and fifty, and what the tabs add holds no Space's cost.
    def test_the_first_stage_opens_one_tab_in_each_space(self):
        self.assertEqual([tab.space for tab in self.stages[0]], [1, 2, 3, 4, 5])

    def test_each_space_holds_an_equal_share_at_each_count(self):
        held: dict[int, int] = {}
        for stage, expected in zip(self.stages, (1, 4, 10)):
            for tab in stage:
                held[tab.space] = held.get(tab.space, 0) + 1
            self.assertEqual(held, {space: expected for space in range(1, 6)})

    def test_the_tabs_are_numbered_once_each_from_the_launch_page(self):
        numbers = [tab.number for stage in self.stages for tab in stage]
        self.assertEqual(numbers, list(range(1, 51)))
        self.assertEqual(self.stages[0][0].space, 1)

    # A Space switch is a key the run has to send and wait for, so a stage visits each Space once.
    def test_a_stage_opens_each_spaces_tabs_together(self):
        for stage in self.stages:
            spaces = [tab.space for tab in stage]
            self.assertEqual(spaces, sorted(spaces))


def state(name: str, cpu: float) -> tuple[str, str, float, int]:
    """One process as `process_states` reads it: name, state, CPU seconds and RSS."""
    return (name, "S", cpu, 1024)


BROWSER = "omaweb-browser"
RENDERER = "QtWebEngineProc"


class IdleCpuTest(unittest.TestCase):
    """The CPU a tree used over a window, as a share of one core, and who used it."""

    def test_the_share_is_the_trees_cpu_seconds_over_the_window(self):
        idle = runtime.idle_cpu({1: state(BROWSER, 10.0), 2: state(RENDERER, 5.0)},
                                {1: state(BROWSER, 10.25), 2: state(RENDERER, 5.05)},
                                30.0)
        self.assertAlmostEqual(idle.percent, 1.0)

    def test_a_process_that_started_in_the_window_counts_all_it_used(self):
        idle = runtime.idle_cpu({1: state(BROWSER, 10.0)},
                                {1: state(BROWSER, 10.0), 7: state(RENDERER, 0.6)},
                                30.0)
        self.assertAlmostEqual(idle.percent, 2.0)

    # Its parent reaped it, so what it used is in the parent's reading, the part from before the
    # window included. That part is taken off, and the process is still named.
    def test_a_process_that_ended_in_the_window_counts_through_its_parent(self):
        idle = runtime.idle_cpu({1: state(BROWSER, 10.0), 4: state(RENDERER, 2.0)},
                                {1: state(BROWSER, 12.6)}, 30.0)
        self.assertAlmostEqual(idle.percent, 2.0)
        self.assertEqual(idle.ended, ["4 QtWebEngineProc"])

    def test_the_busiest_process_is_named_first_and_the_still_ones_not_at_all(self):
        idle = runtime.idle_cpu(
            {1: state(BROWSER, 1.0), 2: state(RENDERER, 1.0),
             3: state("dbus-daemon", 1.0)},
            {1: state(BROWSER, 1.1), 2: state(RENDERER, 1.4),
             3: state("dbus-daemon", 1.0)},
            30.0)
        self.assertEqual([(process.pid, process.name) for process in idle.busiest],
                         [(2, RENDERER), (1, BROWSER)])
        self.assertAlmostEqual(idle.busiest[0].seconds, 0.4)


class ProcessStatTest(unittest.TestCase):
    """What one line of `/proc/<pid>/stat` says a process is."""

    LINE = ("4242 (QtWebEngineProc) S 4000 4000 4000 0 -1 4194560 100 0 0 0 "
            "250 50 120 80 20 0 12 0 9000 2147483648 2560 18446744073709551615")

    # A process's CPU seconds count the children it reaped, so a renderer that went in the window
    # is not lost with it.
    def test_a_processes_cpu_counts_its_own_and_its_reaped_childrens(self):
        name, status, cpu, rss = runtime.parse_stat(self.LINE, ticks=100, page_kib=4)
        self.assertEqual((name, status, rss), ("QtWebEngineProc", "S", 10240))
        self.assertAlmostEqual(cpu, 5.0)

    def test_a_name_with_a_parenthesis_in_it_is_read_whole(self):
        line = self.LINE.replace("(QtWebEngineProc)", "(Web Content (x))")
        self.assertEqual(runtime.parse_stat(line, ticks=100, page_kib=4)[0], "Web Content (x)")


class ResultsTest(unittest.TestCase):
    """What the step hands the budget."""

    # What each tab past a Space's first adds, read against the browser with all five Spaces open,
    # so the cost of a Space is not counted as tabs' and the browser's own is not spread over them.
    def test_a_live_tab_is_what_each_tab_past_the_spaces_first_added(self):
        results = runtime.live_tab_results(500.0, {20: 1400.0, 50: 3650.0}, 1.5)
        self.assertEqual(results, {"live_tab_at_20_mebibytes": 60.0,
                                   "live_tab_at_50_mebibytes": 70.0,
                                   "live_tabs_idle_cpu_percent": 1.5})


class BudgetTest(unittest.TestCase):
    """Against the budget CI holds the browser to, so a ceiling missing from it, or one the step
    does not compare, fails here rather than as a run that never says CROSSED."""

    UNITS = {"live_tab_at_20_mebibytes": "MiB", "live_tab_at_50_mebibytes": "MiB",
             "live_tabs_idle_cpu_percent": "%"}

    def reported(self, results):
        with mock.patch.object(runtime, "log") as log:
            crossed = runtime.report(results, runtime.load_budget())
        return crossed, [call.args[0] for call in log.call_args_list]

    def test_each_reading_past_its_ceiling_fails_the_run(self):
        budget = runtime.load_budget()
        for name, unit in self.UNITS.items():
            ceiling = budget["measurements"][name]["ceiling"]
            crossed, lines = self.reported({name: ceiling + 1.0})
            self.assertEqual(crossed, 1, name)
            self.assertIn(f"CROSSED  {name}: {ceiling + 1.0:.2f} {unit} against {ceiling:.2f} "
                          f"{unit} (-1.00 {unit} of headroom)", lines)
            crossed, _ = self.reported({name: ceiling})
            self.assertEqual(crossed, 0, name)


class TabCountTest(unittest.TestCase):
    """The tabs the browser actually made, counted in each Space's own database."""

    def setUp(self):
        self.workspace = runtime.Workspace()
        self.addCleanup(self.workspace.discard)

    def space(self, name: str, tabs: int) -> None:
        directory = Path(self.workspace.root, "data", "spaces", name)
        directory.mkdir(parents=True)
        with sqlite3.connect(directory / "browser.sqlite") as database:
            database.execute("CREATE TABLE tabs (id TEXT PRIMARY KEY, url TEXT NOT NULL)")
            database.executemany("INSERT INTO tabs VALUES (?, ?)",
                                 [(f"{name}-{tab}", "http://127.0.0.2/") for tab in range(tabs)])
        database.close()

    def test_every_spaces_tabs_are_counted(self):
        self.space("a", 4)
        self.space("b", 3)
        self.assertEqual(self.workspace.tab_count(), 7)

    def test_a_space_with_no_database_yet_has_no_tabs(self):
        self.space("a", 2)
        Path(self.workspace.root, "data", "spaces", "b").mkdir()
        self.assertEqual(self.workspace.tab_count(), 2)


class FakeKeyboard:
    def __init__(self):
        self.keys: list[str] = []

    def focus(self):
        pass

    def press(self, key: str) -> None:
        self.keys.append(key)

    def write(self, text: str) -> None:
        self.keys.append(text)

    def enter_address(self, binding: str, address: str) -> None:
        self.keys += [binding, address, "Return"]


class FakeWorkspace:
    """A browser with five Spaces open, whose tab count is read from `counts` in turn."""

    def __init__(self, counts: list[int]):
        self.counts = counts

    def space_count(self) -> int:
        return 5

    def tab_count(self) -> int:
        return self.counts.pop(0) if len(self.counts) > 1 else self.counts[0]


class FakeBrowser:
    def __init__(self):
        self.awaited: list[str] = []

    def await_title(self, fragment: str) -> None:
        self.awaited.append(fragment)


class FakeSite:
    def address(self, number: int) -> str:
        return f"127.0.0.{number + 1}:8000/"


class OpenTabTest(unittest.TestCase):
    """The keys a tab past a Space's first is opened with, and what they are checked by."""

    def open(self, tab: runtime.LiveTab, shown: int, counts: list[int]):
        keyboard, browser = FakeKeyboard(), FakeBrowser()
        with mock.patch.object(runtime.time, "sleep"):
            now_shown = runtime.open_live_tab(keyboard, FakeWorkspace(counts), browser,
                                              FakeSite(), tab, shown)
        return keyboard.keys, browser.awaited, now_shown

    def test_a_tab_in_the_space_on_show_is_opened_without_a_switch(self):
        keys, awaited, shown = self.open(runtime.LiveTab(12, 3), 3, [10, 11])
        self.assertEqual(keys, ["Primary+T", "127.0.0.13:8000/", "Return"])
        self.assertEqual(awaited, [runtime.live_tab_shown(runtime.LiveTab(12, 3))])
        self.assertEqual(shown, 3)

    def test_a_tab_in_another_space_switches_to_it_first(self):
        keys, _, shown = self.open(runtime.LiveTab(12, 3), 2, [10, 11])
        self.assertEqual(keys, ["Primary+3", "Primary+T", "127.0.0.13:8000/", "Return"])
        self.assertEqual(shown, 3)

    # Keys sent to a window not ready for them make no tab, and the address typed would otherwise
    # be counted as a tab that was never made.
    def test_a_tab_the_browser_did_not_make_is_asked_for_again(self):
        with mock.patch.object(runtime, "TAB_RECORDED_WAIT", 0.0):
            keys, _, _ = self.open(runtime.LiveTab(12, 3), 3, [10, 10, 11])
        self.assertEqual(keys, ["Primary+T", "127.0.0.13:8000/", "Return", "Escape",
                                "Primary+T", "127.0.0.13:8000/", "Return"])

    def test_a_tab_the_browser_never_makes_fails_the_run(self):
        with mock.patch.object(runtime, "TAB_RECORDED_WAIT", 0.0), \
                self.assertRaises(runtime.MeasurementFailed):
            self.open(runtime.LiveTab(12, 3), 3, [10])


class KeyboardTest(unittest.TestCase):
    """Keys go to Hyprland only where there is one to take them."""

    # With HYPRLAND_INSTANCE_SIGNATURE unset, as under a headless cage, `hyprctl` is still on the
    # path and answers that there is no Hyprland, on stdout.
    def test_a_hyprctl_that_finds_no_hyprland_leaves_the_keys_to_wtype(self):
        answer = mock.Mock(returncode=1,
                           stdout="HYPRLAND_INSTANCE_SIGNATURE not set! (is hyprland running?)\n")
        with mock.patch.object(runtime.shutil, "which", return_value="/usr/bin/tool"), \
                mock.patch.object(runtime.subprocess, "run", return_value=answer):
            self.assertFalse(runtime.Keyboard(1).hyprland)


class TitleTest(unittest.TestCase):
    """What the run waits for to know a tab's page is on show."""

    # The run waits for each tab's title, and a title that began another's would end the wait for
    # tab 1 when tab 12 showed.
    def test_no_title_begins_another(self):
        titles = [runtime.live_tab_title(number) for number in range(1, 51)]
        for title in titles:
            self.assertEqual([other for other in titles if other.startswith(title)], [title])


    # The window's title is the tab's then the Space's, so waiting for both says the tab opened in
    # the Space it was meant for.
    def test_a_tab_is_awaited_in_its_own_space(self):
        self.assertEqual(runtime.live_tab_shown(runtime.LiveTab(12, 3)),
                         "Omaweb live tab 12: field notes — space3 — ")
        self.assertEqual(runtime.live_tab_shown(runtime.LiveTab(2, 1)),
                         "Omaweb live tab 2: field notes — Personal — ")


@unittest.skipUnless(sys.platform.startswith("linux"),
                     "only Linux routes all of 127.0.0.0/8 to the loopback interface")
class SiteTest(unittest.TestCase):
    """What each tab is served, from a server of its own on the machine."""

    def setUp(self):
        self.site = runtime.LiveTabSite(3)
        self.site.start()
        self.addCleanup(self.site.stop)

    def fetch(self, address: str) -> tuple[int, str]:
        try:
            with urllib.request.urlopen(f"http://{address}", timeout=5) as response:
                return response.status, response.read().decode("utf-8")
        except urllib.error.HTTPError as error:
            error.close()
            return error.code, ""

    def test_each_tab_is_its_own_page(self):
        for number in (1, 2, 3):
            status, page = self.fetch(self.site.address(number))
            self.assertEqual(status, 200)
            self.assertIn(f"<title>{runtime.live_tab_title(number)}</title>", page)

    # The engine gives a site, not an address and port, its own renderer, so tabs on one host
    # would share processes a reader's tabs on different sites do not.
    def test_each_tab_is_on_a_host_of_its_own(self):
        hosts = {urllib.parse.urlsplit(f"http://{self.site.address(number)}").hostname
                 for number in (1, 2, 3)}
        self.assertEqual(len(hosts), 3)

    def test_a_page_brings_its_stylesheet_and_picture(self):
        _, page = self.fetch(self.site.address(2))
        for resource in ("style.css", "picture.svg"):
            self.assertIn(f'"/{resource}"', page)
            status, _ = self.fetch(f"{self.site.address(2)}{resource}")
            self.assertEqual(status, 200, resource)

    # The idle reading is the browser's. A page that went on working would put its own CPU there.
    def test_nothing_on_a_page_runs_after_it_loaded(self):
        served = "".join(self.fetch(f"{self.site.address(1)}{path}")[1]
                         for path in ("", "style.css", "picture.svg"))
        for clock in ("setInterval", "setTimeout", "requestAnimationFrame", "animation",
                      "transition", "<video", "<audio", "<animate"):
            self.assertNotIn(clock, served)

    def test_the_server_answers_nothing_it_was_not_given(self):
        status, _ = self.fetch(f"{self.site.address(1)}elsewhere")
        self.assertEqual(status, 404)


if __name__ == "__main__":
    unittest.main()
