#!/usr/bin/env python3
"""The engine a package depends on and the capability its build reports, kept together.

Whether Omaweb offers a Known extension is decided when it is compiled, by
`OMAWEB_KNOWN_EXTENSIONS`, and whether it can host one is decided by the engine
the package depends on. The two live in different parts of the PKGBUILD, and
v0.7.1 shipped with the dependency and without the option, so Settings told a
reader running the patched engine that it ran the system's (#365). A build of
the package takes too long to be where this is caught, so the lines that decide
it are read instead, from both the source package and the one a release
derives from it.
"""

from __future__ import annotations

import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SOURCE_PKGBUILD = ROOT / "packaging" / "PKGBUILD"
MAKE_RELEASE = ROOT / "scripts" / "make_release_pkgbuild.sh"

ENGINE = "omaweb-qtwebengine"
OPTION = "OMAWEB_KNOWN_EXTENSIONS"
FLAG = f"-D{OPTION}=ON"


def depends(pkgbuild: str) -> list[str]:
    body = re.search(r"^depends=\((.*?)^\)", pkgbuild, re.MULTILINE | re.DOTALL)
    if not body:
        raise AssertionError("the PKGBUILD has no depends array")
    lines = (line.split("#", 1)[0] for line in body.group(1).splitlines())
    return [name for line in lines for name in re.findall(r"'([^']+)'", line)]


def configure_arguments(pkgbuild: str) -> list[str]:
    """The arguments of the one `cmake --preset release` call, continuations joined."""
    joined = re.sub(r"\\\n\s*", " ", pkgbuild)
    calls = re.findall(r"^\s*cmake --preset release\b(.*)$", joined, re.MULTILINE)
    if len(calls) != 1:
        raise AssertionError(f"expected one configure call, found {len(calls)}")
    return calls[0].split()


def release_pkgbuild() -> str:
    with tempfile.TemporaryDirectory() as output:
        subprocess.run(
            ("bash", str(MAKE_RELEASE), "--version", "0.0.1", "--output", output),
            check=True,
            capture_output=True,
            text=True,
        )
        return (Path(output) / "PKGBUILD").read_text()


class EngineAndCapability(unittest.TestCase):
    def assert_engine_matches_option(self, pkgbuild: str) -> None:
        on_patched_engine = ENGINE in depends(pkgbuild)
        reports_extensions = FLAG in configure_arguments(pkgbuild)
        # Both directions fail a reader. Without the option the patched engine's
        # extensions are never offered; with it on the system engine they are
        # offered and hang.
        self.assertEqual(
            reports_extensions,
            on_patched_engine,
            f"depends on {ENGINE}: {on_patched_engine}, passes {FLAG}: {reports_extensions}",
        )

    def test_the_source_package(self) -> None:
        self.assert_engine_matches_option(SOURCE_PKGBUILD.read_text())

    def test_the_release_package(self) -> None:
        self.assert_engine_matches_option(release_pkgbuild())

    def test_the_option_is_one_cmake_declares(self) -> None:
        # A misspelt or renamed option is accepted by CMake with a warning
        # nobody reads, and the build goes on without it.
        self.assertRegex((ROOT / "CMakeLists.txt").read_text(), rf"(?m)^option\({OPTION}\s")


if __name__ == "__main__":
    unittest.main()
