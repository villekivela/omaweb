#!/usr/bin/env python3
"""Whether a package needs a newer Qt than the one it was built against.

A program linked against Qt records the symbol version of every Qt symbol it
uses, `Qt_6.12` for one Qt 6.12 added, and the dynamic loader refuses to start
it on a Qt that does not define that version. v0.13.0 was built against Qt 6.12
and could not start on the Qt 6.11.2 Omarchy's stable mirror serves (#674). The
fixtures here are real programs linked against a stand-in Qt library whose
symbols carry Qt's version names, so what is checked is what the loader reads.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "check_qt_symbol_versions.sh"
COMPILER = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")


def stand_in_qt(directory: Path) -> Path:
    """A libQt6Stub.so.6 defining one symbol in each of Qt_6.11 and Qt_6.12."""
    source = directory / "stub.c"
    source.write_text(
        "int added_in_11(void) { return 11; }\n"
        "int added_in_12(void) { return 12; }\n"
    )
    script = directory / "stub.map"
    script.write_text(
        "Qt_6.11 { global: added_in_11; local: *; };\n"
        "Qt_6.12 { global: added_in_12; } Qt_6.11;\n"
    )
    library = directory / "libQt6Stub.so.6"
    subprocess.run(
        (COMPILER, "-shared", "-fPIC", "-o", str(library), str(source),
         f"-Wl,--version-script={script}", "-Wl,-soname,libQt6Stub.so.6"),
        check=True,
    )
    return library


def program(directory: Path, library: Path, name: str, calls: str) -> Path:
    """A program that calls `calls` from the stand-in Qt, so it needs that symbol's version."""
    source = directory / f"{name}.c"
    source.write_text(f"int {calls}(void);\nint main(void) {{ return {calls}(); }}\n")
    output = directory / name
    subprocess.run((COMPILER, "-o", str(output), str(source), str(library)), check=True)
    return output


def check(tree: Path, qt: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(("sh", str(SCRIPT), str(tree), qt), capture_output=True, text=True)


@unittest.skipUnless(COMPILER, "needs a C compiler to link the fixtures")
class CheckQtSymbolVersionsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.work = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.work)
        self.library = stand_in_qt(self.work)
        self.tree = self.work / "pkg"
        (self.tree / "usr" / "bin").mkdir(parents=True)

    def put(self, path: str, calls: str) -> None:
        built = program(self.work, self.library, Path(path).name, calls)
        destination = self.tree / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(built, destination)

    # v0.13.0's browser, needing Qt_6.12, on the mirror's Qt 6.11.2.
    def test_a_newer_qt_than_the_build_fails(self) -> None:
        self.put("usr/bin/omaweb-browser", "added_in_12")
        result = check(self.tree, "6.11.2")
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("usr/bin/omaweb-browser", result.stderr)
        self.assertIn("Qt_6.12", result.stderr)

    def test_the_qt_it_was_built_against_passes(self) -> None:
        self.put("usr/bin/omaweb-browser", "added_in_11")
        result = check(self.tree, "6.11.2")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_an_older_qt_than_the_build_passes(self) -> None:
        self.put("usr/bin/omaweb-browser", "added_in_12")
        result = check(self.tree, "6.12.0")
        self.assertEqual(result.returncode, 0, result.stderr)

    # A shared library is checked as well as a program, wherever it is: the
    # sync plugin and its library needed Qt_6.12 in v0.13.0 too.
    def test_every_binary_in_the_tree_is_read(self) -> None:
        self.put("usr/bin/omaweb-browser", "added_in_11")
        self.put("usr/lib/omaweb/modules/libomaweb-sync.so", "added_in_12")
        result = check(self.tree, "6.11.2")
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("libomaweb-sync.so", result.stderr)
        self.assertNotIn("omaweb-browser", result.stderr)

    # A Qt library in the tree defines Qt_6.12 for others to need, and needs
    # nothing of the kind itself.
    def test_a_version_a_library_defines_is_not_a_need(self) -> None:
        self.put("usr/bin/omaweb-browser", "added_in_11")
        shutil.copy(self.library, self.tree / "usr" / "bin" / self.library.name)
        result = check(self.tree, "6.11.2")
        self.assertEqual(result.returncode, 0, result.stderr)

    # A version the comparison cannot read would compare as nothing newer.
    def test_a_qt_version_that_is_not_one_is_refused(self) -> None:
        self.put("usr/bin/omaweb-browser", "added_in_12")
        result = check(self.tree, "")
        self.assertEqual(result.returncode, 2, result.stdout)

    # A file name with a space in it is one file, not two.
    def test_a_name_with_a_space_is_read(self) -> None:
        self.put("usr/lib/omaweb/a module/libomaweb-sync.so", "added_in_12")
        result = check(self.tree, "6.11.2")
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("a module/libomaweb-sync.so needs Qt_6.12", result.stderr)

    # A tree with no binaries in it is the wrong tree, an unpacked path that
    # moved, and passing it would pass every release after.
    def test_a_tree_without_binaries_fails(self) -> None:
        (self.tree / "usr" / "bin" / "README").write_text("not a program\n")
        result = check(self.tree, "6.11.2")
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("no binaries", result.stderr)


if __name__ == "__main__":
    unittest.main()
