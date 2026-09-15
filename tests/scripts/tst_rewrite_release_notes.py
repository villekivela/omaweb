#!/usr/bin/env python3
"""The rewritten release notes, with the model and GitHub taken out.

A release publishes unattended, so the properties worth holding are the ones
nothing reviews in between: the compare URL survives whatever the model
answers, an answer the release page cannot render is refused rather than
published, and every failure leaves the generated notes in place instead of
leaving the release without notes.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import threading
import unittest
import unittest.mock
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
SCRIPT = ROOT / "scripts" / "rewrite_release_notes.py"

import rewrite_release_notes as rewrite  # noqa: E402

GENERATED = """\
Changes since v1.0.0.

### Features

- a feature (abc1234)

**Full changelog**: https://github.com/villekivela/omaweb/compare/v1.0.0...v1.1.0
"""


class Api:
    """The Messages API with the network taken out: what it was asked, and the
    answer or status it was told to give."""

    def __init__(self, answer="## What is new\n\n- The page now does the thing.", status=200,
                 stop_reason="end_turn"):
        self.answer = answer
        self.status = status
        self.stop_reason = stop_reason
        self.requests = []
        api = self

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                length = int(self.headers.get("content-length", "0"))
                api.requests.append(json.loads(self.rfile.read(length)))
                self.send_response(api.status)
                self.send_header("content-type", "application/json")
                body = json.dumps(
                    {
                        "content": [{"type": "text", "text": api.answer}],
                        "stop_reason": api.stop_reason,
                    }
                ).encode()
                self.send_header("content-length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *arguments):
                pass

        self._server = HTTPServer(("127.0.0.1", 0), Handler)
        self.url = f"http://127.0.0.1:{self._server.server_port}"

    def __enter__(self):
        threading.Thread(target=self._server.serve_forever, daemon=True).start()
        return self

    def __exit__(self, *details):
        self._server.shutdown()
        self._server.server_close()


def git(repo: Path, *arguments: str) -> str:
    return subprocess.run(
        ("git", *arguments), cwd=repo, check=True, capture_output=True, text=True
    ).stdout


def repository(directory: str, messages: list[str]) -> Path:
    """A throwaway repository whose only history since v1.0.0 is `messages`."""
    repo = Path(directory)
    git(repo, "init", "--quiet")
    git(repo, "config", "user.email", "test@example.com")
    git(repo, "config", "user.name", "Test")
    git(repo, "commit", "--quiet", "--allow-empty", "-m", "chore: first")
    git(repo, "tag", "v1.0.0")
    for message in messages:
        git(repo, "commit", "--quiet", "--allow-empty", "-m", message)
    git(repo, "tag", "v1.1.0")
    return repo


def run(repo: Path, url: str, key: str = "test-key", generated: str = GENERATED):
    """The script over `repo`, answering from `url`. Returns the process and
    the notes file, which holds the generated notes when the rewrite failed."""
    notes = repo / "notes.md"
    notes.write_text(generated, encoding="utf-8")
    # A `gh` that answers nothing, ahead of any real one: the issue lookup is
    # context the notes are better for, never something a release waits on.
    stubs = repo / "stubs"
    stubs.mkdir(exist_ok=True)
    (stubs / "gh").write_text("#!/bin/sh\nexit 1\n", encoding="utf-8")
    (stubs / "gh").chmod(0o755)
    environment = {
        "PATH": f"{stubs}:{os.environ['PATH']}",
        "HOME": str(repo),
        "ANTHROPIC_BASE_URL": url,
    }
    if key:
        environment["ANTHROPIC_API_KEY"] = key
    result = subprocess.run(
        (sys.executable, str(SCRIPT), "v1.1.0", "--generated", str(notes),
         "--output", str(notes)),
        cwd=repo,
        capture_output=True,
        text=True,
        env=environment,
    )
    return result, notes.read_text(encoding="utf-8")


class Rewrite(unittest.TestCase):
    def test_the_rewrite_replaces_the_generated_notes(self):
        with tempfile.TemporaryDirectory() as directory, Api() as api:
            repo = repository(directory, ["feat: a feature"])

            result, notes = run(repo, api.url)

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("The page now does the thing.", notes)
            self.assertNotIn("### Features", notes)

    def test_the_compare_url_survives_a_rewrite_that_drops_it(self):
        with tempfile.TemporaryDirectory() as directory, Api() as api:
            repo = repository(directory, ["feat: a feature"])

            _, notes = run(repo, api.url)

            self.assertIn(
                "**Full changelog**: "
                "https://github.com/villekivela/omaweb/compare/v1.0.0...v1.1.0",
                notes,
            )

    def test_the_commit_bodies_reach_the_model(self):
        with tempfile.TemporaryDirectory() as directory, Api() as api:
            repo = repository(
                directory, ["feat: a feature\n\nWhy this matters to a reader. (#42)"]
            )

            run(repo, api.url)

            asked = api.requests[0]["messages"][0]["content"]
            self.assertIn("Why this matters to a reader.", asked)
            self.assertIn("a feature", asked)

    def test_the_budget_leaves_room_to_think_and_still_answer(self):
        # v0.4.0 published unrewritten because the model's thinking spent a
        # budget sized for the notes alone, and a truncated answer is refused.
        with tempfile.TemporaryDirectory() as directory, Api() as api:
            repo = repository(directory, ["feat: a feature"])

            run(repo, api.url)

            asked = api.requests[0]
            self.assertGreaterEqual(asked["max_tokens"], 8000)
            self.assertIn("effort", asked["output_config"])

    def test_a_commit_outside_the_range_is_not_described(self):
        with tempfile.TemporaryDirectory() as directory, Api() as api:
            repo = repository(directory, ["feat: in the range"])

            run(repo, api.url)

            self.assertNotIn("chore: first", api.requests[0]["messages"][0]["content"])


class Fallback(unittest.TestCase):
    """Every one of these publishes: the generated notes are left in place and
    the script says why, which is what the release step reports."""

    def assert_fell_back(self, result, notes):
        self.assertEqual(result.returncode, 1)
        self.assertIn("### Features", notes)
        self.assertIn("release notes were not rewritten", result.stderr)

    def test_no_credential_keeps_the_generated_notes(self):
        with tempfile.TemporaryDirectory() as directory, Api() as api:
            repo = repository(directory, ["feat: a feature"])

            result, notes = run(repo, api.url, key="")

            self.assert_fell_back(result, notes)

    def test_a_rejected_key_keeps_the_generated_notes(self):
        with tempfile.TemporaryDirectory() as directory, Api(status=401) as api:
            repo = repository(directory, ["feat: a feature"])

            result, notes = run(repo, api.url)

            self.assert_fell_back(result, notes)
            self.assertEqual(len(api.requests), 1, "a rejected key is not retried")

    # These two ask in process rather than through the script, so the backoff
    # between attempts can be taken out of the run.
    def setUp(self):
        patch = unittest.mock.patch.object(rewrite, "RETRY_DELAY", 0)
        patch.start()
        self.addCleanup(patch.stop)

    def test_an_unreachable_api_is_retried_and_then_given_up_on(self):
        with self.assertRaises(rewrite.Unavailable):
            # Port 1 answers nothing.
            rewrite.ask("notes", rewrite.MODEL, "test-key", "http://127.0.0.1:1")

    def test_a_server_error_is_retried(self):
        with Api(status=503) as api:
            with self.assertRaises(rewrite.Unavailable):
                rewrite.ask("notes", rewrite.MODEL, "test-key", api.url)

            self.assertEqual(len(api.requests), rewrite.ATTEMPTS)


class Answers(unittest.TestCase):
    """`finish` is the gate between what the model said and what publishes."""

    def test_a_short_answer_is_not_notes(self):
        with self.assertRaises(rewrite.Unavailable):
            rewrite.as_published("ok", GENERATED)

    def test_a_command_a_reader_types_survives_as_one(self):
        # The release page renders a fence, so a release that has to tell a
        # reader what to type can, and refusing one would lose the command.
        answer = "## What is new\n\n```sh\nsudo pacman -U ./omaweb.pkg.tar.zst\n```"

        notes = rewrite.as_published(answer, GENERATED)

        self.assertIn("sudo pacman -U ./omaweb.pkg.tar.zst", notes)

    def test_a_table_is_refused(self):
        answer = "## What is new\n\n| Change | Why |\n| --- | --- |\n| a | b |"
        with self.assertRaises(rewrite.Unavailable):
            rewrite.as_published(answer, GENERATED)

    def test_a_rewrite_that_kept_the_link_does_not_get_a_second_one(self):
        link = "**Full changelog**: https://github.com/villekivela/omaweb/compare/v1.0.0...v1.1.0"
        notes = rewrite.as_published(f"## What is new\n\n- The thing.\n\n{link}", GENERATED)

        self.assertEqual(notes.count("**Full changelog**"), 1)
        self.assertIn(link, notes)

    def test_a_compare_url_the_model_wrote_is_replaced(self):
        invented = (
            "**Full changelog**: "
            "https://github.com/villekivela/omaweb/compare/v0.9.0...v1.1.0"
        )
        notes = rewrite.as_published(f"## What is new\n\n- The thing.\n\n{invented}", GENERATED)

        self.assertNotIn("v0.9.0", notes)
        self.assertIn("compare/v1.0.0...v1.1.0", notes)

    def test_the_notes_end_with_one_newline(self):
        notes = rewrite.as_published("## What is new\n\n- The thing that changed.\n\n\n", GENERATED)

        self.assertTrue(notes.endswith("\n"))
        self.assertFalse(notes.endswith("\n\n"))


class Truncation(unittest.TestCase):
    def test_an_answer_that_ran_out_of_tokens_is_not_published(self):
        with Api(stop_reason="max_tokens") as api:
            with self.assertRaises(rewrite.Unavailable):
                rewrite.ask("notes", rewrite.MODEL, "test-key", api.url)

    def test_the_generated_notes_stay_when_the_answer_was_truncated(self):
        with tempfile.TemporaryDirectory() as directory, Api(stop_reason="max_tokens") as api:
            repo = repository(directory, ["feat: a feature"])

            result, notes = run(repo, api.url)

            self.assertEqual(result.returncode, 1)
            self.assertIn("### Features", notes)


class Vocabulary(unittest.TestCase):
    def test_the_projects_words_reach_the_model(self):
        # v0.4.0's notes called a split a "Split view", which the glossary
        # lists as a name for it not to use. The glossary was not in the ask.
        with tempfile.TemporaryDirectory() as directory, Api() as api:
            repo = repository(directory, ["feat: a feature"])

            run(repo, api.url)

            asked = api.requests[0]["messages"][0]["content"]
            self.assertIn("**Split**", asked)
            self.assertIn("Split view (as the name of the thing)", asked)

    def test_an_unreadable_glossary_is_looser_words_not_a_failed_release(self):
        missing = rewrite.GLOSSARY
        rewrite.GLOSSARY = "/nonexistent/CONTEXT.md"
        try:
            self.assertEqual(rewrite.vocabulary(), "")
            text = rewrite.prompt("v1.1.0", GENERATED, [
                {"hash": "abc1234", "subject": "feat: a feature", "body": ""}
            ], [])
            self.assertIn("a feature", text)
        finally:
            rewrite.GLOSSARY = missing


class Issues(unittest.TestCase):
    def test_every_reference_is_looked_up_once(self):
        found = [
            {"hash": "abc1234", "subject": "feat: a feature (#42)", "body": "Closes #42 and #7."},
            {"hash": "def5678", "subject": "fix: a fix (#7)", "body": ""},
        ]

        self.assertEqual(rewrite.issue_numbers(found), [42, 7])


if __name__ == "__main__":
    unittest.main()
