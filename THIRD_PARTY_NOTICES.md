# Third-party notices

## Material Symbols

Tanto bundles a small subset of Material Symbols Rounded by Google. Material Symbols are licensed under Apache License 2.0. The license text and subset provenance live in `assets/fonts/`.

## Omarchy shell component kit

Tanto vendors the QML component kit from the Omarchy shell (`shell/Ui` and
`shell/Commons` of https://github.com/omacom/omarchy) under
`third_party/omarchy-shell`, copied verbatim and pinned by `MANIFEST.json`.
Omarchy is licensed under the MIT License, Copyright (c) David Heinemeier Hansson.
The upstream license text ships with the pinned revision.

This file is populated from the exact dependency graph for each distributed build.

The release process must include, at minimum:

- Qt and QtWebEngine licenses, corresponding-source instructions, and replacement guidance
- Chromium component notices generated from the selected Qt SDK
- Ladybird and its dependency notices for the Ladybird variant
- `adblock-rust` and Rust dependency notices
- Filter-list source, license, attribution, and update address

Do not publish a binary using this placeholder as its final notice inventory.
