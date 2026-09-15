#!/usr/bin/env python3
"""Which changes are worth a ten-minute build, from a throwaway repository.

The interesting property is the direction of the mistake. A file this does not
recognise has to count as source: skipping a build that was needed lets a break
reach `main`, and running one that was not costs ten minutes. So the cases
below are mostly about what must still build, not about what may be skipped.
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "source_changed.sh"

NOTHING_TO_BUILD = 1


def git(repo: Path, *arguments: str) -> str:
    return subprocess.run(
        ("git", *arguments), cwd=repo, check=True, capture_output=True, text=True
    ).stdout


def builds_for(paths: list[str]) -> bool:
    """Whether a commit touching exactly `paths` is worth a build."""
    with tempfile.TemporaryDirectory() as directory:
        repo = Path(directory)
        git(repo, "init", "--quiet")
        git(repo, "config", "user.email", "test@example.com")
        git(repo, "config", "user.name", "Test")
        git(repo, "commit", "--quiet", "--allow-empty", "-m", "chore: first")
        base = git(repo, "rev-parse", "HEAD").strip()
        for path in paths:
            written = repo / path
            written.parent.mkdir(parents=True, exist_ok=True)
            written.write_text("changed\n", encoding="utf-8")
        git(repo, "add", "--all")
        git(repo, "commit", "--quiet", "--allow-empty", "-m", "chore: change")

        result = subprocess.run(
            ("bash", str(SCRIPT), base, "HEAD"),
            cwd=repo,
            capture_output=True,
            text=True,
        )
        if result.returncode not in (0, NOTHING_TO_BUILD):
            raise AssertionError(f"unreadable range: {result.stderr}")
        return result.returncode == 0


class SourceChangedTest(unittest.TestCase):
    def test_prose_and_the_website_are_not_worth_a_build(self) -> None:
        self.assertFalse(builds_for(["docs/development.md"]))
        self.assertFalse(builds_for(["website/styles.css"]))
        self.assertFalse(builds_for(["README.md", "docs/adr/0001-a.md"]))

    def test_source_is(self) -> None:
        self.assertTrue(builds_for(["src/ui/Main.qml"]))
        self.assertTrue(builds_for(["CMakeLists.txt"]))
        self.assertTrue(builds_for(["tests/engine/tst_engine.cpp"]))

    def test_what_builds_the_build_counts_too(self) -> None:
        # None of these is compiled, and every one of them decides how the
        # compile happens or whether the package it produces still installs.
        self.assertTrue(builds_for(["scripts/check_package.sh"]))
        self.assertTrue(builds_for(["packaging/PKGBUILD"]))
        self.assertTrue(builds_for(["cmake/OmawebVersion.cmake"]))
        self.assertTrue(builds_for([".github/workflows/ci.yml"]))

    def test_a_directory_nobody_thought_of_builds(self) -> None:
        # The whole reason the test is written as an exclusion. Something new
        # at the top level is source until somebody says otherwise.
        self.assertTrue(builds_for(["modules/new-thing/thing.cpp"]))
        self.assertTrue(builds_for(["somewhere-nobody-expected/file.txt"]))

    def test_prose_beside_source_is_source(self) -> None:
        self.assertTrue(builds_for(["docs/development.md", "src/ui/Main.qml"]))

    def test_a_range_that_changes_nothing_builds_nothing(self) -> None:
        self.assertFalse(builds_for([]))

    def test_a_range_it_cannot_read_is_an_error(self) -> None:
        result = subprocess.run(
            ("bash", str(SCRIPT), "no-such-commit"),
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
