#!/usr/bin/env python3
"""Checks that no two architecture decisions hold the same number.

An ADR number is how the rest of the repository refers to a decision, so two
decisions under one number make every reference to it a guess. It has happened
once: two branches were open at the same time, neither saw the other's number,
and both merged (#274).

That is the shape of the problem. Nothing either author could have checked would
have caught it, because at the time each wrote their file the number was free.
The collision exists only in the merged tree, which is where this looks.

A gap is not an error. The check is for repeats, not for a contiguous run: a
withdrawn decision leaves a hole and the hole is correct, because the numbers
that remain still name what they always named.

    scripts/check_adr_numbers.py

Exits 0 when every number is its own, 1 when one is shared.
"""

import collections
import pathlib
import re
import sys

ADR_DIRECTORY = pathlib.Path(__file__).resolve().parent.parent / "docs" / "adr"
# `0038-own-a-windows-downloads-in-core.md`: the number, then what it decided.
NUMBERED = re.compile(r"^(\d+)-.+\.md$")


def main():
    if not ADR_DIRECTORY.is_dir():
        print(f"check_adr_numbers: no {ADR_DIRECTORY}", file=sys.stderr)
        return 1

    held_by = collections.defaultdict(list)
    unnumbered = []
    for entry in sorted(ADR_DIRECTORY.iterdir()):
        if not entry.is_file() or entry.suffix != ".md":
            continue
        match = NUMBERED.match(entry.name)
        if not match:
            unnumbered.append(entry.name)
            continue
        held_by[int(match.group(1))].append(entry.name)

    problems = []
    for number, files in sorted(held_by.items()):
        if len(files) > 1:
            problems.append(f"{number:04d} is held by {len(files)} decisions:")
            problems.extend(f"    {name}" for name in files)
    # A file in here that names no number is a decision nothing can refer to.
    for name in unnumbered:
        problems.append(f"{name} has no number, so nothing can refer to it")

    if problems:
        print("The architecture decisions do not each hold their own number:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1

    if held_by:
        print(f"{len(held_by)} architecture decisions, each holding its own number "
              f"(next free: {max(held_by) + 1:04d})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
