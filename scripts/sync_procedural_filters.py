#!/usr/bin/env python3
"""Vendor Brave's procedural selector matcher into third_party/brave-procedural-filters.

brave-core's `procedural_filters.ts` finds the elements a procedural cosmetic
rule names, from the operator JSON adblock-rust emits. It is copied byte for
byte from a pinned brave-core commit, with brave-core's licence, and bundled by
esbuild into `procedural_filters.js`: a classic script defining one global,
because a WebEngineScript cannot load an ES module. The bundle is committed so a
build needs neither Node nor the network; Node and the esbuild pinned in
package.json are needed only here. The manifest pins the copies by their
upstream blob and the bundle by its digest. See
docs/adr/0052-apply-procedural-cosmetic-filters.md.

Usage:

    scripts/sync_procedural_filters.py --verify           # local tree matches the manifest
    scripts/sync_procedural_filters.py --check-upstream   # what changed since the pin
    scripts/sync_procedural_filters.py --sync             # re-fetch the pinned commit
    scripts/sync_procedural_filters.py --sync --ref <commit>
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

REPOSITORY = "brave/brave-core"
BRANCH = "master"

# Upstream path -> vendored path. The matcher has no imports, so the copies
# need not keep upstream's layout.
VENDORED_FILES = {
    "LICENSE": "LICENSE",
    "components/cosmetic_filters/resources/data/procedural_filters.ts": "src/procedural_filters.ts",
}
MATCHER = "components/cosmetic_filters/resources/data/procedural_filters.ts"

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
VENDOR_ROOT = REPOSITORY_ROOT / "third_party" / "brave-procedural-filters"
MANIFEST_PATH = VENDOR_ROOT / "MANIFEST.json"
BUNDLE = "procedural_filters.js"
# The name the bundle defines in the world that loads it. The engine loads it
# into ApplicationWorld only, so the page never sees it.
GLOBAL_NAME = "omawebProceduralFilters"
ESBUILD = REPOSITORY_ROOT / "node_modules" / ".bin" / "esbuild"

API = "https://api.github.com"
RAW = "https://raw.githubusercontent.com"


def token() -> str | None:
    """GitHub's unauthenticated API allowance is 60 calls an hour. Reuse
    whatever token is already on the machine."""
    for name in ("GITHUB_TOKEN", "GH_TOKEN"):
        value = os.environ.get(name)
        if value:
            return value
    try:
        result = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True,
            check=False, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return None
    return result.stdout.strip() or None


def fetch(url: str, attempts: int = 4) -> bytes:
    headers = {"User-Agent": "omaweb-sync-procedural-filters"}
    if url.startswith(API):
        authorization = token()
        if authorization:
            headers["Authorization"] = f"Bearer {authorization}"
    request = urllib.request.Request(url, headers=headers)
    for attempt in range(1, attempts + 1):
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                return response.read()
        except urllib.error.HTTPError as error:
            # A refusal is an answer; only a flaky response is worth retrying.
            if error.code < 500 or attempt == attempts:
                raise
            time.sleep(attempt)
        except urllib.error.URLError:
            if attempt == attempts:
                raise
            time.sleep(attempt)
    raise AssertionError("unreachable")


def fetch_json(url: str):
    return json.loads(fetch(url))


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def blob_id(data: bytes) -> str:
    """Git's own blob sha, so a local file can be compared to a tree entry."""
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def read_manifest() -> dict:
    if not MANIFEST_PATH.exists():
        sys.exit(f"Missing manifest: {MANIFEST_PATH}. Run with --sync --ref <ref>.")
    return json.loads(MANIFEST_PATH.read_text())


def resolve_ref(ref: str) -> str:
    return fetch_json(f"{API}/repos/{REPOSITORY}/commits/{ref}")["sha"]


def upstream_blobs(commit: str) -> dict[str, str]:
    """Upstream path -> git blob sha at the commit. brave-core's tree is too
    large to list recursively, so each file is asked for on its own."""
    blobs = {}
    for upstream_path in VENDORED_FILES:
        entry = fetch_json(f"{API}/repos/{REPOSITORY}/contents/{upstream_path}?ref={commit}")
        blobs[upstream_path] = entry["sha"]
    return blobs


def esbuild_version() -> str:
    package = REPOSITORY_ROOT / "node_modules" / "esbuild" / "package.json"
    if not ESBUILD.exists() or not package.exists():
        sys.exit("esbuild is not installed. Run `npm ci` at the repository root first.")
    return json.loads(package.read_text())["version"]


def bundle(commit: str) -> bytes:
    """Bundle the vendored matcher into a classic script. The banner carries
    the licence notice esbuild would otherwise drop with the source's leading
    comment, and the commit the bundle was built from."""
    banner = (
        "// Bundled from brave-core's procedural_filters.ts at\n"
        f"// {commit} by scripts/sync_procedural_filters.py.\n"
        "// This Source Code Form is subject to the terms of the Mozilla Public\n"
        "// License, v. 2.0. If a copy of the MPL was not distributed with this file,\n"
        "// You can obtain one at https://mozilla.org/MPL/2.0/."
    )
    result = subprocess.run(
        [
            str(ESBUILD),
            str(VENDOR_ROOT / VENDORED_FILES[MATCHER]),
            "--bundle",
            "--format=iife",
            f"--global-name={GLOBAL_NAME}",
            "--target=es2022",
            "--charset=utf8",
            "--legal-comments=none",
            f"--banner:js={banner}",
            "--log-level=warning",
        ],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr.decode())
        sys.exit("Could not bundle the vendored matcher.")
    return result.stdout


def verify(manifest: dict) -> int:
    """Fail when a vendored file or the bundle was edited, lost, or is untracked."""
    problems: list[str] = []
    for vendored_path, expected in manifest["files"].items():
        path = VENDOR_ROOT / vendored_path
        if not path.exists():
            problems.append(f"missing: {vendored_path}")
        elif digest(path.read_bytes()) != expected["sha256"]:
            problems.append(f"edited locally: {vendored_path}")

    for path in sorted((VENDOR_ROOT / "src").rglob("*")):
        if path.is_file():
            relative = path.relative_to(VENDOR_ROOT).as_posix()
            if relative not in manifest["files"]:
                problems.append(f"not in the manifest: {relative}")

    for entry in manifest["generated"]:
        generated = VENDOR_ROOT / entry["path"]
        if not generated.exists():
            problems.append(f"missing: {entry['path']}")
        elif digest(generated.read_bytes()) != entry["sha256"]:
            problems.append(f"does not match its pin: {entry['path']}")

    if problems:
        print(f"Vendored procedural filters do not match {MANIFEST_PATH.name}:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        print(
            "\nVendored files are copies and must not be edited. Re-run"
            "\nscripts/sync_procedural_filters.py --sync to restore them.",
            file=sys.stderr,
        )
        return 1

    print(f"{len(manifest['files'])} vendored files and {BUNDLE} match the "
          f"manifest ({manifest['ref'][:12]}).")
    return 0


def check_upstream(manifest: dict) -> int:
    head = resolve_ref(BRANCH)
    pinned = manifest["ref"]
    if head == pinned:
        print(f"Pinned at {BRANCH} head ({head[:12]}). Nothing to sync.")
        return 0

    blobs = upstream_blobs(head)
    changed = [
        upstream_path
        for upstream_path, vendored_path in VENDORED_FILES.items()
        if blobs[upstream_path] != manifest["files"][vendored_path]["blob"]
    ]
    print(f"Pinned:      {pinned[:12]}")
    print(f"{BRANCH} head: {head[:12]}")
    for upstream_path in changed:
        print(f"  changed: {upstream_path}")
    if not changed:
        print("  no changes to the vendored files")
    print(f"\nCompare: https://github.com/{REPOSITORY}/compare/{pinned}...{head}")
    print(f"Sync with: scripts/sync_procedural_filters.py --sync --ref {head}")
    return 0


def sync(ref: str, force: bool) -> int:
    manifest = read_manifest() if MANIFEST_PATH.exists() else {"files": {}}
    if not force and manifest["files"]:
        drifted = [
            path
            for path, entry in manifest["files"].items()
            if (VENDOR_ROOT / path).exists()
            and digest((VENDOR_ROOT / path).read_bytes()) != entry["sha256"]
        ]
        if drifted:
            print("Refusing to overwrite locally edited vendored files:", file=sys.stderr)
            for path in drifted:
                print(f"  {path}", file=sys.stderr)
            print("Re-run with --force to discard those edits.", file=sys.stderr)
            return 1

    version = esbuild_version()
    commit = resolve_ref(ref)
    blobs = upstream_blobs(commit)

    files: dict[str, dict[str, str]] = {}
    for upstream_path, vendored_path in VENDORED_FILES.items():
        data = fetch(f"{RAW}/{REPOSITORY}/{commit}/{upstream_path}")
        if blob_id(data) != blobs[upstream_path]:
            sys.exit(f"{upstream_path} does not match its tree entry at {commit[:12]}")
        destination = VENDOR_ROOT / vendored_path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        files[vendored_path] = {
            "upstream": upstream_path,
            "blob": blobs[upstream_path],
            "sha256": digest(data),
        }
        print(f"  {vendored_path}")

    bundled = bundle(commit)
    (VENDOR_ROOT / BUNDLE).write_bytes(bundled)
    print(f"  {BUNDLE}")

    MANIFEST_PATH.write_text(
        json.dumps(
            {
                "repository": f"https://github.com/{REPOSITORY}",
                "branch": BRANCH,
                "ref": commit,
                "fetched": datetime.now(timezone.utc).strftime("%Y-%m-%d"),
                "license": "MPL-2.0",
                "bundler": f"esbuild {version}",
                "global": GLOBAL_NAME,
                "generated": [{"path": BUNDLE, "sha256": digest(bundled)}],
                "files": files,
            },
            indent=2,
        )
        + "\n"
    )
    print(f"\nPinned {len(files)} files at {commit}.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--verify", action="store_true",
                     help="check the vendored tree against the manifest (no network)")
    mode.add_argument("--check-upstream", action="store_true",
                     help="report what changed upstream since the pin")
    mode.add_argument("--sync", action="store_true", help="re-fetch the vendored files")
    parser.add_argument("--ref", help="commit, tag, or branch to pin (default: the current pin)")
    parser.add_argument("--force", action="store_true",
                        help="overwrite vendored files that were edited locally")
    arguments = parser.parse_args()

    try:
        if arguments.verify:
            return verify(read_manifest())
        if arguments.check_upstream:
            return check_upstream(read_manifest())
        ref = arguments.ref or (read_manifest()["ref"] if MANIFEST_PATH.exists() else BRANCH)
        return sync(ref, arguments.force)
    except urllib.error.URLError as error:
        print(f"Could not reach GitHub: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
