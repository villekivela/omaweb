#!/usr/bin/env python3
"""Where the website is published from, against a throwaway origin.

Vercel builds the live site from the `website` branch, and the script moves
that branch to a release. A branch already at the named commit gets no push
event, so the script asks the deploy hook for the build instead; a branch it
moved has already asked, and one build is enough.
"""

from __future__ import annotations

import http.server
import os
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "publish_website.sh"


def git(repo: Path, *arguments: str) -> str:
    return subprocess.run(
        ("git", *arguments), cwd=repo, check=True, capture_output=True, text=True
    ).stdout.strip()


class Hook(http.server.BaseHTTPRequestHandler):
    """A deploy hook that counts the builds it was asked for."""

    posts = 0

    def do_POST(self) -> None:  # noqa: N802, the name http.server calls
        type(self).posts += 1
        self.send_response(201)
        self.end_headers()

    def log_message(self, *_: object) -> None:
        pass


class PublishWebsiteTest(unittest.TestCase):
    def setUp(self) -> None:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.origin = Path(directory.name) / "origin.git"
        self.clone = Path(directory.name) / "clone"
        git(Path(directory.name), "init", "--quiet", "--bare", str(self.origin))
        git(Path(directory.name), "clone", "--quiet", str(self.origin), str(self.clone))
        git(self.clone, "config", "user.email", "test@example.com")
        git(self.clone, "config", "user.name", "Test")
        git(self.clone, "commit", "--quiet", "--allow-empty", "-m", "feat: first")
        git(self.clone, "tag", "v0.1.0")
        self.first = git(self.clone, "rev-parse", "HEAD")
        git(self.clone, "commit", "--quiet", "--allow-empty", "-m", "feat: second")
        git(self.clone, "tag", "v0.2.0")
        self.second = git(self.clone, "rev-parse", "HEAD")
        git(self.clone, "push", "--quiet", "origin", "HEAD:main", "--tags")

        Hook.posts = 0
        server = http.server.HTTPServer(("127.0.0.1", 0), Hook)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.shutdown)
        self.hook = f"http://127.0.0.1:{server.server_port}/"

    def publish(self, *arguments: str, hook: str | None = None) -> subprocess.CompletedProcess:
        environment = {k: v for k, v in os.environ.items() if k != "HOOK"}
        if hook is not None:
            environment["HOOK"] = hook
        return subprocess.run(
            ("sh", str(SCRIPT), *arguments),
            cwd=self.clone,
            env=environment,
            capture_output=True,
            text=True,
        )

    def published(self) -> str:
        return git(self.origin, "rev-parse", "--verify", "--quiet", "refs/heads/website")

    def test_a_release_moves_the_branch_and_the_push_asks_for_the_build(self) -> None:
        result = self.publish("v0.2.0", hook=self.hook)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.published(), self.second)
        self.assertEqual(Hook.posts, 0)

    def test_a_branch_already_there_is_built_through_the_hook(self) -> None:
        self.assertEqual(self.publish("v0.2.0", hook=self.hook).returncode, 0)
        result = self.publish("v0.2.0", hook=self.hook)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(Hook.posts, 1)

    def test_an_older_release_can_be_published_again(self) -> None:
        self.assertEqual(self.publish("v0.2.0").returncode, 0)
        result = self.publish("v0.1.0")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.published(), self.first)

    def test_a_dry_run_says_what_it_would_do_and_does_nothing(self) -> None:
        result = self.publish("--dry-run", "v0.2.0", hook=self.hook)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(self.second, result.stdout)
        with self.assertRaises(subprocess.CalledProcessError):
            self.published()
        self.assertEqual(Hook.posts, 0)

    def test_a_name_that_is_not_a_commit_is_refused(self) -> None:
        result = self.publish("v9.9.9", hook=self.hook)
        self.assertNotEqual(result.returncode, 0)
        with self.assertRaises(subprocess.CalledProcessError):
            self.published()

    def test_a_missing_hook_leaves_an_unmoved_branch_unbuilt_and_says_so(self) -> None:
        self.assertEqual(self.publish("v0.2.0").returncode, 0)
        result = self.publish("v0.2.0")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("VERCEL_DEPLOY_HOOK_URL", result.stdout)


if __name__ == "__main__":
    unittest.main()
