#!/usr/bin/env python3
"""What the two PKGBUILDs say about the packages they build.

The engine a package depends on and the capability its build reports, kept together.

Whether Omaweb offers a Known extension is decided when it is compiled, by
`OMAWEB_KNOWN_EXTENSIONS`, and whether it can host one is decided by the engine
the package depends on. The two live in different parts of the PKGBUILD, and
v0.7.1 shipped with the dependency and without the option, so Settings told a
reader running the patched engine that it ran the system's (#365). A build of
the package takes too long to be where this is caught, so the lines that decide
it are read instead, from both the source package and the one a release
derives from it.

The small client is a package of its own, which the browser's depends on, so a
sandbox installs it alone and an upgrade from a release that had one package
brings both.
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


def package_function(pkgbuild: str, name: str) -> str:
    pattern = rf"^package_{re.escape(name)}\(\) {{\n(.*?)^}}"
    body = re.search(pattern, pkgbuild, re.MULTILINE | re.DOTALL)
    if not body:
        raise AssertionError(f"the PKGBUILD has no package_{name}()")
    return body.group(1)


def assignment(body: str, name: str) -> str:
    found = re.findall(rf"^\s*{name}\+?=\((.*)\)$", body, re.MULTILINE)
    if len(found) != 1:
        raise AssertionError(f"expected one {name} in the function, found {found}")
    return found[0]


class TwoPackages(unittest.TestCase):
    def assert_splits(self, pkgbuild: str, browser: str, client: str) -> None:
        self.assertRegex(pkgbuild, rf"(?m)^pkgname=\('{browser}' '{client}'\)$")
        browser_body = package_function(pkgbuild, browser)
        client_body = package_function(pkgbuild, client)
        # Each installs its own half, and the browser cannot be had without
        # the command that starts it.
        self.assertIn("--component browser", browser_body)
        self.assertIn("--component cli", client_body)
        self.assertIn('"omaweb-cli=$pkgver"', assignment(browser_body, "depends"))
        # A sandbox has Qt's base and not the browser's dependencies.
        self.assertEqual(assignment(client_body, "depends"), "'qt6-base'")

    def test_the_source_package(self) -> None:
        pkgbuild = SOURCE_PKGBUILD.read_text()
        self.assert_splits(pkgbuild, "omaweb-git", "omaweb-cli-git")
        self.assertRegex(pkgbuild, r"(?m)^pkgbase=omaweb-git$")
        client = package_function(pkgbuild, "omaweb-cli-git")
        self.assertEqual(assignment(client, "provides"), '"omaweb-cli=$pkgver"')
        self.assertEqual(assignment(client, "conflicts"), "'omaweb-cli'")

    def test_the_release_package(self) -> None:
        pkgbuild = release_pkgbuild()
        self.assert_splits(pkgbuild, "omaweb", "omaweb-cli")
        self.assertRegex(pkgbuild, r"(?m)^pkgbase=omaweb$")
        self.assertEqual(
            assignment(package_function(pkgbuild, "omaweb"), "conflicts"), "'omaweb-git'"
        )
        self.assertEqual(
            assignment(package_function(pkgbuild, "omaweb-cli"), "conflicts"), "'omaweb-cli-git'"
        )


class EngineAndCapability(unittest.TestCase):
    def assert_engine_matches_option(self, pkgbuild: str) -> None:
        engine = re.compile(rf"{ENGINE}([<>=].*)?")
        on_patched_engine = any(engine.fullmatch(name) for name in depends(pkgbuild))
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

    def test_the_engine_has_what_omaweb_needs(self) -> None:
        # Engine 6.11.2-5 crashed the moment a Known extension made an offscreen
        # document (#646), before 6.11.2-12 an extension lost the scripts it
        # registered at every restart (#686), before 6.11.2-14 a tab kept the
        # favicon drawn under the colour scheme it loaded under (#700), before
        # 6.11.2-15 a raster icon beside an SVG one was shown instead of the
        # SVG, so the icon could not follow the scheme (#700), a page that
        # asked what its passkeys can reach never heard back (#711), and a
        # Private window asked the reader before it refused a push subscription
        # (#421), before 6.11.2-16 a copy button asked the reader every time,
        # an extension that declares clipboardWrite could not copy from its
        # offscreen document (#745) and the engine gave a page no chrome.app,
        # and Omaweb defines none of its own (#766), and an Omaweb built against
        # 6.11.2-12 calls the window geometry API that 6.11.2-7 does not have,
        # so it would not start on it (#684). Omaweb runs on 6.11.2-16, but
        # that engine carries Chromium's security fixes only up to
        # 151.0.7922.71, and 6.11.3-1 carries them up to 155.0.8059.85 (#802).
        # pacman upgrades Omaweb without the engine unless the dependency asks
        # for a newer one.
        for pkgbuild in (SOURCE_PKGBUILD.read_text(), release_pkgbuild()):
            self.assertIn(f"{ENGINE}>=6.11.3-1", depends(pkgbuild))

    def test_the_option_is_one_cmake_declares(self) -> None:
        # A misspelt or renamed option is accepted by CMake with a warning
        # nobody reads, and the build goes on without it.
        self.assertRegex((ROOT / "CMakeLists.txt").read_text(), rf"(?m)^option\({OPTION}\s")


class GitBuildIsNamedDev(unittest.TestCase):
    """The git package says on its launcher and in About that it is not a release (#772)."""

    GIT_VERSION = "0.16.1.r14.gc8c9084"

    def test_the_source_package_names_its_launcher_dev(self) -> None:
        body = package_function(SOURCE_PKGBUILD.read_text(), "omaweb-git")
        self.assertRegex(
            body,
            r'sed -i .*s/\^Name=Omaweb\$/Name=Omaweb Dev/.* '
            r'"\$pkgdir/usr/share/applications/omaweb\.desktop"',
        )

    def test_the_release_package_names_its_launcher_omaweb(self) -> None:
        pkgbuild = release_pkgbuild()
        self.assertNotIn("Omaweb Dev", pkgbuild)
        self.assertNotIn("share/applications", pkgbuild)

    def test_the_source_launcher_file_keeps_its_name_and_class(self) -> None:
        entry = (ROOT / "packaging" / "omaweb.desktop").read_text()
        self.assertIn("\nName=Omaweb\n", entry)
        self.assertIn("\nStartupWMClass=omaweb\n", entry)

    def test_the_source_package_builds_with_its_own_version(self) -> None:
        self.assertIn('-DOMAWEB_VERSION_OVERRIDE="$pkgver"', configure_arguments(SOURCE_PKGBUILD.read_text()))

    def test_the_release_package_builds_with_the_version_its_tag_gives(self) -> None:
        self.assertFalse(
            [a for a in configure_arguments(release_pkgbuild()) if "VERSION_OVERRIDE" in a]
        )

    def resolved(self, *arguments: str) -> dict[str, str]:
        with tempfile.TemporaryDirectory() as scratch:
            script = Path(scratch) / "resolve.cmake"
            script.write_text(
                f'set(CMAKE_CURRENT_SOURCE_DIR "{ROOT}")\n'
                f'include("{ROOT}/cmake/OmawebVersion.cmake")\n'
                "omaweb_resolve_version()\n"
                'message(STATUS "version=${OMAWEB_VERSION}")\n'
                'message(STATUS "string=${OMAWEB_VERSION_STRING}")\n'
            )
            done = subprocess.run(
                ("cmake", *arguments, "-P", str(script)),
                check=True,
                capture_output=True,
                text=True,
            )
        output = done.stdout + done.stderr
        return dict(re.findall(r"-- (version|string)=(\S*)", output))

    def test_about_shows_the_override_whole(self) -> None:
        found = self.resolved(f"-DOMAWEB_VERSION_OVERRIDE={self.GIT_VERSION}")
        self.assertEqual(found["string"], self.GIT_VERSION)
        # Bundle versions and the project version stay a plain x.y.z.
        self.assertRegex(found["version"], r"^\d+\.\d+\.\d+$")

    def test_without_an_override_the_version_comes_from_the_tag(self) -> None:
        found = self.resolved()
        self.assertNotIn(".r", found["string"])
        self.assertTrue(found["string"])


if __name__ == "__main__":
    unittest.main()
