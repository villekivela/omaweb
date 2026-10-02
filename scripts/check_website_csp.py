#!/usr/bin/env python3
"""Confirms the website can still load under the policy it ships.

`website/vercel.json` sends `default-src 'self'` with no exceptions, so the
page may load nothing from another origin, run no inline script, and carry no
inline style attribute. Nothing in a browser tells an author when that breaks:
the policy is enforced at the reader's end, the deploy succeeds, and the first
sign is a font or a stylesheet silently missing for everyone.

So this reads the shipped files and fails on the additions that would violate
the policy, before the policy meets a reader.

Blocked by `default-src 'self'`:

- a `src` or `href` naming another origin, or a protocol-relative `//host/path`
- `url()` in a stylesheet naming another origin
- a `<script>` with a body and no `src`, which `script-src 'self'` blocks
- a script importing or fetching from another origin, which `script-src` and
  `connect-src` block
- a `style` attribute, which `style-src 'self'` blocks, whether it is in the
  HTML or in markup a script builds; a script setting `element.style` goes
  through the CSSOM, which the policy allows

`<a href>` is a navigation rather than a subresource, so a link to github.com
is left alone. `application/ld+json` is data the browser never executes, so it
is left alone too. `data:` is reported: the policy admits no `data:` today, and
adding one means widening the policy on purpose rather than by accident.

Usage:

    scripts/check_website_csp.py [--website DIR]
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

WEBSITE = Path(__file__).resolve().parent.parent / "website"

# Not served, so not this check's to read. `dist/` is where
# `website/build/site.mjs` writes the deployable site, skipped so this check
# reads the same files whether or not someone has run a local build; what lands
# there is the sources plus the release markup, and `website/build/render.mjs`
# emits links and nothing the browser fetches. `build/` and `tests/` run in
# Node, and `node_modules/` is the build's Markdown parser: a package shipping
# an example page would otherwise fail a policy it never meets.
UNSERVED = ("dist", "build", "tests", "node_modules")

# An origin this page may not reach: an absolute URL, or a protocol-relative
# one. `data:` is caught separately so it can say something more useful.
FOREIGN = re.compile(r"""^(?:[a-zA-Z][a-zA-Z0-9+.-]*:)?//""")

# A subresource the browser fetches. `<a href>` and `<form action>` navigate
# instead, and the policy governs those with `frame-ancestors` and
# `form-action`.
SUBRESOURCE = re.compile(
    r"""<(?P<tag>script|img|source|iframe|embed|video|audio|track|link)\b[^>]*>""",
    re.IGNORECASE,
)
ATTRIBUTE = re.compile(r"""\b(?P<name>src|href)\s*=\s*["'](?P<value>[^"']*)["']""", re.I)
STYLE_ATTRIBUTE = re.compile(r"""\bstyle\s*=\s*\\?["']""", re.IGNORECASE)
# What a script loads by address: a static or dynamic import, a fetch, and the
# constructors that open a connection or a worker.
SCRIPT_LOAD = re.compile(
    r"""(?:\bimport\s*(?:[\w${}\s,*]+\s+from\s*)?\(?|\bfetch\s*\(|"""
    r"""\bnew\s+(?:Worker|SharedWorker|EventSource|WebSocket)\s*\()\s*["'`](?P<value>[^"'`]+)["'`]"""
)
CSS_URL = re.compile(r"""url\(\s*["']?(?P<value>[^"')]+)["']?\s*\)""", re.IGNORECASE)
SCRIPT_BLOCK = re.compile(
    r"""<script(?P<attributes>[^>]*)>(?P<body>.*?)</script>""",
    re.IGNORECASE | re.DOTALL,
)
SCRIPT_TYPE = re.compile(r"""\btype\s*=\s*["'](?P<value>[^"']*)["']""", re.IGNORECASE)

# Types a browser does not execute, so `script-src` never reaches them.
DATA_TYPES = {"application/ld+json", "application/json", "importmap", "speculationrules"}


def line_of(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


def report(problems: list[str], path: Path, line: int, message: str) -> None:
    problems.append(f"{path}:{line}: {message}")


def check_markup(path: Path, problems: list[str]) -> None:
    text = path.read_text(encoding="utf-8")

    for element in SUBRESOURCE.finditer(text):
        for attribute in ATTRIBUTE.finditer(element.group(0)):
            value = attribute.group("value").strip()
            line = line_of(text, element.start())
            if FOREIGN.match(value):
                report(problems, path, line, f"{element.group('tag')} loads {value}")
            elif value.startswith("data:"):
                report(problems, path, line, f"{element.group('tag')} loads a data: URI")

    for block in SCRIPT_BLOCK.finditer(text):
        if "src" in ATTRIBUTE.sub("src", block.group("attributes")):
            continue
        if not block.group("body").strip():
            continue
        declared = SCRIPT_TYPE.search(block.group("attributes"))
        if declared and declared.group("value").strip().lower() in DATA_TYPES:
            continue
        report(problems, path, line_of(text, block.start()), "inline script")

    for attribute in STYLE_ATTRIBUTE.finditer(text):
        report(problems, path, line_of(text, attribute.start()), "style attribute")


def check_script(path: Path, problems: list[str]) -> None:
    """Markup a script builds meets the same policy as markup in a page."""
    text = path.read_text(encoding="utf-8")
    for load in SCRIPT_LOAD.finditer(text):
        value = load.group("value").strip()
        if FOREIGN.match(value) or value.startswith("data:"):
            report(problems, path, line_of(text, load.start()), f"script loads {value}")
    for attribute in STYLE_ATTRIBUTE.finditer(text):
        report(problems, path, line_of(text, attribute.start()), "style attribute")


def check_stylesheet(path: Path, problems: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    for reference in CSS_URL.finditer(text):
        value = reference.group("value").strip()
        line = line_of(text, reference.start())
        if FOREIGN.match(value):
            report(problems, path, line, f"url() loads {value}")
        elif value.startswith("data:"):
            report(problems, path, line, "url() loads a data: URI")


def check_policy(policy: Path, problems: list[str]) -> None:
    """The policy this script assumes is the one the site actually sends."""
    if not policy.exists():
        problems.append(f"{policy}: missing")
        return
    document = json.loads(policy.read_text(encoding="utf-8"))
    sent = {
        header["key"].lower(): header["value"]
        for rule in document.get("headers", [])
        for header in rule.get("headers", [])
    }
    if "default-src 'self'" not in sent.get("content-security-policy", ""):
        problems.append(
            f"{policy}: no `default-src 'self'`, so this check no longer describes the site"
        )
    # Every directive names this origin or nothing, so a source added beside `default-src`, a
    # `media-src` for a film hosted elsewhere for example, is a change to the policy too.
    for directive in sent.get("content-security-policy", "").split(";"):
        if not directive.strip():
            continue
        name, *sources = directive.split()
        if not name.endswith("-src") and name not in {"base-uri", "form-action", "frame-ancestors"}:
            continue
        for source in sources:
            if source not in {"'self'", "'none'"}:
                problems.append(f"{policy}: `{name}` lets in {source}, beyond this origin")


def served(website: Path, pattern: str) -> list[Path]:
    """The site's files matching `pattern`, leaving out what is never served."""
    return sorted(
        path
        for path in website.rglob(pattern)
        if not set(path.relative_to(website).parts) & set(UNSERVED)
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--website", type=Path, default=WEBSITE, help="the site to check")
    website = parser.parse_args().website.resolve()
    problems: list[str] = []
    check_policy(website / "vercel.json", problems)
    for path in served(website, "*.html"):
        check_markup(path, problems)
    for path in served(website, "*.css"):
        check_stylesheet(path, problems)
    for pattern in ("*.js", "*.mjs"):
        for path in served(website, pattern):
            check_script(path, problems)
    # Named from the directory holding the site, as `website/index.html:12`.
    problems = [problem.replace(f"{website.parent}/", "") for problem in problems]

    if problems:
        print("The website would break under its own Content-Security-Policy:\n")
        for problem in problems:
            print(f"  {problem}")
        print(
            "\nEither keep the resource on this origin, or widen the policy in "
            "website/vercel.json on purpose."
        )
        return 1

    print("website: every subresource is same-origin, no inline script or style")
    return 0


if __name__ == "__main__":
    sys.exit(main())
