#!/usr/bin/env python3
"""The baseline offer a published engine makes, offline.

An engine release carries one file per architecture, and the offer reads them
all (#846). Run as the workflow runs it, with `--dry-run` and a baseline of its
own, so nothing is committed, pushed or opened.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "offer_engine_baseline.py"

APPROVED = {
    "version": 1,
    "reviewed": "2026-10-28",
    "qtwebengine": "6.11.3",
    "chromium": "140.0.7339.264",
    "chromiumSecurityPatch": "155.0.8059.85",
}


class OfferEngineBaseline(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.addCleanup(self._directory.cleanup)
        self.directory = Path(self._directory.name)
        self.baseline = self.directory / "baseline.json"
        self.baseline.write_text(json.dumps(APPROVED), encoding="utf-8")

    def engine_file(self, name, qtwebengine="6.11.4", chromium="140.0.7339.300"):
        path = self.directory / name
        path.write_text(json.dumps({"qtwebengine": qtwebengine, "chromium": chromium}),
                        encoding="utf-8")
        return path

    def offer(self, *engines):
        return subprocess.run(
            [sys.executable, str(SCRIPT), "--engine", *map(str, engines), "--tag",
             "engine-6.11.4-1", "--baseline", str(self.baseline), "--dry-run"],
            capture_output=True, text=True, check=False)

    def test_a_release_with_both_architectures_offers_the_baseline(self):
        result = self.offer(self.engine_file("engine-x86_64.json"),
                            self.engine_file("engine-aarch64.json"))

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("baseline: 6.11.3 -> 6.11.4, Chromium 140.0.7339.300", result.stdout)
        self.assertEqual(json.loads(self.baseline.read_text(encoding="utf-8"))["qtwebengine"], "6.11.4")

    def test_architectures_that_disagree_on_qtwebengine_fail(self):
        result = self.offer(self.engine_file("engine-x86_64.json"),
                            self.engine_file("engine-aarch64.json", qtwebengine="6.11.5"))

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("::error::", result.stderr)
        self.assertEqual(json.loads(self.baseline.read_text(encoding="utf-8")), APPROVED)

    def test_architectures_that_disagree_on_chromium_fail(self):
        result = self.offer(self.engine_file("engine-x86_64.json"),
                            self.engine_file("engine-aarch64.json", chromium="141.0.0.1"))

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("::error::", result.stderr)
        self.assertEqual(json.loads(self.baseline.read_text(encoding="utf-8")), APPROVED)

    def test_an_engine_file_without_a_version_fails(self):
        broken = self.directory / "engine-x86_64.json"
        broken.write_text(json.dumps({"qtwebengine": "6.11.4"}), encoding="utf-8")

        result = self.offer(broken)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("::error::", result.stderr)

    def test_a_release_with_no_engine_file_warns_and_offers_nothing(self):
        result = self.offer()

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("::warning::", result.stdout)
        self.assertIn("baseline was not offered", result.stdout)
        self.assertEqual(json.loads(self.baseline.read_text(encoding="utf-8")), APPROVED)


if __name__ == "__main__":
    unittest.main()
