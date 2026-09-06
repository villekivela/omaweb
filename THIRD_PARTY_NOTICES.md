# Third-party notices

## Material Symbols

Omaweb bundles a small subset of Material Symbols Rounded by Google. Material Symbols are licensed
under Apache License 2.0. The license text and subset provenance live in `assets/fonts/`. The
website ships its own smaller subset, with the same license text, in `website/assets/fonts/`.

## JetBrains Mono

The website ships a web copy of the JetBrains Mono variable font in `website/assets/fonts/`.
JetBrains Mono is licensed under the SIL Open Font License 1.1; the license text lives beside it as
`OFL.txt`.

## Omarchy shell component kit

Omaweb vendors the QML component kit from the Omarchy shell (`shell/Ui` and `shell/Commons` of
https://github.com/omacom/omarchy) under `third_party/omarchy-shell`, copied verbatim and pinned by
`MANIFEST.json`. Omarchy is licensed under the MIT License, Copyright (c) David Heinemeier Hansson.
The upstream license text ships with the pinned revision.

## uBlock Origin resource library

Omaweb vendors uBlock Origin's scriptlet library (`src/js/resources` and the three modules it
imports from beside it) and its web-accessible resources (`src/web_accessible_resources` and the
`src/js/redirect-resources.js` map naming them), from https://github.com/gorhill/uBlock, under
`third_party/ubo-scriptlets`, copied verbatim and pinned by `MANIFEST.json`. Both sets are built
into the content blocker as `scriptlets.json` and `redirects.json`, generated from the copies and
pinned by the same manifest. uBlock Origin is licensed under the GNU General Public License version
3 or later, Copyright (C) Raymond Hill. The upstream license text ships with the pinned revision as
`LICENSE.txt`.

A `##+js(...)` filter rule names a function from this library and a `$redirect=` rule names a body
from it; no filter list ever supplies either. See `docs/adr/0025-run-only-vendored-scriptlets.md`
and `docs/adr/0026-serve-substitutes-under-an-omaweb-scheme.md`.

## Default filter lists

Omaweb subscribes to two filter lists on a first run and fetches them from https://easylist.to/.
Neither list ships in the repository or in a build.

- EasyList: https://easylist.to/easylist/easylist.txt
- EasyPrivacy: https://easylist.to/easylist/easyprivacy.txt

Both are published by the EasyList authors under the GNU General Public License version 3 or
Creative Commons Attribution-ShareAlike 3.0 Unported, as stated at
https://easylist.to/pages/licence.html.

## Corresponding source

Omaweb is under the Mozilla Public License 2.0, which is a per-file copyleft: the source for every
file in this repository is this repository, at the commit a build names. `omaweb --version` and
Settings both report that commit, because CMake takes the version from the nearest release tag and a
build past a tag carries the description that names it.

The vendored copies above are the other half. Each is pinned to one upstream commit in its
`MANIFEST.json`, and the inventory below names that commit and the tree it came from, which is what
a reader needs to fetch the corresponding source for a GPL-licensed copy.

## The inventory

`scripts/generate_sbom.py` writes a CycloneDX inventory of what a distributed build contains, read
from the places those things are already pinned rather than restated by hand. Every release carries
one, and the Arch package installs it as `share/omaweb/sbom.json`.

It covers what Omaweb ships: the Rust dependency graph the content blocker links, the two vendored
web-asset directories, and the icon font. It does not cover the web engine, and that is deliberate.
The Arch package depends on `qt6-webengine` rather than bundling it, so the distribution's own
package carries Qt's and Chromium's notices, and its package manager already knows which version is
installed. A second answer from Omaweb could only disagree with that one. What the inventory records
instead is the approved engine baseline, which is Omaweb's own claim about the engine it is
supported on.

Filter lists are not in it either. They are fetched on a first run rather than shipped, and the
addresses and licences above are what applies to them.

## A build that bundles its engine

A bundled build, an AppImage or a Flatpak, would carry Qt and Chromium itself, and none of the
paragraph above would hold: it would need Qt and QtWebEngine licences, corresponding-source
instructions and replacement guidance, plus Chromium component notices generated from the selected
Qt SDK, and the same for Ladybird and its dependencies in the Ladybird variant. ADR 0013 defers both
formats until Omaweb can maintain bundled engine security updates, and this inventory is not
sufficient for one until it does.
