#!/usr/bin/env python3
"""The fingerprint the repository check reads out of the signing key.

Everything else that check does is comparing strings between files. This is the
one computation in it, and it is the one that decides whether the key a release
trusts is the key the pages tell a reader to check by eye. The expected value
comes from the published fingerprint rather than from running the code, so the
test can disagree with the implementation.
"""

from __future__ import annotations

import base64
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import check_repository_instructions as instructions  # noqa: E402

# What README.md, the website and the repository script all publish. Written out
# rather than read from any of them: a test that took the value from the same
# place the check does would pass whatever either said.
PUBLISHED = "FDA535B2185755EA718BEA585DBF15FE484EFA64"


class KeyFingerprint(unittest.TestCase):
    def test_the_shipped_key_is_the_published_one(self):
        self.assertEqual(instructions.key_fingerprint(instructions.KEY), PUBLISHED)

    def test_a_key_with_no_armor_headers_still_reads(self):
        """Armor headers are optional, and a key exported by gpg carries none.

        The one in the repository came from a keyserver and has them, so
        without this the header-less form is untested.
        """
        original = (ROOT / instructions.KEY).read_text(encoding="utf-8").splitlines()
        stripped = [original[0]]
        body = original[1:]
        # Drop everything up to and including the blank line that ends the
        # headers, then put a blank line back, which is the header-less form.
        while body and body[0].strip():
            body.pop(0)
        stripped.append("")
        stripped.extend(body[1:] if body else [])
        self._assert_reads("\n".join(stripped) + "\n", PUBLISHED)

    def test_something_that_is_not_a_key_is_refused(self):
        for name, text in (
            ("empty", ""),
            ("prose", "This is not a key.\n"),
            (
                "an armored block holding a signature rather than a key",
                "-----BEGIN PGP PUBLIC KEY BLOCK-----\n\n"
                + base64.b64encode(b"\xc2\x01\x00").decode()
                + "\n-----END PGP PUBLIC KEY BLOCK-----\n",
            ),
        ):
            with self.subTest(name):
                with self.assertRaises(ValueError):
                    self._read(text)

    def _read(self, text):
        # An absolute path, which `ROOT / path` resolves to itself, so a
        # scratch file needs no place inside the repository.
        scratch = Path(self.enterContext(tempfile.TemporaryDirectory()))
        key = scratch / "key.asc"
        key.write_text(text, encoding="utf-8")
        return instructions.key_fingerprint(key)

    def _assert_reads(self, text, expected):
        self.assertEqual(self._read(text), expected)


if __name__ == "__main__":
    unittest.main()
