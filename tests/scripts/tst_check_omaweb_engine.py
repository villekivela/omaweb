#!/usr/bin/env python3
"""Whether a configured build found Omaweb's engine, from CMake caches written here.

CI's `arch-linux` job reads this script's answer rather than trusting the prefix
path to have won, so the answer is what is tested: each way a build can end up
on a stock engine has to fail, and only the build that found Omaweb's passes.
"""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "check_omaweb_engine.sh"

OMAWEB = "/usr/lib/omaweb/lib/cmake"
STOCK = "/usr/lib/cmake"
DNS_ALIASES = "OMAWEB_ENGINE_OFFERS_DNS_ALIASES:INTERNAL=1"
NO_DNS_ALIASES = "OMAWEB_ENGINE_OFFERS_DNS_ALIASES:INTERNAL="


def packages(core: str, quick: str) -> list[str]:
    return [
        f"Qt6WebEngineCore_DIR:PATH={core}/Qt6WebEngineCore",
        f"Qt6WebEngineQuick_DIR:PATH={quick}/Qt6WebEngineQuick",
    ]


def check(lines: list[str]) -> subprocess.CompletedProcess[str]:
    with tempfile.TemporaryDirectory() as directory:
        build = Path(directory)
        cache = ["Qt6_DIR:PATH=/usr/lib/cmake/Qt6", *lines]
        (build / "CMakeCache.txt").write_text("\n".join(cache) + "\n", encoding="utf-8")
        return subprocess.run(
            ("sh", str(SCRIPT), str(build)), capture_output=True, text=True
        )


class CheckOmawebEngineTest(unittest.TestCase):
    def test_the_patched_engine_passes(self) -> None:
        result = check([*packages(OMAWEB, OMAWEB), DNS_ALIASES])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("the DNS alias API is there", result.stdout)

    # What a VM build found: CMake took one engine package from the stock
    # directory and the rest from the prefix.
    def test_one_stock_package_fails(self) -> None:
        result = check([*packages(OMAWEB, STOCK), DNS_ALIASES])
        self.assertEqual(result.returncode, 1)
        self.assertIn("outside /usr/lib/omaweb/", result.stderr)
        self.assertIn("Qt6WebEngineQuick_DIR", result.stderr)

    def test_a_stock_engine_fails(self) -> None:
        result = check([*packages(STOCK, STOCK), NO_DNS_ALIASES])
        self.assertEqual(result.returncode, 1)
        self.assertIn("outside /usr/lib/omaweb/", result.stderr)

    # The prefix won for every package, and the header still lacks the API.
    def test_an_engine_without_the_dns_alias_api_fails(self) -> None:
        result = check([*packages(OMAWEB, OMAWEB), NO_DNS_ALIASES])
        self.assertEqual(result.returncode, 1)
        self.assertIn("did not find the DNS alias API", result.stderr)

    # A probe that never ran leaves no answer, which is not a yes.
    def test_an_unasked_probe_fails(self) -> None:
        result = check(packages(OMAWEB, OMAWEB))
        self.assertEqual(result.returncode, 1)
        self.assertIn("did not find the DNS alias API", result.stderr)

    def test_a_build_with_no_engine_fails(self) -> None:
        result = check([DNS_ALIASES])
        self.assertEqual(result.returncode, 1)
        self.assertIn("names no QtWebEngine package", result.stderr)

    # A prefix that only starts like Omaweb's is somewhere else.
    def test_a_lookalike_prefix_fails(self) -> None:
        result = check([*packages(OMAWEB, "/usr/lib/omaweb-old/lib/cmake"), DNS_ALIASES])
        self.assertEqual(result.returncode, 1)
        self.assertIn("outside /usr/lib/omaweb/", result.stderr)


if __name__ == "__main__":
    unittest.main()
