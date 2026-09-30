#!/usr/bin/env python3
"""The filter-list refresh, with easylist.to stood in for.

`scripts/sync_filter_lists.py --sync` writes the snapshots and the manifest that
`omaweb-vendored-filter-lists` checks them against. What is tested here is that
the two agree: each copy is what was served, and the manifest pins its source,
the day it was fetched and its digest.
"""

from __future__ import annotations

import contextlib
import datetime
import hashlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import sync_filter_lists as sync  # noqa: E402


def served(url: str) -> bytes:
    return f"! served from {url}\n||tracker.example^\n".encode()


class SyncTest(unittest.TestCase):

    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        for name, value in (("ROOT", self.root), ("MANIFEST_PATH", self.root / "MANIFEST.json"),
                            ("fetch", served)):
            patcher = mock.patch.object(sync, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def run_quietly(self, action) -> int:
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return action()

    def test_each_list_is_written_as_served_and_pinned_by_its_digest(self):
        self.assertEqual(self.run_quietly(sync.sync), 0)
        manifest = json.loads((self.root / "MANIFEST.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["license"], sync.LICENSE)
        self.assertEqual(set(manifest["files"]), set(sync.LISTS))
        for name, source in sync.LISTS.items():
            data = (self.root / name).read_bytes()
            self.assertEqual(data, served(source))
            entry = manifest["files"][name]
            self.assertEqual(entry["source"], source)
            self.assertEqual(entry["fetched"], datetime.date.today().isoformat())
            self.assertEqual(entry["sha256"], hashlib.sha256(data).hexdigest())

    def test_a_copy_edited_after_the_sync_fails_the_check(self):
        self.run_quietly(sync.sync)
        with open(self.root / "easylist.txt", "ab") as handle:
            handle.write(b"||edited.example^\n")
        self.assertEqual(self.run_quietly(sync.verify), 1)

    def test_a_list_the_manifest_does_not_name_fails_the_check(self):
        self.run_quietly(sync.sync)
        (self.root / "extra.txt").write_text("||extra.example^\n", encoding="utf-8")
        self.assertEqual(self.run_quietly(sync.verify), 1)


if __name__ == "__main__":
    unittest.main()
