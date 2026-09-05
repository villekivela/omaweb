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

- EasyList — https://easylist.to/easylist/easylist.txt
- EasyPrivacy — https://easylist.to/easylist/easyprivacy.txt

Both are published by the EasyList authors under the GNU General Public License version 3 or
Creative Commons Attribution-ShareAlike 3.0 Unported, as stated at
https://easylist.to/pages/licence.html.

This file is populated from the exact dependency graph for each distributed build.

The release process must include, at minimum:

- Qt and QtWebEngine licenses, corresponding-source instructions, and replacement guidance
- Chromium component notices generated from the selected Qt SDK
- Ladybird and its dependency notices for the Ladybird variant
- `adblock-rust` and Rust dependency notices
- uBlock Origin's license and corresponding-source instructions for the vendored resource library
- Filter-list source, license, attribution, and update address

Do not publish a binary using this placeholder as its final notice inventory.
