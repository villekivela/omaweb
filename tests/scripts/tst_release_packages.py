#!/usr/bin/env python3
"""Which packages a release may put in the pacman repository, and in what order.

`scripts/release_packages.sh` is what the Release workflow and the engine workflow
that recovers a release both ask before they publish. A browser release carries the
browser and the client for each architecture, and the client goes first: each
package is served as soon as it is published, and a browser served before its client
is a dependency no reader's `pacman -Syu` can meet. The packages here are stand-ins
that carry only the `.PKGINFO` the script reads.
"""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "release_packages.sh"


def make_package(
    directory: Path, pkgname: str, arch: str, named_arch: str | None = None, version="0.11.0"
) -> str:
    """A package file holding a `.PKGINFO`, named as makepkg names one."""
    name = f"{pkgname}-{version}-1-{named_arch or arch}.pkg.tar.zst"
    staging = directory / f".{name}"
    staging.mkdir()
    (staging / ".PKGINFO").write_text(f"pkgname = {pkgname}\npkgver = {version}-1\narch = {arch}\n")
    subprocess.run(
        ("bsdtar", "-cf", str(directory / name), "-C", str(staging), ".PKGINFO"), check=True
    )
    return name


def publish_order(tag: str, packages: list[tuple]) -> tuple[int, list[str], str]:
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        for package in packages:
            make_package(directory, *package)
        done = subprocess.run(
            ("bash", str(SCRIPT), "--tag", tag, "--dir", str(directory)),
            capture_output=True,
            text=True,
        )
        names = [Path(line).name for line in done.stdout.splitlines()]
        return done.returncode, names, done.stderr


class BrowserRelease(unittest.TestCase):
    def test_publishes_the_client_before_the_browser(self) -> None:
        status, order, said = publish_order(
            "v0.11.0",
            [
                ("omaweb", "aarch64"),
                ("omaweb", "x86_64"),
                ("omaweb-cli", "aarch64"),
                ("omaweb-cli", "x86_64"),
            ],
        )
        self.assertEqual(status, 0, said)
        self.assertEqual(
            order,
            [
                "omaweb-cli-0.11.0-1-aarch64.pkg.tar.zst",
                "omaweb-0.11.0-1-aarch64.pkg.tar.zst",
                "omaweb-cli-0.11.0-1-x86_64.pkg.tar.zst",
                "omaweb-0.11.0-1-x86_64.pkg.tar.zst",
            ],
        )

    def test_refuses_a_browser_without_its_client(self) -> None:
        status, order, said = publish_order(
            "v0.11.0", [("omaweb", "x86_64"), ("omaweb", "aarch64"), ("omaweb-cli", "aarch64")]
        )
        self.assertNotEqual(status, 0)
        self.assertEqual(order, [])
        self.assertIn("omaweb-cli", said)
        self.assertIn("x86_64", said)

    def test_refuses_a_client_without_its_browser(self) -> None:
        status, order, said = publish_order("v0.11.0", [("omaweb-cli", "x86_64")])
        self.assertNotEqual(status, 0)
        self.assertEqual(order, [])

    def test_refuses_two_of_one_package(self) -> None:
        status, _, said = publish_order(
            "v0.11.0",
            [
                ("omaweb", "x86_64"),
                ("omaweb-cli", "x86_64"),
                ("omaweb-cli", "x86_64", None, "0.10.9"),
            ],
        )
        self.assertNotEqual(status, 0, said)

    def test_refuses_a_package_the_tag_does_not_name(self) -> None:
        status, order, said = publish_order(
            "v0.11.0",
            [("omaweb", "x86_64"), ("omaweb-cli", "x86_64"), ("omaweb-qtwebengine", "x86_64")],
        )
        self.assertNotEqual(status, 0)
        self.assertEqual(order, [])
        self.assertIn("omaweb-qtwebengine", said)

    def test_refuses_a_name_that_disagrees_with_the_build(self) -> None:
        status, order, said = publish_order(
            "v0.11.0", [("omaweb", "x86_64"), ("omaweb-cli", "aarch64", "x86_64")]
        )
        self.assertNotEqual(status, 0)
        self.assertEqual(order, [])
        self.assertIn("aarch64", said)


class EngineRelease(unittest.TestCase):
    def test_publishes_the_engine_alone(self) -> None:
        status, order, said = publish_order(
            "engine-6.11.2-2", [("omaweb-qtwebengine", "x86_64")]
        )
        self.assertEqual(status, 0, said)
        self.assertEqual(order, ["omaweb-qtwebengine-0.11.0-1-x86_64.pkg.tar.zst"])

    def test_refuses_the_browser_on_an_engine_tag(self) -> None:
        status, order, _ = publish_order(
            "engine-6.11.2-2", [("omaweb-qtwebengine", "x86_64"), ("omaweb", "x86_64")]
        )
        self.assertNotEqual(status, 0)
        self.assertEqual(order, [])


class Tags(unittest.TestCase):
    def test_refuses_a_tag_that_names_no_package(self) -> None:
        status, _, said = publish_order("nightly", [("omaweb", "x86_64")])
        self.assertNotEqual(status, 0)
        self.assertIn("nightly", said)

    def test_refuses_a_release_with_nothing_attached(self) -> None:
        status, _, said = publish_order("v0.11.0", [])
        self.assertNotEqual(status, 0)
        self.assertIn("no package", said)


if __name__ == "__main__":
    unittest.main()
