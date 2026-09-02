#!/usr/bin/env python3
"""Vendor the Omarchy shell's QML component kit into third_party/omarchy-shell.

The kit is copied byte for byte. Nothing under the vendored tree is ever
edited by hand: Omaweb adapts to the kit through its own QML modules
(`src/ui/quickshell-shim` supplies the Quickshell types the kit imports,
`src/ui/*.qml` adapts the components to Omaweb's call sites). Keeping the
copies pristine is what makes a sync a review of upstream's diff instead of
a merge.

Usage:

    scripts/sync_omarchy_ui.py --verify              # local tree matches the manifest
    scripts/sync_omarchy_ui.py --check-upstream      # what changed on the pinned branch
    scripts/sync_omarchy_ui.py --check-upstream --report drift.json
    scripts/sync_omarchy_ui.py --sync                # re-fetch the pinned commit
    scripts/sync_omarchy_ui.py --sync --ref <sha>    # move the pin and re-fetch
"""

from __future__ import annotations

import argparse
import hashlib
import io
import subprocess
import tarfile
import json
import os
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

REPOSITORY = "omacom/omarchy"
BRANCH = "quattro"

# Upstream directory -> directory under the vendored `qs` module root. The
# names must stay `Commons` and `Ui`: every component imports `qs.Commons`,
# so the QML module URIs are part of the copied source.
VENDORED_DIRECTORIES = {
    "shell/Commons": "Commons",
    "shell/Ui": "Ui",
}

# Single files vendored alongside the modules, as upstream path -> local path.
VENDORED_FILES = {
    "LICENSE": "LICENSE",
}

VENDOR_ROOT = Path(__file__).resolve().parent.parent / "third_party" / "omarchy-shell"
MANIFEST_PATH = VENDOR_ROOT / "MANIFEST.json"
MODULE_ROOT = VENDOR_ROOT / "qs"

API = "https://api.github.com"
CODELOAD = "https://codeload.github.com"


def token() -> str | None:
    """GitHub's unauthenticated API allowance is 60 calls an hour, which one
    sync can exhaust. Reuse whatever token is already on the machine."""
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
    headers = {"User-Agent": "omaweb-sync-omarchy-ui"}
    if url.startswith(API):
        authorization = token()
        if authorization:
            headers["Authorization"] = f"Bearer {authorization}"
    request = urllib.request.Request(url, headers=headers)
    for attempt in range(1, attempts + 1):
        try:
            with urllib.request.urlopen(request, timeout=300) as response:
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


def read_manifest() -> dict:
    if not MANIFEST_PATH.exists():
        sys.exit(f"Missing manifest: {MANIFEST_PATH}. Run with --sync --ref <sha>.")
    return json.loads(MANIFEST_PATH.read_text())


def resolve_ref(ref: str) -> str:
    return fetch_json(f"{API}/repos/{REPOSITORY}/commits/{ref}")["sha"]


def blob_id(data: bytes) -> str:
    """Git's own blob sha, so a local file can be compared to a tree entry."""
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def list_upstream(ref: str) -> dict[str, dict[str, str]]:
    """Vendored path -> {upstream path, git blob sha}, from one tree listing."""
    tree = fetch_json(f"{API}/repos/{REPOSITORY}/git/trees/{ref}?recursive=1")
    if tree.get("truncated"):
        sys.exit("GitHub truncated the tree listing; sync needs a narrower query.")

    wanted: dict[str, str] = {}
    for entry in tree["tree"]:
        if entry["type"] != "blob":
            continue
        upstream_path = entry["path"]
        directory, _, name = upstream_path.rpartition("/")
        if directory in VENDORED_DIRECTORIES:
            wanted[f"qs/{VENDORED_DIRECTORIES[directory]}/{name}"] = upstream_path
        elif upstream_path in VENDORED_FILES:
            wanted[VENDORED_FILES[upstream_path]] = upstream_path

    blobs = {entry["path"]: entry["sha"] for entry in tree["tree"]}
    files = {
        vendored_path: {"upstream": upstream_path, "blob": blobs[upstream_path]}
        for vendored_path, upstream_path in wanted.items()
    }
    missing = set(VENDORED_FILES.values()) - set(files)
    if missing:
        sys.exit(f"Upstream no longer has: {', '.join(sorted(missing))}")
    return dict(sorted(files.items()))


def fetch_archive(commit: str, upstream_paths: set[str]) -> dict[str, bytes]:
    """Read the wanted files out of the commit's tarball — one request, and the
    contents are pinned to the commit rather than to a mutable branch."""
    archive = fetch(f"{CODELOAD}/{REPOSITORY}/tar.gz/{commit}")
    contents: dict[str, bytes] = {}
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as tar:
        for member in tar:
            if not member.isfile():
                continue
            # Every path in a GitHub tarball is prefixed with <repo>-<commit>/.
            _, _, upstream_path = member.name.partition("/")
            if upstream_path in upstream_paths:
                handle = tar.extractfile(member)
                if handle is not None:
                    contents[upstream_path] = handle.read()
    return contents


def local_digests(manifest: dict) -> dict[str, str | None]:
    digests: dict[str, str | None] = {}
    for vendored_path in manifest["files"]:
        path = VENDOR_ROOT / vendored_path
        digests[vendored_path] = digest(path.read_bytes()) if path.exists() else None
    return digests


def verify(manifest: dict) -> int:
    """Fail when a vendored file was edited, lost, or is untracked."""
    problems: list[str] = []
    for vendored_path, expected in manifest["files"].items():
        actual = (VENDOR_ROOT / vendored_path)
        if not actual.exists():
            problems.append(f"missing: {vendored_path}")
        elif digest(actual.read_bytes()) != expected["sha256"]:
            problems.append(f"edited locally: {vendored_path}")

    tracked = set(manifest["files"])
    for path in sorted(MODULE_ROOT.rglob("*")):
        if path.is_file():
            relative = path.relative_to(VENDOR_ROOT).as_posix()
            if relative not in tracked:
                problems.append(f"not in the manifest: {relative}")

    if problems:
        print(f"Vendored Omarchy kit does not match {MANIFEST_PATH.name}:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        print(
            "\nVendored files are copies and must not be edited. Adapt in src/ui instead,"
            "\nor re-run scripts/sync_omarchy_ui.py --sync to restore them.",
            file=sys.stderr,
        )
        return 1

    print(f"{len(tracked)} vendored files match the manifest ({manifest['ref'][:12]}).")
    return 0


def drift_report(manifest: dict, head: str, upstream: dict | None) -> dict:
    """What the pin is missing, as data. `upstream` is the tree listing at
    `head`, or None when the pin is already there and nothing was listed."""
    pinned = manifest["ref"]
    tracked = manifest["files"]
    behind = head != pinned

    added: list[str] = []
    removed: list[str] = []
    changed: list[str] = []
    if behind:
        upstream = upstream or {}
        added = sorted(path for path in upstream if path not in tracked)
        removed = sorted(path for path in tracked if path not in upstream)
        changed = sorted(
            path
            for path, entry in upstream.items()
            if path in tracked and entry["blob"] != tracked[path]["blob"]
        )

    return {
        "repository": REPOSITORY,
        "branch": BRANCH,
        "pinned": pinned,
        "head": head,
        "behind": behind,
        "added": added,
        "removed": removed,
        "changed": changed,
        "compare": f"https://github.com/{REPOSITORY}/compare/{pinned}...{head}",
        "sync_command": f"scripts/sync_omarchy_ui.py --sync --ref {head}",
    }


def write_report(path: Path, report: dict) -> None:
    path.write_text(json.dumps(report, indent=2) + "\n")


def report_changes(report: dict) -> list[tuple[str, str]]:
    """Every vendored file the pin is missing, as (kind, path), in one order."""
    return sorted(
        (kind, path)
        for kind in ("added", "removed", "changed")
        for path in report[kind]
    )


def print_report(report: dict) -> None:
    if not report["behind"]:
        print(f"Pinned at {report['branch']} head ({report['head'][:12]}). "
              "Nothing to sync.")
        return

    print(f"Pinned:      {report['pinned'][:12]}")
    print(f"{report['branch']} head: {report['head'][:12]}")

    changes = report_changes(report)
    for kind, path in changes:
        print(f"  {kind}: {path}")
    if not changes:
        print("  no changes to the vendored directories")

    print(f"\nCompare: {report['compare']}")
    print(f"Sync with: {report['sync_command']}")


def check_upstream(manifest: dict, report_path: Path | None = None) -> int:
    head = resolve_ref(BRANCH)
    # The tree listing is a second API call, and a pin that is already at head
    # has nothing to list.
    upstream = list_upstream(head) if head != manifest["ref"] else None
    report = drift_report(manifest, head, upstream)

    print_report(report)
    if report_path is not None:
        write_report(report_path, report)
    return 0


def sync(ref: str, force: bool) -> int:
    manifest = read_manifest() if MANIFEST_PATH.exists() else {"files": {}}
    if not force and manifest["files"]:
        drifted = [
            path
            for path, actual in local_digests(manifest).items()
            if actual is not None and actual != manifest["files"][path]["sha256"]
        ]
        if drifted:
            print("Refusing to overwrite locally edited vendored files:", file=sys.stderr)
            for path in drifted:
                print(f"  {path}", file=sys.stderr)
            print("Re-run with --force to discard those edits.", file=sys.stderr)
            return 1

    commit = resolve_ref(ref)
    upstream = list_upstream(commit)
    contents = fetch_archive(commit, {entry["upstream"] for entry in upstream.values()})

    for existing in sorted(MODULE_ROOT.rglob("*")):
        if existing.is_file():
            existing.unlink()
    for local_path in VENDORED_FILES.values():
        (VENDOR_ROOT / local_path).unlink(missing_ok=True)

    files: dict[str, dict[str, str]] = {}
    for vendored_path, entry in upstream.items():
        data = contents.get(entry["upstream"])
        if data is None:
            sys.exit(f"{entry['upstream']} is missing from the {commit[:12]} archive")
        if blob_id(data) != entry["blob"]:
            sys.exit(f"{entry['upstream']} does not match its tree entry at {commit[:12]}")
        destination = VENDOR_ROOT / vendored_path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        files[vendored_path] = {
            "upstream": entry["upstream"],
            "blob": entry["blob"],
            "sha256": digest(data),
        }
        print(f"  {vendored_path}")

    MANIFEST_PATH.write_text(
        json.dumps(
            {
                "repository": f"https://github.com/{REPOSITORY}",
                "branch": BRANCH,
                "ref": commit,
                "fetched": datetime.now(timezone.utc).strftime("%Y-%m-%d"),
                "license": "MIT",
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
                     help="report what changed on the pinned branch since the pin")
    mode.add_argument("--sync", action="store_true", help="re-fetch the vendored files")
    parser.add_argument("--report", type=Path,
                        help="write the --check-upstream result to this JSON file")
    parser.add_argument("--ref", help="commit or branch to pin (default: the current pin)")
    parser.add_argument("--force", action="store_true",
                        help="overwrite vendored files that were edited locally")
    arguments = parser.parse_args()
    if arguments.report and not arguments.check_upstream:
        parser.error("--report is only written by --check-upstream")

    try:
        if arguments.verify:
            return verify(read_manifest())
        if arguments.check_upstream:
            return check_upstream(read_manifest(), arguments.report)
        ref = arguments.ref or (read_manifest()["ref"] if MANIFEST_PATH.exists() else BRANCH)
        return sync(ref, arguments.force)
    except urllib.error.URLError as error:
        print(f"Could not reach GitHub: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
