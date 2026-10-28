#!/usr/bin/env python3
"""What `scripts/build_website_themes.py` writes into the website's `themes.css`, without a lab.

The captures need the lab and a compositor, and are not gated. The stylesheet is: the page wears a
theme only through its rule there, so a run for one theme that dropped the others' rules, or a theme
the page wears without offering that took captures it never shows, would be wrong in a way nothing
downstream catches.
"""

import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import build_website_themes as themes  # noqa: E402

PALETTE = {
    "windowOpaque": "#16242d",
    "sidebarOpaque": "#101b21",
    "text": "#d6e2ee",
    "accent": "#8bc9eb",
    "urgent": "#4d86b0",
    "syntax": {"keyword": "#b4e4f6"},
}


class Stylesheet(unittest.TestCase):
    def test_a_run_for_one_theme_keeps_the_rules_of_the_others(self):
        earlier = themes.stylesheet(
            {"nord": (PALETTE, {"mutedText": "#667080"})}, {"nord": "#81a1c1"}
        )
        kept = themes.kept_rules(earlier)
        self.assertEqual(set(kept), {"nord"})
        later = themes.stylesheet(
            {"lumon": (PALETTE, {"mutedText": "#4d86b0"})}, {"lumon": "#f2fcff"}, kept
        )
        self.assertIn(kept["nord"], later)
        self.assertIn('[data-theme="lumon"] {', later)
        # In the page's order, whichever run wrote each.
        self.assertLess(later.index('"nord"'), later.index('"lumon"'))

    def test_a_rule_built_again_replaces_the_one_kept(self):
        kept = themes.kept_rules(
            themes.stylesheet({"nord": (PALETTE, {"mutedText": "#667080"})}, {"nord": "#81a1c1"})
        )
        again = themes.stylesheet(
            {"nord": (PALETTE, {"mutedText": "#123456"})}, {"nord": "#81a1c1"}, kept
        )
        self.assertEqual(again.count('[data-theme="nord"]'), 1)
        self.assertIn("#123456", again)

    def test_lumon_is_worn_by_the_page_and_never_captured(self):
        # The desktop film's Theme beat changes to Lumon, and the page puts it on with that card's
        # clip; no swatch offers it, so no capture would ever be shown.
        self.assertIn("lumon", [name for name, _ in themes.THEMES])
        self.assertIn("lumon", themes.WORN_ONLY)
        self.assertNotIn("ethereal-nebula-silver", themes.WORN_ONLY)


if __name__ == "__main__":
    unittest.main()
