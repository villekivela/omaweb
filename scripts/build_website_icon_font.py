#!/usr/bin/env python3
"""Subset Material Symbols Rounded for the website's product UI.

The vendored font in `assets/fonts/material-symbols-rounded.ttf` is Google's
full variable font at ~15 MB, which is fine for the application and far too
heavy for a web page. This builds `website/assets/fonts/material-symbols-
subset.woff2`, holding only the glyphs the site draws.

Icon codepoints are read out of the font's own ligature table rather than
hard-coded, because the PUA assignments are not guessable: `lock` is U+E88D and
`shield` is U+E75B, neither of which follows from the name. Add a name to
ICONS and re-run; the script prints the codepoints to reference from CSS.

Needs fontTools and brotli:

    python3 -m venv .venv && .venv/bin/pip install fonttools brotli
    .venv/bin/python scripts/build_website_icon_font.py
"""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "assets" / "fonts" / "material-symbols-rounded.ttf"
TARGET = ROOT / "website" / "assets" / "fonts" / "material-symbols-subset.woff2"

# The icon names the website's product UI draws. These match the names the
# application asks for in src/ui/*.qml, so the site and the browser cannot
# drift apart.
ICONS = [
    "arrow_back",
    "arrow_forward",
    "left_panel_close",
    "left_panel_open",
    "lock",
    "more_horiz",
    "refresh",
    "search",
    "settings",
    "shield",
]

# The application draws icons at one weight and no fill, so the website pins
# the same instance and drops the variation tables entirely.
INSTANCE = {"wght": 400, "FILL": 0, "GRAD": 0, "opsz": 24}


def ligature_map(font) -> dict[tuple[str, ...], str]:
    """Every ligature in the font, keyed by its component glyph sequence."""
    table = font["GSUB"].table
    found: dict[tuple[str, ...], str] = {}
    for lookup in table.LookupList.Lookup:
        for subtable in lookup.SubTable:
            # Material Symbols wraps its ligatures in extension lookups.
            if lookup.LookupType == 7:
                kind, subtable = subtable.ExtensionLookupType, subtable.ExtSubTable
            else:
                kind = lookup.LookupType
            if kind != 4:
                continue
            for first, ligatures in subtable.ligatures.items():
                for ligature in ligatures:
                    key = (first,) + tuple(ligature.Component)
                    found[key] = ligature.LigGlyph
    return found


def resolve(font) -> dict[str, int]:
    """Map each icon name to the codepoint of the glyph its ligature yields."""
    cmap = font.getBestCmap()
    first_codepoint: dict[str, int] = {}
    for codepoint, glyph in cmap.items():
        first_codepoint.setdefault(glyph, codepoint)

    ligatures = ligature_map(font)
    resolved: dict[str, int] = {}
    missing: list[str] = []
    for name in ICONS:
        try:
            sequence = tuple(cmap[ord(char)] for char in name)
        except KeyError:
            missing.append(name)
            continue
        glyph = ligatures.get(sequence)
        codepoint = first_codepoint.get(glyph) if glyph else None
        if codepoint is None:
            missing.append(name)
        else:
            resolved[name] = codepoint
    if missing:
        raise SystemExit(f"no glyph for: {', '.join(missing)}")
    return resolved


def main() -> int:
    try:
        from fontTools import subset
        from fontTools.ttLib import TTFont
        from fontTools.varLib import instancer
    except ImportError:
        raise SystemExit("needs fontTools and brotli; see the module docstring")

    if not SOURCE.exists():
        raise SystemExit(f"missing {SOURCE.relative_to(ROOT)}")

    codepoints = resolve(TTFont(SOURCE, lazy=True))

    font = instancer.instantiateVariableFont(TTFont(SOURCE), INSTANCE, inplace=True)
    options = subset.Options()
    options.flavor = "woff2"
    options.desubroutinize = True
    options.drop_tables += ["GSUB", "GPOS", "DSIG"]
    options.name_IDs = []
    options.layout_features = []
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=codepoints.values())
    subsetter.subset(font)

    TARGET.parent.mkdir(parents=True, exist_ok=True)
    font.flavor = "woff2"
    font.save(TARGET)

    size = TARGET.stat().st_size
    print(f"{TARGET.relative_to(ROOT)}  {size:,} bytes  ({len(codepoints)} icons)")
    print()
    print("Reference these from CSS as content: \"\\eXXX\":")
    for name, codepoint in sorted(codepoints.items()):
        print(f"  {name:20} \\{codepoint:x}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
