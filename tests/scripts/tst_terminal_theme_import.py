#!/usr/bin/env python3
"""The syntax half of the terminal importer, which is the half with a decision in it.

Which ANSI slot each construct is read from is a claim about what an editor
does with the same colours, and it is made twice — here, and in
`integrations/omarchy/omaweb.json.tpl` for a desktop that renders the palette
itself. These tests pin the claim so the two cannot drift apart unnoticed.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import import_terminal_theme as importer  # noqa: E402

# A terminal carrying Tokyo Night, which is the theme the mapping was read off:
# an editor rendering it draws a tag's name in the brighter magenta beside the
# keyword's deeper one, an attribute's name in the brighter cyan beside the
# type's, a number orange, and a plain identifier in body text.
TOKYO_NIGHT = {
    "background": "#1a1b26",
    "foreground": "#c0caf5",
    "palette": {
        0: "#15161e", 1: "#f7768e", 2: "#9ece6a", 3: "#e0af68",
        4: "#7aa2f7", 5: "#bb9af7", 6: "#7dcfff", 7: "#a9b1d6",
        8: "#414868", 9: "#ff7a93", 10: "#b9f27c", 11: "#ff9e64",
        12: "#7da6ff", 13: "#c6a0f6", 14: "#0db9d7", 15: "#c0caf5",
    },
}


def derive(**overrides):
    source = dict(TOKYO_NIGHT)
    source.update(overrides)
    return importer.derive(source)


class SyntaxDerivation(unittest.TestCase):
    def test_reads_each_construct_from_the_slot_an_editor_draws_it_in(self):
        syntax = derive()["syntax"]
        palette = TOKYO_NIGHT["palette"]
        self.assertEqual(syntax["string"], palette[2])
        self.assertEqual(syntax["number"], palette[11])
        self.assertEqual(syntax["function"], palette[4])
        self.assertEqual(syntax["keyword"], palette[5])
        self.assertEqual(syntax["tag"], palette[13])
        self.assertEqual(syntax["attribute"], palette[14])
        self.assertEqual(syntax["type"], palette[6])

    def test_draws_a_plain_identifier_in_body_text(self):
        # Spending a slot on it would cost the palette a colour and tell the
        # reader nothing: every name a document invented would then share one
        # with whichever construct that slot already carried.
        syntax = derive()["syntax"]
        self.assertEqual(syntax["variable"], TOKYO_NIGHT["foreground"])
        self.assertNotEqual(syntax["variable"], syntax["tag"])
        # And red stays out of code entirely: it carries "this failed"
        # everywhere else in the interface.
        self.assertNotIn(TOKYO_NIGHT["palette"][1], syntax.values())
        self.assertNotIn(TOKYO_NIGHT["palette"][9], syntax.values())

    def test_keeps_the_two_constructs_sharing_a_family_apart(self):
        # Each pair takes the two strengths of its family rather than the one
        # colour, so a tag still reads as a tag beside a keyword.
        syntax = derive()["syntax"]
        self.assertNotEqual(syntax["keyword"], syntax["tag"])
        self.assertNotEqual(syntax["attribute"], syntax["type"])

    def test_names_no_comment_or_punctuation(self):
        # Both are the theme's own text turned down, and ThemeController
        # derives them that way for every theme it loads.
        syntax = derive()["syntax"]
        self.assertNotIn("comment", syntax)
        self.assertNotIn("punctuation", syntax)

    def test_swaps_a_slot_too_dark_to_read_for_its_twin(self):
        palette = dict(TOKYO_NIGHT["palette"])
        palette[2] = "#0a1a0a"
        syntax = derive(palette=palette)["syntax"]
        self.assertEqual(syntax["string"], palette[10])

    def test_swaps_an_unreadable_bright_slot_for_its_ordinary_twin(self):
        # A construct read from the bright half has its twin behind it rather
        # than ahead of it, and the pair is the same hue either way.
        palette = dict(TOKYO_NIGHT["palette"])
        palette[11] = "#141005"
        syntax = derive(palette=palette)["syntax"]
        self.assertEqual(syntax["number"], palette[3])

    def test_keeps_the_more_legible_twin_when_both_fail(self):
        palette = dict(TOKYO_NIGHT["palette"])
        palette[5] = "#241d22"
        palette[13] = "#3a2f36"
        syntax = derive(palette=palette)["syntax"]
        self.assertEqual(syntax["tag"], palette[13])

    def test_every_construct_reads_against_the_window(self):
        window = importer.parse_hex(TOKYO_NIGHT["background"])
        for token, colour in derive()["syntax"].items():
            with self.subTest(token=token):
                self.assertGreaterEqual(
                    importer.contrast(importer.parse_hex(colour), window),
                    importer.SYNTAX_MINIMUM_CONTRAST)


class BuiltTheme(unittest.TestCase):
    def test_replaces_the_shipped_syntax_block_wholesale(self):
        theme = importer.build_theme(TOKYO_NIGHT)
        self.assertNotIn("comment", theme["syntax"])
        self.assertEqual(theme["syntax"]["tag"], TOKYO_NIGHT["palette"][13])

    def test_leaves_omawebs_own_layout_decisions_alone(self):
        shipped = importer.json.loads(importer.TEMPLATE.read_text())
        theme = importer.build_theme(TOKYO_NIGHT)
        self.assertEqual(theme["opacity"], shipped["opacity"])
        self.assertEqual(theme["tint"], shipped["tint"])


if __name__ == "__main__":
    unittest.main()
