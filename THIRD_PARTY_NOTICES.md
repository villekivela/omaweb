# Third-party notices

## Material Symbols

Tanto bundles a small subset of Material Symbols Rounded by Google. Material Symbols are licensed under Apache License 2.0. The license text and subset provenance live in `assets/fonts/`.

## Omarchy shell component kit

Tanto vendors the QML component kit from the Omarchy shell (`shell/Ui` and
`shell/Commons` of https://github.com/omacom/omarchy) under
`third_party/omarchy-shell`, copied verbatim and pinned by `MANIFEST.json`.
Omarchy is licensed under the MIT License, Copyright (c) David Heinemeier Hansson.
The upstream license text ships with the pinned revision.

## Default filter lists

Tanto subscribes to two filter lists on a first run and fetches them from
https://easylist.to/. Neither list ships in the repository or in a build.

- EasyList — https://easylist.to/easylist/easylist.txt
- EasyPrivacy — https://easylist.to/easylist/easyprivacy.txt

Both are published by the EasyList authors under the GNU General Public License
version 3 or Creative Commons Attribution-ShareAlike 3.0 Unported, as stated at
https://easylist.to/pages/licence.html.

This file is populated from the exact dependency graph for each distributed build.

The release process must include, at minimum:

- Qt and QtWebEngine licenses, corresponding-source instructions, and replacement guidance
- Chromium component notices generated from the selected Qt SDK
- Ladybird and its dependency notices for the Ladybird variant
- `adblock-rust` and Rust dependency notices
- Filter-list source, license, attribution, and update address

Do not publish a binary using this placeholder as its final notice inventory.
