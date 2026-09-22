#!/usr/bin/env python3
"""Checks that everything naming the [omaweb] repository names the same one.

A reader adds the repository from whichever page they happened to open, so the
address they configure and the key they trust have to be identical in both.
Nothing else would report a disagreement: each page is correct on its own, and a
fingerprint that is right in one place and stale in the other fails at the
reader's machine with a signature error, which is the worst place to find out.

The release workflow is the third place, because it installs the engine from
that same repository to build against it (ADR 0049). A workflow trusting a key
the pages do not publish would build a release against an engine nobody could
verify.

And the key file is the fourth, which is the one that is not a copy: the other
three state a fingerprint, `security/repo-signing-key.asc` is the key that has
one. The fingerprint is computed from the key packet here rather than by asking
gpg, so this runs the same way wherever it runs.

The fingerprint is the reason this exists. It is the one value a reader is asked
to check by eye, and it is the anchor for everything else: a keyserver hands out
any key that claims a name, so naming the one key it has to be is what makes
`pacman-key --recv-keys` safe to run.

    scripts/check_repository_instructions.py

Exits 0 when the two agree, 1 when they do not.
"""

import base64
import hashlib
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PAGES = ("README.md", "website/pages/index.html", ".github/workflows/release.yml")
KEY = "security/repo-signing-key.asc"

# A full OpenPGP fingerprint, which is the only form worth publishing: a short
# key id can be collided with, and a reader comparing one is checking nothing.
FINGERPRINT = re.compile(r"\b([0-9A-F]{40})\b")
# Stops at the first `<` or `'`, because one page is HTML and the address is
# followed there by the tags that close the line, and the workflow writes it
# inside a quoted shell string so that `$arch` reaches pacman unexpanded.
SERVER = re.compile(r"Server = ([^\s<']+)")


def stated(page):
    """What one page says the repository is, as (fingerprints, servers)."""
    text = (ROOT / page).read_text(encoding="utf-8")
    return set(FINGERPRINT.findall(text)), set(SERVER.findall(text))


def key_fingerprint(path):
    """The fingerprint of the public key in an armored OpenPGP file.

    A v4 fingerprint is SHA-1 over the public key packet's body with a fixed
    three-byte preamble (RFC 4880 section 12.2), which is short enough to do
    here and saves the check depending on a gpg being installed.
    """
    lines = (ROOT / path).read_text(encoding="utf-8").splitlines()
    try:
        start = lines.index("-----BEGIN PGP PUBLIC KEY BLOCK-----") + 1
    except ValueError:
        raise ValueError(f"{path} is not an armored public key block")
    # Armor headers, then a blank line, then the data. A block with no headers
    # starts at the blank line.
    while start < len(lines) and lines[start].strip():
        start += 1
    start += 1
    encoded = []
    for line in lines[start:]:
        if line.startswith("-----END"):
            break
        # The CRC24 line, which is not part of the packets.
        if line.startswith("="):
            continue
        encoded.append(line)
    data = base64.b64decode("".join(encoded))

    first = data[0]
    if not first & 0x80:
        raise ValueError(f"{path} does not begin with an OpenPGP packet")
    if first & 0x40:
        tag = first & 0x3F
        octet = data[1]
        if octet < 192:
            header, length = 2, octet
        elif octet < 224:
            header, length = 3, ((octet - 192) << 8) + data[2] + 192
        else:
            raise ValueError(f"{path} uses a packet length this does not read")
    else:
        tag = (first >> 2) & 0x0F
        of_type = first & 0x03
        if of_type == 0:
            header, length = 2, data[1]
        elif of_type == 1:
            header, length = 3, int.from_bytes(data[1:3], "big")
        elif of_type == 2:
            header, length = 5, int.from_bytes(data[1:5], "big")
        else:
            raise ValueError(f"{path} has a packet of indeterminate length")
    if tag != 6:
        raise ValueError(f"{path} begins with packet {tag} rather than a public key")

    packet = data[header:header + length]
    if packet[0] != 4:
        raise ValueError(f"{path} is a version {packet[0]} key, and only 4 is read here")
    preamble = b"\x99" + length.to_bytes(2, "big")
    return hashlib.sha1(preamble + packet).hexdigest().upper()


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

    # The key itself, against what the three places say it is. Checked after
    # the pages agree, because a disagreement between them is the more useful
    # thing to report first.
    if not problems:
        published = sorted(set().union(*fingerprints.values()))[0]
        try:
            actual = key_fingerprint(KEY)
        except (OSError, ValueError, IndexError) as unreadable:
            problems.append(f"{KEY} could not be read: {unreadable}")
        else:
            if actual != published:
                problems.append(
                    f"{KEY} is {actual} and every page publishes {published}"
                )

    if problems:
        print("The repository instructions do not agree:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1

    fingerprint = sorted(set().union(*fingerprints.values()))[0]
    server = sorted(set().union(*servers.values()))[0]
    print(f"repository instructions agree: {server} signed by {fingerprint}")
    print(f"and {KEY} is that key")
    return 0


if __name__ == "__main__":
    sys.exit(main())
