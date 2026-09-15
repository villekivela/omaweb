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
- The compare URL survives, carried over from the generated notes rather than
  left to the rewrite to reproduce.
- The layout is repaired rather than trusted. `repair_layout` drops a title
  restating the version, drops a heading with nothing under it, and shifts a
  body that opens at `#` down to `##`. Repaired rather than refused: refusing
  publishes the generated commit list, which is a worse page than a heading in
  the wrong place. What the sections are called is asked for in the prompt,
  because naming them is the judgement the model is there to make.

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

# Reader-facing notes out of text that is already in the prompt, with the
# parts a release cannot get wrong held in code rather than asked of the
# model. `--model` overrides this where a range turns out to need more.
MODEL = "claude-sonnet-5"
API_VERSION = "2023-06-01"

# Thinking is on by default on this model and its tokens come out of the same
# budget as the notes, so a ceiling sized for the notes alone truncates the
# answer and the truncation check turns a release into an unrewritten one.
# Low effort is what the work is: the range is already in the prompt, and the
# judgement asked for is which fix belongs to which feature.
MAX_TOKENS = 16000
EFFORT = "low"

# A range with more commits than this is summarised from the first COMMIT_LIMIT
# of them rather than from a request too large to answer. The generated notes
# in the same request still list every commit, so nothing is hidden, only
# unelaborated.
COMMIT_LIMIT = 200
BODY_LIMIT = 2000
ISSUE_LIMIT = 40
ISSUE_BODY_LIMIT = 4000

# The project's own words. `CONTEXT.md` names every domain term and, for each,
# the names not to use for it, which is exactly what a writer who has only read
# the commits would otherwise guess at: v0.4.0's notes called a split a "Split
# view", a name the glossary lists as one to avoid.
GLOSSARY = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "CONTEXT.md"
)
GLOSSARY_SECTION = "## Language"

# The step that publishes comes after this one, so the worst case here is a
# budget rather than a preference: every attempt of REQUEST_TIMEOUT plus the
# backoff between them has to leave the release time to publish inside the
# job's fifteen minutes. A request that thinks before it answers needs minutes
# rather than seconds, which is what pays for the second attempt going.
ATTEMPTS = 2
REQUEST_TIMEOUT = 180
RETRY_DELAY = 5

# A shorter answer than this is not notes, whatever the API said about it.
MINIMUM_LENGTH = 40

CHANGELOG = re.compile(r"^\*\*Full changelog\*\*: .*$", re.MULTILINE)
ISSUE_REFERENCE = re.compile(r"#(\d{1,6})\b")

# A heading, either way Markdown writes one. `SETEXT` needs the line above it,
# so it is matched against the pair rather than a single line.
ATX = re.compile(r"^(?P<hashes>#{1,6})\s+(?P<text>.*?)\s*#*\s*$")
SETEXT = re.compile(r"^(?P<rule>=+|-+)\s*$")

# A fenced block holds a command to type, and `# ` starts a shell comment as
# readily as it starts a heading. Nothing inside a fence is markup.
FENCE = re.compile(r"^\s{0,3}(?:`{3,}|~{3,})")

# What a title restating the release looks like. The version alone, or the
# project's name and the version, with or without the words around them: the
# page already heads the notes with the version, so any of these says it twice.
TITLE = re.compile(
    r"^(?:release\s+notes?\s*(?:for)?\s*)?(?:omaweb\s*)?v?\d[\w.+-]*\s*"
    r"(?:release\s*notes?)?$",
    re.IGNORECASE,
)


def headings(body: str) -> list[tuple[int, int, str]]:
    """Every heading as `(line, level, text)`, both spellings, in order."""
    lines = body.split("\n")
    found = []
    fenced = False
    for number, line in enumerate(lines):
        if FENCE.match(line):
            fenced = not fenced
            continue
        if fenced:
            continue
        atx = ATX.match(line)
        if atx:
            found.append((number, len(atx.group("hashes")), atx.group("text").strip()))
            continue
        rule = SETEXT.match(line)
        # A rule underlines the line above it only when that line is text. With
        # a blank line above it is a thematic break, which heads nothing.
        if rule and number and lines[number - 1].strip() and not ATX.match(lines[number - 1]):
            level = 1 if rule.group("rule").startswith("=") else 2
            found.append((number - 1, level, lines[number - 1].strip()))
    return found


def drop_heading(lines: list[str], line: int, level: int) -> None:
    """Blank out the heading at `line`, and the rule under it when it has one."""
    lines[line] = ""
    under = line + 1
    if under < len(lines) and SETEXT.match(lines[under]) and not ATX.match(lines[line]):
        lines[under] = ""
    del level


def repair_layout(body: str) -> str:
    """The shape a release page can head, out of the shape the model wrote.

    Three repairs, each unambiguous enough to make without guessing at prose. A
    body that needs none comes back as it went in. Refusing any of these would
    publish the generated commit list instead, which is a worse page than a
    title in the wrong place.
    """
    lines = body.split("\n")

    # A leading title restates the version the page's own `h1` already carries.
    # Only a leading one: `## v0.4.0 in detail` further down is a section.
    marks = headings(body)
    if marks:
        line, level, text = marks[0]
        if not any(lines[index].strip() for index in range(line)) and TITLE.match(text):
            drop_heading(lines, line, level)
            marks = marks[1:]

    # A section that heads nothing is noise. The prompt says so; this is what
    # enforces it. The last heading counts too, when the body ends under it.
    for index, (line, level, _) in enumerate(marks):
        following = marks[index + 1][0] if index + 1 < len(marks) else len(lines)
        under = range(line + 1, following)
        if not any(lines[number].strip() and not SETEXT.match(lines[number]) for number in under):
            drop_heading(lines, line, level)

    body = "\n".join(lines)

    # The page's outline starts under its own `h2`, so a body opens at `##`.
    # Shifted rather than clamped: a body written `#`/`##` keeps the difference
    # between its levels, which is the only thing its depth was saying.
    marks = headings(body)
    if marks and min(level for _, level, _ in marks) < 2:
        lines = body.split("\n")
        for line, level, text in reversed(marks):
            under = line + 1
            if under < len(lines) and SETEXT.match(lines[under]):
                lines[under] = ""
            lines[line] = f"{'#' * min(level + 1, 6)} {text}"
        body = "\n".join(lines)

    # Repairs leave blank lines where they took something out.
    return re.sub(r"\n{3,}", "\n\n", body).strip()

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
about what a reader has to do. Write that heading only when the range has one: \
a heading that announces nothing is noise, so leave the section out entirely \
rather than heading it and saying none. The same goes for every other section.
- Put repairs last, under a `## Fixes` heading, when the range has any left \
after folding. Those two headings are the skeleton and they are the only names \
fixed in advance; name the sections between them for what actually changed, \
because `## Linux platform integration` tells a reader more than `## What's \
new` does.
- Say only what the commits and issues say. You have no other source for this \
project's behavior, so anything you cannot point at in the input does not go in \
the notes.
- Call things what the project calls them. The vocabulary below is the \
project's own, and each entry ends with the names that are not to be used for \
that thing. A commit or an issue that uses one of those names is not licence to \
repeat it.
- Leave out changes with no reader-visible effect unless the range is nothing \
else. Internal work, refactors, and test changes belong in the commit list, \
which the compare URL reaches.

Format: Markdown, starting at `##` for the sections. Never write a `#` \
heading, and do not open with a title naming the release: the page these notes \
become already heads them with the version, so a title repeats it. Do not wrap \
the whole answer in a fence and do not introduce it. The first line is the \
first line of the notes.\
"""


class Unavailable(Exception):
    """The rewrite did not happen. The caller publishes the generated notes."""


def run(*arguments: str) -> tuple[int, str]:
    result = subprocess.run(arguments, capture_output=True, text=True)
    return result.returncode, result.stdout if result.returncode == 0 else result.stderr


def git(*arguments: str) -> str:
    status, output = run("git", *arguments)
    if status != 0:
        raise Unavailable(f"git {' '.join(arguments)}: {output.strip()}")
    return output


def previous_tag(tag: str) -> str:
    """The tag the range starts at, by the rule `release_notes.sh` uses. Empty
    when this is the first release: the range is then the whole history."""
    status, output = run("git", "describe", "--tags", "--abbrev=0", "--match", "v[0-9]*",
                         f"{tag}^")
    return output.strip() if status == 0 else ""


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


def issue_numbers(changes: list[dict[str, str]]) -> list[int]:
    numbers: list[int] = []
    for commit in changes:
        for match in ISSUE_REFERENCE.finditer(f"{commit['subject']}\n{commit['body']}"):
            number = int(match.group(1))
            if number not in numbers:
                numbers.append(number)
    return numbers[:ISSUE_LIMIT]


def issues(numbers: list[int]) -> list[dict[str, str]]:
    """What each referenced issue asked for. A number that does not resolve is
    left out: an unreachable issue is less context, not a failed release.

    Read through `gh api` rather than `gh issue view`, because a `#nnn` in a
    subject is as often a pull request, and the issues endpoint answers for
    both while `gh issue view` refuses one of them."""
    fetched = []
    for number in numbers:
        status, output = run("gh", "api", f"repos/{{owner}}/{{repo}}/issues/{number}")
        if status != 0:
            continue
        try:
            issue = json.loads(output)
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


def vocabulary() -> str:
    """The glossary section of `CONTEXT.md`, or nothing. An unreadable glossary
    is notes in looser words, not a failed release."""
    try:
        with open(GLOSSARY, encoding="utf-8") as handle:
            text = handle.read()
    except OSError:
        return ""
    start = text.find(f"\n{GLOSSARY_SECTION}\n")
    if start < 0:
        return ""
    end = text.find("\n## ", start + 1)
    return text[start:end if end > 0 else len(text)].strip()


def prompt(tag: str, generated: str, changes: list[dict[str, str]],
           referenced: list[dict[str, str]]) -> str:
    parts = [f"Release: {tag}", "", "## Generated notes", "", generated.strip(), "", "## Commits"]
    for commit in changes:
        parts.append("")
        parts.append(f"### {commit['hash']} {commit['subject']}")
        if commit["body"]:
            parts.append("")
            parts.append(commit["body"])
    words = vocabulary()
    if words:
        parts.append("")
        parts.append("## The project's vocabulary")
        parts.append("")
        parts.append(words)
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
            "output_config": {"effort": EFFORT},
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
            with urllib.request.urlopen(request, timeout=REQUEST_TIMEOUT) as response:
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

    # Notes that ran out of tokens end mid-sentence, and every check below
    # passes on them, so the truncation would publish looking deliberate.
    if answer.get("stop_reason") == "max_tokens":
        raise Unavailable("the answer ran out of tokens")

    blocks = [
        block.get("text", "")
        for block in answer.get("content", [])
        if block.get("type") == "text"
    ]
    return "".join(blocks).strip()


def as_published(rewritten: str, generated: str) -> str:
    """The gate between what the model said and what a release publishes:
    either the notes as they go out, or `Unavailable`."""
    if len(rewritten) < MINIMUM_LENGTH:
        raise Unavailable("the answer was too short to be notes")

    rewritten = repair_layout(rewritten)
    if len(rewritten) < MINIMUM_LENGTH:
        raise Unavailable("the answer was a layout with nothing under it")

    # A compare URL the model wrote is a URL nobody checked, and one that names
    # the wrong range reads exactly like one that names the right one. Whatever
    # it wrote goes, and the generated line takes its place.
    link = CHANGELOG.search(generated)
    rewritten = CHANGELOG.sub("", rewritten).rstrip()
    if link:
        rewritten = f"{rewritten}\n\n{link.group(0)}"
    return f"{rewritten.rstrip()}\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag", help="the tag being released")
    parser.add_argument("--generated", required=True, help="the notes release_notes.sh wrote")
    parser.add_argument("--output", required=True, help="where the rewritten notes go")
    parser.add_argument("--model", default=MODEL)
    arguments = parser.parse_args()

    key = os.environ.get("ANTHROPIC_API_KEY", "")
    base_url = os.environ.get("ANTHROPIC_BASE_URL", "https://api.anthropic.com")

    try:
        if not key:
            raise Unavailable("no ANTHROPIC_API_KEY")
        with open(arguments.generated, encoding="utf-8") as handle:
            generated = handle.read()
        previous = previous_tag(arguments.tag)
        changes = commits(arguments.tag, previous)
        if not changes:
            raise Unavailable(f"no commits in {previous or 'the range'}..{arguments.tag}")
        text = prompt(arguments.tag, generated, changes, issues(issue_numbers(changes)))
        notes = as_published(ask(text, arguments.model, key, base_url), generated)
    except (Unavailable, OSError) as error:
        print(f"release notes were not rewritten: {error}", file=sys.stderr)
        return 1

    with open(arguments.output, "w", encoding="utf-8") as handle:
        handle.write(notes)
    return 0


if __name__ == "__main__":
    sys.exit(main())
