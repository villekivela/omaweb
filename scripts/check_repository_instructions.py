#!/usr/bin/env python3
"""Checks that the README and the website say the same thing about the repository.

A reader adds the [omaweb] repository from whichever of the two they happened to
open, so the address they configure and the key they trust have to be identical
in both. Nothing else would report a disagreement: each page is correct on its
own, and a fingerprint that is right in one place and stale in the other fails
at the reader's machine with a signature error, which is the worst place to find
out.

The fingerprint is the reason this exists. It is the one value a reader is asked
to check by eye, and it is the anchor for everything else: a keyserver hands out
any key that claims a name, so naming the one key it has to be is what makes
`pacman-key --recv-keys` safe to run.

    scripts/check_repository_instructions.py

Exits 0 when the two agree, 1 when they do not.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PAGES = ("README.md", "website/pages/index.html")

# A full OpenPGP fingerprint, which is the only form worth publishing: a short
# key id can be collided with, and a reader comparing one is checking nothing.
FINGERPRINT = re.compile(r"\b([0-9A-F]{40})\b")
# Stops at the first `<`, because one of the two pages is HTML and the address
# is followed there by the tags that close the line.
SERVER = re.compile(r"Server = ([^\s<]+)")


def stated(page):
    """What one page says the repository is, as (fingerprints, servers)."""
    text = (ROOT / page).read_text(encoding="utf-8")
    return set(FINGERPRINT.findall(text)), set(SERVER.findall(text))


def main():
    problems = []
    fingerprints = {}
    servers = {}

    for page in PAGES:
        found, addresses = stated(page)
        if not found:
            problems.append(f"{page} publishes no signing key fingerprint")
        if len(found) > 1:
            problems.append(f"{page} publishes {len(found)} different fingerprints: {sorted(found)}")
        if not addresses:
            problems.append(f"{page} names no repository address")
        if len(addresses) > 1:
            problems.append(f"{page} names {len(addresses)} different addresses: {sorted(addresses)}")
        fingerprints[page] = found
        servers[page] = addresses

    # The point of the check: not that each page is self-consistent, but that
    # the two agree with each other.
    for name, stated_by_page in (("fingerprint", fingerprints), ("repository address", servers)):
        values = set().union(*stated_by_page.values())
        if len(values) > 1:
            problems.append(
                f"the pages disagree about the {name}: "
                + ", ".join(f"{page} says {sorted(found)}" for page, found in stated_by_page.items())
            )

    if problems:
        print("The repository instructions do not agree:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1

    fingerprint = sorted(set().union(*fingerprints.values()))[0]
    server = sorted(set().union(*servers.values()))[0]
    print(f"repository instructions agree: {server} signed by {fingerprint}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
