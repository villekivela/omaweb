#!/usr/bin/env python3
"""Which processes the memory budget's reading is made of, without a browser.

`scripts/benchmark_runtime.py memory` reads one number for the whole process tree, and a number that
moved does not say whether the browser's own process grew or one of the engine's. Every engine
process has the same name, so the split is read from each one's command line.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import benchmark_runtime as runtime  # noqa: E402

ENGINE = "/usr/lib/omaweb/lib/qt6/libexec/QtWebEngineProcess"


class ProcessKindTest(unittest.TestCase):

    def test_the_browser_has_no_type(self):
        self.assertEqual(runtime.process_kind(["/build/ci/omaweb-browser", "http://a.test/"]),
                         "browser")

    def test_an_engine_process_is_named_by_its_type(self):
        self.assertEqual(runtime.process_kind([ENGINE, "--type=gpu-process", "--no-sandbox"]),
                         "gpu-process")
        self.assertEqual(runtime.process_kind([ENGINE, "--type=renderer", "--lang=en"]),
                         "renderer")
        self.assertEqual(runtime.process_kind([ENGINE, "--type=zygote"]), "zygote")

    def test_a_utility_process_is_named_by_its_service(self):
        self.assertEqual(
            runtime.process_kind([ENGINE, "--type=utility",
                                  "--utility-sub-type=network.mojom.NetworkService"]),
            "utility network.mojom.NetworkService")

    def test_the_session_bus_is_the_launcher_and_not_the_browser(self):
        self.assertEqual(runtime.process_kind(["dbus-run-session", "--", "omaweb-browser"]),
                         "session bus")
        self.assertEqual(runtime.process_kind(["/usr/bin/dbus-daemon", "--session"]),
                         "session bus")


class SplitTest(unittest.TestCase):
    """The tree's reading, summed by kind, adds up to the whole tree's."""

    def setUp(self):
        # launcher 1 -> bus 2 and browser 3 -> zygote 4 -> renderers 5 and 6, and the gpu 7.
        self.tree = {1: [2, 3], 3: [4, 7], 4: [5, 6]}
        self.argv = {
            1: ["dbus-run-session", "--", "omaweb-browser"],
            2: ["dbus-daemon", "--session"],
            3: ["omaweb-browser", "http://a.test/"],
            4: [ENGINE, "--type=zygote"],
            5: [ENGINE, "--type=renderer"],
            6: [ENGINE, "--type=renderer"],
            7: [ENGINE, "--type=gpu-process"],
        }
        self.pss_kib = {1: 512, 2: 1536, 3: 300 * 1024, 4: 10 * 1024, 5: 60 * 1024,
                        6: 40 * 1024, 7: 90 * 1024}
        self.saved = runtime.children_by_parent, runtime.read_pss_kib, runtime.read_argv
        runtime.children_by_parent = lambda: self.tree
        runtime.read_pss_kib = lambda pid: self.pss_kib[pid]
        runtime.read_argv = lambda pid: self.argv[pid]

    def tearDown(self):
        runtime.children_by_parent, runtime.read_pss_kib, runtime.read_argv = self.saved

    def test_each_kind_holds_what_its_processes_hold(self):
        self.assertEqual(runtime.tree_split_mib(1), {
            "browser": 300.0,
            "gpu-process": 90.0,
            "renderer": 100.0,
            "session bus": 2.0,
            "zygote": 10.0,
        })

    def test_the_kinds_add_up_to_the_tree(self):
        self.assertEqual(sum(runtime.tree_split_mib(1).values()), runtime.tree_mib(1))

    def test_a_process_gone_before_its_command_line_is_read_is_left_out(self):
        self.argv[6] = []
        self.pss_kib[6] = 0
        self.assertEqual(runtime.tree_split_mib(1)["renderer"], 60.0)


if __name__ == "__main__":
    unittest.main()
