#!/usr/bin/env python3
"""Snapshot EasyList and EasyPrivacy into third_party/filter-lists.

The browser fetches both lists itself on a first run. These copies are for the measurements that
need the rules a reader runs and cannot wait on the network for them: the page-load budget in
scripts/benchmark_runtime.py, and the cosmetic benchmark's `--lists`. Each copy is byte for byte
what easylist.to served, pinned in MANIFEST.json by its source address, the day it was fetched and
its SHA-256, so a budget measured against one snapshot is measured against the same rules the next
time. `ctest` fails when a copy does not match (`omaweb-vendored-filter-lists`).

A refresh changes what the page-load budget measures, so take one on its own and re-record the
budget's numbers with it rather than folding it into another change.

Usage:

    scripts/sync_filter_lists.py --verify   # the snapshots match the manifest
    scripts/sync_filter_lists.py --sync     # fetch both lists again
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

LISTS = {
    "easylist.txt": "https://easylist.to/easylist/easylist.txt",
    "easyprivacy.txt": "https://easylist.to/easylist/easyprivacy.txt",
}
LICENSE = "GPL-3.0-or-later OR CC-BY-SA-3.0"

ROOT = Path(__file__).resolve().parent.parent / "third_party" / "filter-lists"
MANIFEST_PATH = ROOT / "MANIFEST.json"


def fetch(url: str, attempts: int = 4) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "omaweb-sync-filter-lists"})
    for attempt in range(1, attempts + 1):
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                return response.read()
        except urllib.error.HTTPError as error:
            # A refusal is an answer; only a flaky response is worth retrying.
            if error.code < 500 or attempt == attempts:
                raise
        except urllib.error.URLError:
            if attempt == attempts:
                raise
        time.sleep(attempt)
    raise AssertionError("unreachable")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify() -> int:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    problems = []
    for name, entry in manifest["files"].items():
        path = ROOT / name
        if not path.exists():
            problems.append(f"missing: {name}")
        elif digest(path.read_bytes()) != entry["sha256"]:
            problems.append(f"edited locally: {name}")
    for path in sorted(ROOT.glob("*.txt")):
        if path.name not in manifest["files"]:
            problems.append(f"not in the manifest: {path.name}")
    if problems:
        print(f"Filter-list snapshots do not match {MANIFEST_PATH.name}:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        print("\nRe-run scripts/sync_filter_lists.py --sync to replace them.", file=sys.stderr)
        return 1
    print(f"{len(manifest['files'])} filter-list snapshots match the manifest.")
    return 0


def sync() -> int:
    today = datetime.date.today().isoformat()
    files = {}
    for name, source in LISTS.items():
        data = fetch(source)
        (ROOT / name).write_bytes(data)
        files[name] = {"source": source, "fetched": today, "sha256": digest(data)}
        print(f"{name}: {len(data)} bytes from {source}")
    manifest = {"license": LICENSE, "files": files}
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return verify()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--verify", action="store_true",
                      help="check the snapshots against the manifest (no network)")
    mode.add_argument("--sync", action="store_true", help="fetch both lists again")
    arguments = parser.parse_args()
    if arguments.verify:
        return verify()
    try:
        return sync()
    except urllib.error.URLError as error:
        print(f"Could not reach easylist.to: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
