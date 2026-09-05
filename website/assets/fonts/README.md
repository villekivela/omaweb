# Fonts

## JetBrains Mono

`JetBrainsMono.woff2` is a web-optimized copy of the JetBrains Mono variable font. The source is
[JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) version 2.304.

JetBrains Mono is licensed under the SIL Open Font License 1.1. See `OFL.txt`.

## Space Grotesk

`SpaceGrotesk.woff2` is the Space Grotesk variable font from the
[Space Grotesk project](https://github.com/floriankarsten/space-grotesk).

Space Grotesk is licensed under the SIL Open Font License 1.1. See `OFL.txt`.

## Material Symbols Rounded

`material-symbols-subset.woff2` holds only the icons the product UI on the page draws, subset from
the full variable font this repository vendors in `assets/fonts/`. Rebuild it after changing which
icons the page uses:

    scripts/build_website_icon_font.py

The icon names in that script match the ones the application asks for in `src/ui/*.qml`, so the site
and the browser cannot drift apart. The script resolves each codepoint out of the font's own
ligature table rather than hard-coding it.

Material Symbols are licensed under Apache License 2.0. See `MATERIAL_SYMBOLS_LICENSE.txt`.
