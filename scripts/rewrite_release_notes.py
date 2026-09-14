#!/usr/bin/env python3
"""Rewrite generated release notes into notes addressed to a reader.

`scripts/release_notes.sh` groups commit subjects by Conventional Commit type.
That output is accurate and close to useless to anyone who did not write the
commits: subjects written for other maintainers end up doing a job they were
never written for, and a fix that repairs an unreleased commit in the same
range sits beside the feature it repairs as if the two were separate news.

This reads those generated notes, the commit bodies behind them, and the issues
they reference, and asks a model for the same range written as what a reader
gets from upgrading. The generated notes stay the input and the fallback: a
release publishes them unchanged when anything here fails, so a missing
credential or an unreachable API is a plainer release rather than no release.

Two things are guaranteed here rather than asked for, because a release body is
published unattended and nothing reviews it in between:

- The compare URL survives. It is the one line that makes the commit detail
  reachable, so it is carried over from the generated notes rather than left to
  the rewrite to reproduce.
- The result stays inside the Markdown subset `website/build/render.mjs`
  renders, since the same body becomes the release page on the website.

Usage:

    scripts/rewrite_release_notes.py v0.4.0 --generated notes.md --output notes.md
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request

MODEL = "claude-opus-5"
API_VERSION = "2023-06-01"
MAX_TOKENS = 4000

# A range with more commits than this is summarised from the first COMMIT_LIMIT
# of them rather than from a request too large to answer. The generated notes
# in the same request still list every commit, so nothing is hidden, only
# unelaborated.
COMMIT_LIMIT = 200
BODY_LIMIT = 2000
ISSUE_LIMIT = 40
ISSUE_BODY_LIMIT = 4000

ATTEMPTS = 3
RETRY_DELAY = 5

# A shorter answer than this is not notes, whatever the API said about it.
MINIMUM_LENGTH = 40

CHANGELOG = re.compile(r"^\*\*Full changelog\*\*: .*$", re.MULTILINE)
ISSUE_REFERENCE = re.compile(r"#(\d{1,6})\b")

# What `markdownToHtml` has no rule for arrives as the paragraph text it reads
# as. For most constructs that is merely plain, but a fence and a table row
# render as their own punctuation, so the rewrite is rejected over them.
FENCE = re.compile(r"^\s*(```|~~~)", re.MULTILINE)
TABLE_ROW = re.compile(r"^\s*\|.*\|\s*$", re.MULTILINE)

SYSTEM = """\
You write the release notes for Omaweb, a web browser. You are given the notes \
generated from the commit range, the commit bodies behind them, and the issues \
those commits reference. You return the notes the release publishes.

Write for someone deciding whether to upgrade, not for the people who wrote the \
commits.

- Lead with what the reader gets. Group by what changed for them, not by \
Conventional Commit type.
- Fold a fix that repairs an earlier commit in this same range into the change \
it repairs. Neither the reader nor the release ever saw the broken state, so \
reporting the repair as news is reporting a state that never shipped.
- Keep breaking changes first, under a `## Breaking changes` heading, and exact \
about what a reader has to do.
- Say only what the commits and issues say. You have no other source for this \
project's behavior, so anything you cannot point at in the input does not go in \
the notes.
- Leave out changes with no reader-visible effect unless the range is nothing \
else. Internal work, refactors, and test changes belong in the commit list, \
which the compare URL reaches.

Format: Markdown, using only `##` headings, `-` lists, paragraphs, and inline \
`**bold**`, `` `code` `` and `[links](https://example.com)`. No code fences, no \
tables, no nested lists, no images. Do not wrap the answer in a fence and do \
not introduce it. The first line is the first line of the notes.\
"""


class Unavailable(Exception):
    """The rewrite did not happen. The caller publishes the generated notes."""


def git(*arguments: str) -> str:
    result = subprocess.run(("git", *arguments), capture_output=True, text=True)
    if result.returncode != 0:
        raise Unavailable(f"git {' '.join(arguments)}: {result.stderr.strip()}")
    return result.stdout


def previous_tag(tag: str) -> str:
    """The tag the range starts at, by the rule `release_notes.sh` uses. Empty
    when this is the first release: the range is then the whole history."""
    result = subprocess.run(
        ("git", "describe", "--tags", "--abbrev=0", "--match", "v[0-9]*", f"{tag}^"),
        capture_output=True,
        text=True,
    )
    return result.stdout.strip() if result.returncode == 0 else ""


def commits(tag: str, previous: str) -> list[dict[str, str]]:
    """Every non-merge commit in the range, subject and body. Merge subjects are
    GitHub's, and the commits they carry are already in the range."""
    span = f"{previous}..{tag}" if previous else tag
    # A record separator that cannot occur in a commit message, so a body with
    # blank lines in it stays one record.
    log = git("log", "--no-merges", "--format=%h%x1f%s%x1f%b%x1e", span)
    found = []
    for record in log.split("\x1e"):
        record = record.strip("\n")
        if not record:
            continue
        short, subject, body = record.split("\x1f", 2)
        found.append({"hash": short, "subject": subject, "body": body.strip()[:BODY_LIMIT]})
    return found[:COMMIT_LIMIT]


def issue_numbers(found: list[dict[str, str]]) -> list[int]:
    numbers: list[int] = []
    for commit in found:
        for match in ISSUE_REFERENCE.finditer(f"{commit['subject']}\n{commit['body']}"):
            number = int(match.group(1))
            if number not in numbers:
                numbers.append(number)
    return numbers[:ISSUE_LIMIT]


def issues(numbers: list[int]) -> list[dict[str, str]]:
    """What each referenced issue asked for. A number that does not resolve is
    left out: an unreachable issue is less context, not a failed release."""
    fetched = []
    for number in numbers:
        result = subprocess.run(
            ("gh", "issue", "view", str(number), "--json", "number,title,body"),
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            continue
        try:
            issue = json.loads(result.stdout)
        except json.JSONDecodeError:
            continue
        fetched.append(
            {
                "number": str(issue.get("number", number)),
                "title": str(issue.get("title", "")),
                "body": str(issue.get("body", ""))[:ISSUE_BODY_LIMIT],
            }
        )
    return fetched


def prompt(tag: str, generated: str, found: list[dict[str, str]],
           referenced: list[dict[str, str]]) -> str:
    parts = [f"Release: {tag}", "", "## Generated notes", "", generated.strip(), "", "## Commits"]
    for commit in found:
        parts.append("")
        parts.append(f"### {commit['hash']} {commit['subject']}")
        if commit["body"]:
            parts.append("")
            parts.append(commit["body"])
    if referenced:
        parts.append("")
        parts.append("## Issues these commits reference")
        for issue in referenced:
            parts.append("")
            parts.append(f"### #{issue['number']} {issue['title']}")
            if issue["body"]:
                parts.append("")
                parts.append(issue["body"])
    return "\n".join(parts)


def ask(text: str, model: str, key: str, base_url: str) -> str:
    """The model's answer, or `Unavailable`. A 429 or a 5xx is retried, because
    a release is cut once and the next attempt is a human noticing days later.
    A 4xx is not: a rejected key or an unknown model answers the same way every
    time."""
    payload = json.dumps(
        {
            "model": model,
            "max_tokens": MAX_TOKENS,
            "system": SYSTEM,
            "messages": [{"role": "user", "content": text}],
        }
    ).encode()
    request = urllib.request.Request(
        f"{base_url.rstrip('/')}/v1/messages",
        data=payload,
        headers={
            "content-type": "application/json",
            "x-api-key": key,
            "anthropic-version": API_VERSION,
        },
    )

    last = ""
    for attempt in range(1, ATTEMPTS + 1):
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                answer = json.load(response)
            break
        except urllib.error.HTTPError as error:
            last = f"HTTP {error.code}"
            error.close()
            if error.code != 429 and error.code < 500:
                raise Unavailable(last) from error
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as error:
            last = str(error)
        if attempt < ATTEMPTS:
            time.sleep(RETRY_DELAY * attempt)
    else:
        raise Unavailable(last)

    blocks = [
        block.get("text", "")
        for block in answer.get("content", [])
        if block.get("type") == "text"
    ]
    return "".join(blocks).strip()


def finish(rewritten: str, generated: str) -> str:
    """The rewrite as it publishes, or `Unavailable` when it is not notes."""
    if len(rewritten) < MINIMUM_LENGTH:
        raise Unavailable("the answer was too short to be notes")
    if FENCE.search(rewritten) or TABLE_ROW.search(rewritten):
        raise Unavailable("the answer used Markdown the release page cannot render")

    link = CHANGELOG.search(generated)
    if link and not CHANGELOG.search(rewritten):
        rewritten = f"{rewritten}\n\n{link.group(0)}"
    return f"{rewritten.rstrip()}\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag", help="the tag being released")
    parser.add_argument("--generated", required=True, help="the notes release_notes.sh wrote")
    parser.add_argument("--output", required=True, help="where the rewritten notes go")
    parser.add_argument("--previous", default="", help="the tag the range starts at")
    parser.add_argument("--model", default=MODEL)
    arguments = parser.parse_args()

    key = os.environ.get("ANTHROPIC_API_KEY", "")
    base_url = os.environ.get("ANTHROPIC_BASE_URL", "https://api.anthropic.com")

    try:
        if not key:
            raise Unavailable("no ANTHROPIC_API_KEY")
        with open(arguments.generated, encoding="utf-8") as handle:
            generated = handle.read()
        previous = arguments.previous or previous_tag(arguments.tag)
        found = commits(arguments.tag, previous)
        if not found:
            raise Unavailable(f"no commits in {previous or 'the range'}..{arguments.tag}")
        text = prompt(arguments.tag, generated, found, issues(issue_numbers(found)))
        notes = finish(ask(text, arguments.model, key, base_url), generated)
    except (Unavailable, OSError) as error:
        print(f"release notes were not rewritten: {error}", file=sys.stderr)
        return 1

    with open(arguments.output, "w", encoding="utf-8") as handle:
        handle.write(notes)
    return 0


if __name__ == "__main__":
    sys.exit(main())
