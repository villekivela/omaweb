#!/usr/bin/env python3
"""Write a CycloneDX inventory of what goes into a distributed Omaweb build.

Everything here is already pinned somewhere in the repository: the Rust
dependency graph in `third_party/content-blocker/Cargo.lock`, the vendored web
assets in their `MANIFEST.json` files, and the approved engine in
`security/baseline.json`. This reads those rather than restating them, so an
inventory cannot drift from the thing it inventories.

What it deliberately leaves out is what the distribution supplies. An Arch
package depends on `qt6-webengine` rather than bundling it, so pacman already
knows which Qt is installed and shipping a second, possibly disagreeing answer
would be worse than shipping none. The approved engine baseline is recorded as
the build's own metadata instead, because that is Omaweb's claim about which
engine it is supported on.

Filter lists are left out for the same kind of reason: they are fetched on a
first run and are not in the artifact.

Usage:

    scripts/generate_sbom.py --output omaweb-sbom.json
    scripts/generate_sbom.py --output omaweb-sbom.json --version 0.1.2
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import subprocess
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
CONTENT_BLOCKER_MANIFEST = REPOSITORY_ROOT / "third_party/content-blocker/Cargo.toml"


def repository_version() -> str:
    """The nearest release tag, which is where CMake takes the version from."""
    try:
        described = subprocess.run(
            ["git", "describe", "--tags", "--dirty", "--match", "v[0-9]*"],
            cwd=REPOSITORY_ROOT,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "0.0.0"
    return described.removeprefix("v")


def rust_components() -> list[dict]:
    """Every crate the content blocker links, with the licence each declares.

    `cargo metadata` rather than the lockfile alone, because a lockfile records
    versions and checksums but never licences, and a licence is most of the
    point of an inventory.
    """
    try:
        metadata = json.loads(
            subprocess.run(
                [
                    "cargo",
                    "metadata",
                    "--format-version",
                    "1",
                    "--locked",
                    "--manifest-path",
                    str(CONTENT_BLOCKER_MANIFEST),
                ],
                capture_output=True,
                text=True,
                check=True,
            ).stdout
        )
    except FileNotFoundError:
        print("cargo is not installed, so the Rust graph cannot be read", file=sys.stderr)
        raise SystemExit(1)
    except subprocess.CalledProcessError as error:
        print(f"cargo metadata failed: {error.stderr}", file=sys.stderr)
        raise SystemExit(1)

    # The workspace member is Omaweb's own wrapper rather than a dependency of
    # it, so it is the subject of this inventory rather than an entry in it.
    workspace = set(metadata.get("workspace_members", []))
    components = []
    for package in sorted(metadata["packages"], key=lambda item: (item["name"], item["version"])):
        if package["id"] in workspace:
            continue
        component = {
            "type": "library",
            "name": package["name"],
            "version": package["version"],
            "purl": f"pkg:cargo/{package['name']}@{package['version']}",
            "scope": "required",
        }
        if package.get("license"):
            component["licenses"] = [{"expression": package["license"]}]
        elif package.get("license_file"):
            component["licenses"] = [{"license": {"name": f"see {package['license_file']}"}}]
        if package.get("repository"):
            component["externalReferences"] = [
                {"type": "vcs", "url": package["repository"]},
            ]
        components.append(component)
    return components


def vendored_component(manifest_path: Path, name: str, description: str) -> dict:
    """A directory copied verbatim from someone else's repository at one commit.

    The commit is the version. A vendored copy has no release of its own, and
    the ref is what a reader needs to fetch the corresponding source.
    """
    manifest = json.loads(manifest_path.read_text())
    repository = manifest["repository"]
    reference = manifest["ref"]
    component = {
        "type": "library",
        "name": name,
        "version": manifest.get("tag", reference[:12]),
        "description": description,
        "purl": f"pkg:github/{repository.removeprefix('https://github.com/')}@{reference}",
        "scope": "required",
        "licenses": [{"expression": manifest["license"]}],
        "externalReferences": [
            {"type": "vcs", "url": f"{repository}.git"},
            # Corresponding source: the exact tree the copy was taken from.
            {"type": "distribution", "url": f"{repository}/tree/{reference}"},
        ],
    }
    return component


def font_component() -> dict:
    """The icon font, which a release build compiles into the binary.

    Upstream publishes it from a branch rather than from releases, so it has no
    version to record and is identified by the hash of the file itself. Reading
    the file rather than a recorded hash means the inventory describes the copy
    that is actually being built.
    """
    font = REPOSITORY_ROOT / "assets/fonts/material-symbols-rounded.ttf"
    digest = hashlib.sha256(font.read_bytes()).hexdigest()
    return {
        "type": "file",
        "name": "material-symbols-rounded",
        "description": "Google Material Symbols Rounded, the interface's icon font",
        "scope": "required",
        "licenses": [{"expression": "Apache-2.0"}],
        "hashes": [{"alg": "SHA-256", "content": digest}],
        "externalReferences": [
            {"type": "vcs", "url": "https://github.com/google/material-design-icons.git"},
        ],
    }


def build(version: str) -> dict:
    baseline = json.loads((REPOSITORY_ROOT / "security/baseline.json").read_text())
    timestamp = datetime.datetime.now(datetime.UTC).replace(microsecond=0).isoformat()

    components = rust_components()
    components.append(
        vendored_component(
            REPOSITORY_ROOT / "third_party/ubo-scriptlets/MANIFEST.json",
            "ubo-scriptlets",
            "uBlock Origin scriptlet library and web-accessible resources",
        )
    )
    components.append(
        vendored_component(
            REPOSITORY_ROOT / "third_party/omarchy-shell/MANIFEST.json",
            "omarchy-shell",
            "Omarchy shell QML component kit",
        )
    )
    components.append(font_component())

    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.6",
        "version": 1,
        "metadata": {
            "timestamp": timestamp,
            "component": {
                "type": "application",
                "name": "omaweb",
                "version": version,
                "description": "A keyboard-driven web browser",
                "licenses": [{"expression": "MPL-2.0"}],
                "externalReferences": [
                    {"type": "vcs", "url": "https://github.com/villekivela/omaweb.git"},
                    {"type": "website", "url": "https://omaweb.app"},
                ],
            },
            # The engine is a dependency of the package rather than a part of
            # it, so it is stated as what this build is supported on rather than
            # listed as something shipped.
            "properties": [
                {"name": "omaweb:approvedQtWebEngine", "value": baseline["qtwebengine"]},
                {"name": "omaweb:approvedChromium", "value": baseline["chromium"]},
                {
                    "name": "omaweb:approvedChromiumSecurityPatch",
                    "value": baseline["chromiumSecurityPatch"],
                },
                {"name": "omaweb:engineSuppliedBy", "value": "distribution package"},
            ],
        },
        "components": components,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path, help="where to write the inventory")
    parser.add_argument("--version", help="the build's version, otherwise taken from the tag")
    arguments = parser.parse_args()

    document = build(arguments.version or repository_version())
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(document, indent=2) + "\n")
    print(f"{arguments.output}: {len(document['components'])} components")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
