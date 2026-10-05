#!/usr/bin/env python3
"""Which pushes Vercel builds a website preview for, against a throwaway origin.

The direction of the mistake matters here as it does for `source_changed.sh`.
A preview built for nothing costs seconds; a preview skipped for a change to
the site leaves a pull request without one, and a production build skipped
leaves readers on an old site. So most of these cases are about what must
still build.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "website_unchanged.sh"

# Vercel's convention, not the usual one.
SKIP = 0
BUILD = 1


def git(repo: Path, *arguments: str) -> str:
    return subprocess.run(
        ("git", *arguments), cwd=repo, check=True, capture_output=True, text=True
    ).stdout.strip()


def shared_files() -> list[str]:
    """The files outside `website/` that the build serves, as `site.mjs` names them."""
    source = (ROOT / "website" / "build" / "site.mjs").read_text(encoding="utf-8")
    block = re.search(r"export const SHARED = \{(.*?)\};", source, re.DOTALL)
    assert block, "site.mjs no longer exports SHARED"
    return re.findall(r'new URL\("\.\./\.\./([^"]+)"', block.group(1))


class WebsiteUnchangedTest(unittest.TestCase):
    def setUp(self) -> None:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.origin = Path(directory.name) / "origin.git"
        self.clone = Path(directory.name) / "clone"
        git(Path(directory.name), "init", "--quiet", "--bare", str(self.origin))
        git(Path(directory.name), "clone", "--quiet", str(self.origin), str(self.clone))
        git(self.clone, "config", "user.email", "test@example.com")
        git(self.clone, "config", "user.name", "Test")
        git(self.clone, "switch", "--quiet", "-c", "main")
        self.commit(["website/index.html", "src/main.cpp"])
        git(self.clone, "push", "--quiet", "origin", "main")
        self.main = self.head()
        git(self.clone, "switch", "--quiet", "-c", "feat/1-something")

    def head(self) -> str:
        return git(self.clone, "rev-parse", "HEAD")

    def commit(self, paths: list[str]) -> str:
        for path in paths:
            written = self.clone / path
            written.parent.mkdir(parents=True, exist_ok=True)
            with written.open("a", encoding="utf-8") as file:
                file.write("changed\n")
        git(self.clone, "add", "--all")
        git(self.clone, "commit", "--quiet", "--allow-empty", "-m", "chore: change")
        return self.head()

    def answer(self, previous: str | None, environment: str | None = "preview") -> int:
        env = {k: v for k, v in os.environ.items() if not k.startswith("VERCEL_")}
        if environment is not None:
            env["VERCEL_ENV"] = environment
        if previous is not None:
            env["VERCEL_GIT_PREVIOUS_SHA"] = previous
        result = subprocess.run(
            ("sh", str(SCRIPT)),
            cwd=self.clone / "website",
            env=env,
            capture_output=True,
            text=True,
        )
        self.assertIn(result.returncode, (SKIP, BUILD), result.stderr)
        return result.returncode

    def test_a_change_to_the_site_builds(self) -> None:
        self.commit(["website/styles.css"])
        self.assertEqual(self.answer(self.main), BUILD)

    def test_a_change_elsewhere_does_not(self) -> None:
        self.commit(["src/main.cpp"])
        self.assertEqual(self.answer(self.main), SKIP)
        self.commit(["docs/development.md", "README.md"])
        self.assertEqual(self.answer(self.main), SKIP)

    def test_the_files_the_site_serves_from_outside_it_build(self) -> None:
        # Read from the build itself, so a file added to SHARED without the
        # script learning of it fails here rather than on the site.
        shared = shared_files()
        self.assertTrue(shared)
        for path in shared:
            with self.subTest(path=path):
                before = self.head()
                self.commit([path])
                self.assertEqual(self.answer(before), BUILD)

    def test_production_always_builds(self) -> None:
        # A release changes the release pages without changing a file.
        self.commit(["src/main.cpp"])
        self.assertEqual(self.answer(self.main, "production"), BUILD)
        self.assertEqual(self.answer(self.head(), "production"), BUILD)

    def test_an_unnamed_environment_builds(self) -> None:
        # Production's guard must not rest on Vercel naming it: without the
        # name, the website branch would be compared like any other.
        self.commit(["src/main.cpp"])
        self.assertEqual(self.answer(self.head(), None), BUILD)

    def test_every_push_since_the_last_build_counts(self) -> None:
        self.commit(["website/styles.css"])
        self.commit(["src/main.cpp"])
        self.assertEqual(self.answer(self.main), BUILD)

    def test_a_push_after_the_last_build_is_compared_with_it(self) -> None:
        built = self.commit(["website/styles.css"])
        self.commit(["src/main.cpp"])
        self.assertEqual(self.answer(built), SKIP)

    def test_a_first_push_is_compared_with_main(self) -> None:
        # Vercel has built nothing for a new branch, so the base is where it
        # left `main`, not the last commit alone.
        self.commit(["website/styles.css"])
        self.commit(["src/main.cpp"])
        self.assertEqual(self.answer(None), BUILD)

    def test_a_first_push_that_leaves_the_site_alone_skips(self) -> None:
        self.commit(["src/main.cpp"])
        self.commit(["docs/development.md"])
        self.assertEqual(self.answer(None), SKIP)

    def test_a_last_build_older_than_the_clone_is_compared_with_main(self) -> None:
        self.commit(["website/styles.css"])
        self.commit(["src/main.cpp"])
        self.assertEqual(self.answer("0" * 40), BUILD)

    def test_no_main_to_compare_with_builds(self) -> None:
        self.commit(["src/main.cpp"])
        git(self.clone, "remote", "remove", "origin")
        self.assertEqual(self.answer(None), BUILD)

    def test_vercel_runs_it(self) -> None:
        config = json.loads((ROOT / "website" / "vercel.json").read_text(encoding="utf-8"))
        self.assertEqual(config.get("ignoreCommand"), "sh ../scripts/website_unchanged.sh")


if __name__ == "__main__":
    unittest.main()
