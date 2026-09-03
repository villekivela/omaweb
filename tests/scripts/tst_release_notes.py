#!/usr/bin/env python3
"""The release notes a tag turns into, built from a throwaway repository.

The interesting property is that a Conventional Commit scope does not change
which section a subject lands in. `fix:` and `fix(engine):` are the same kind
of change, and a grouping that quietly drops the scoped half produces notes
that look complete while missing most of the release.
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "release_notes.sh"


def git(repo: Path, *arguments: str) -> str:
    return subprocess.run(
        ("git", *arguments), cwd=repo, check=True, capture_output=True, text=True
    ).stdout


def notes_for(subjects: list[str]) -> str:
    """Notes for a repository whose only history since v1.0.0 is `subjects`."""
    with tempfile.TemporaryDirectory() as directory:
        repo = Path(directory)
        git(repo, "init", "--quiet")
        git(repo, "config", "user.email", "test@example.com")
        git(repo, "config", "user.name", "Test")
        git(repo, "commit", "--quiet", "--allow-empty", "-m", "chore: first")
        git(repo, "tag", "v1.0.0")
        for subject in subjects:
            git(repo, "commit", "--quiet", "--allow-empty", "-m", subject)
        git(repo, "tag", "v1.1.0")
        return subprocess.run(
            ("bash", str(SCRIPT), "v1.1.0"),
            cwd=repo,
            check=True,
            capture_output=True,
            text=True,
        ).stdout


class ReleaseNotesTest(unittest.TestCase):
    def test_a_scope_does_not_hide_a_subject(self) -> None:
        notes = notes_for(["fix(engine): scoped", "fix: unscoped"])
        self.assertIn("- scoped", notes)
        self.assertIn("- unscoped", notes)

    def test_every_type_keeps_its_section_when_scoped(self) -> None:
        notes = notes_for(
            [
                "feat(ui): a feature",
                "fix(engine): a fix",
                "perf(qml): a speedup",
                "build(cmake): a build change",
                "ci(arch): a ci change",
                "docs(adr): a doc",
                "refactor(core): an internal change",
                "revert(ui)!: a breaking revert",
            ]
        )
        for heading, entry in (
            ("### Breaking changes", "- a breaking revert"),
            ("### Features", "- a feature"),
            ("### Fixes", "- a fix"),
            ("### Performance", "- a speedup"),
            ("### Build and CI", "- a build change"),
            ("### Documentation", "- a doc"),
            ("### Internal", "- an internal change"),
        ):
            self.assertIn(heading, notes)
            self.assertIn(entry, notes)

    def test_a_breaking_change_is_listed_once_as_breaking(self) -> None:
        notes = notes_for(["feat(ui)!: a breaking feature"])
        self.assertIn("### Breaking changes", notes)
        self.assertNotIn("### Features", notes)

    def test_an_empty_section_is_left_out(self) -> None:
        notes = notes_for(["docs(adr): a doc"])
        self.assertNotIn("### Fixes", notes)

    def test_a_merge_commit_is_not_an_entry(self) -> None:
        # GitHub writes a merge subject, and the commits it carries are already
        # in the range, so the merge itself must not become a second line.
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            git(repo, "init", "--quiet", "--initial-branch", "main")
            git(repo, "config", "user.email", "test@example.com")
            git(repo, "config", "user.name", "Test")
            git(repo, "commit", "--quiet", "--allow-empty", "-m", "chore: first")
            git(repo, "tag", "v1.0.0")
            git(repo, "checkout", "--quiet", "-b", "work")
            git(repo, "commit", "--quiet", "--allow-empty", "-m", "fix(ui): the carried commit")
            git(repo, "checkout", "--quiet", "main")
            git(repo, "merge", "--quiet", "--no-ff", "work", "-m",
                "Merge pull request #1 from villekivela/work")
            git(repo, "tag", "v1.1.0")
            notes = subprocess.run(
                ("bash", str(SCRIPT), "v1.1.0"),
                cwd=repo, check=True, capture_output=True, text=True,
            ).stdout
        self.assertIn("- the carried commit", notes)
        self.assertNotIn("Merge pull request", notes)


if __name__ == "__main__":
    unittest.main(verbosity=2)
