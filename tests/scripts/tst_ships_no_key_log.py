#!/usr/bin/env python3
"""A browser built without `OMAWEB_KEY_EVENT_LOG` carries no key log.

The runtime budget builds the browser with the log, which writes what each key would type, so a
failed attempt at a Space can say where its keys went (#795). A browser built to ship must not be
able to write what its reader typed, whatever logging rules they set for a bug report, so neither
the category nor the function that installs the log may be in it at all.

Usage: tst_ships_no_key_log.py <browser executable>
"""

from __future__ import annotations

import sys
from pathlib import Path

# The log's category, and the function `main` would call to install it.
MARKS = (b"omaweb.keys", b"installKeyEventLog")


def main(arguments: list[str]) -> int:
    if len(arguments) != 2:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    binary = Path(arguments[1]).read_bytes()
    found = [mark.decode() for mark in MARKS if mark in binary]
    if found:
        print(f"{arguments[1]} carries the key log: {', '.join(found)}")
        return 1
    print(f"{arguments[1]} carries no key log")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
