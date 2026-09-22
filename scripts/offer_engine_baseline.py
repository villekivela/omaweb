#!/usr/bin/env python3
"""Offers the security baseline the engine just published needs.

`security/baseline.json` is Omaweb's claim about the engine it is supported on,
and Settings marks a build below it as an unsupported preview. So the baseline
has to name the engine the pacman repository serves, and the two numbers that
say which engine that is come out of the tree it was built from rather than
being retyped: `engine.json` is written by the build.

What this opens is a pull request, not a commit. Raising the baseline means the
Qt release notes were read and a build was qualified against the engine, and
neither is something a publishing job can know (SECURITY.md). Two fields are
left alone for that reason, and the pull request says so:
`chromiumSecurityPatch`, which Qt states in prose, and `reviewed`, which is the
date a person read it.

    scripts/offer_engine_baseline.py --engine artifacts/engine.json --tag engine-6.11.2

Exits 0 having done nothing when the baseline already names that engine, which
is the case on a rebuild of a version already approved.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "security" / "baseline.json"

AUTHOR = ("Ville Kivelä", "5814559+villekivela@users.noreply.github.com")


def run(*command, check=True):
    return subprocess.run(command, cwd=ROOT, check=check, text=True,
                          capture_output=True)


def commit_message(qt, chromium, tag):
    return f"""chore(security): raise the baseline to QtWebEngine {qt}

The engine published from {tag} carries Chromium {chromium}. Both numbers are
read out of the tree it was built from rather than restated.

Two fields are deliberately untouched. `chromiumSecurityPatch` is the Chromium
release whose security fixes this engine includes, which Qt states in prose in
its release notes and nothing here can derive. `reviewed` is the date a person
read those notes. Merging this without filling both in publishes a baseline
naming an engine nobody checked."""


def body(qt, tag):
    return f"""The engine from `{tag}` is published and the pacman repository serves it. This
raises `qtwebengine` and `chromium` to match, both read out of the tree the
engine was built from.

Do not merge until two things are true, because neither is something the
publishing job can know:

- `chromiumSecurityPatch` names the Chromium release whose security fixes this
  engine includes. Qt states that in prose in its release notes.
- `reviewed` is the date somebody read those notes and qualified a build
  against this engine.

Until the baseline lands, every build reports itself as below it."""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True,
                        help="engine.json, as the build wrote it")
    parser.add_argument("--tag", required=True,
                        help="the release the engine was published from")
    parser.add_argument("--base", default="main",
                        help="the branch to open the pull request against")
    parser.add_argument("--dry-run", action="store_true",
                        help="write the file and print what would be pushed")
    arguments = parser.parse_args()

    engine = json.loads(arguments.engine.read_text(encoding="utf-8"))
    qt, chromium = engine["qtwebengine"], engine["chromium"]

    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    if baseline["qtwebengine"] == qt:
        print(f"the baseline already names {qt}, so there is nothing to offer")
        return 0

    was = baseline["qtwebengine"]
    baseline["qtwebengine"] = qt
    baseline["chromium"] = chromium
    BASELINE.write_text(json.dumps(baseline, indent=2) + "\n", encoding="utf-8")
    print(f"baseline: {was} -> {qt}, Chromium {chromium}")

    if arguments.dry_run:
        print(commit_message(qt, chromium, arguments.tag))
        print()
        print(body(qt, arguments.tag))
        return 0

    branch = f"engine/baseline-{qt}"
    run("git", "config", "user.name", AUTHOR[0])
    run("git", "config", "user.email", AUTHOR[1])
    run("git", "switch", "-c", branch)
    run("git", "add", str(BASELINE.relative_to(ROOT)))
    run("git", "commit", "--quiet", "--message",
        commit_message(qt, chromium, arguments.tag))
    run("git", "push", "--force", "origin", branch)

    opened = run("gh", "pr", "create", "--base", arguments.base, "--head", branch,
                 "--title", f"chore(security): raise the baseline to QtWebEngine {qt}",
                 "--body", body(qt, arguments.tag), check=False)
    print(opened.stdout.strip() or opened.stderr.strip())
    # A second publish of the same engine finds its own pull request already
    # open, which is not a failure: the branch was force-pushed above, so the
    # one that exists now carries this run's numbers.
    if opened.returncode != 0 and "already exists" not in opened.stderr:
        return opened.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())
