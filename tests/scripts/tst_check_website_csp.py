#!/usr/bin/env python3
"""What the website's Content-Security-Policy check reports, on a site it is
pointed at.

The landing page builds part of itself in script, so markup the policy blocks
can arrive from a script as well as from the HTML. These run the check against
a throwaway site with the policy the real one sends.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "check_website_csp.py"

POLICY = {
    "headers": [
        {
            "source": "/(.*)",
            "headers": [{"key": "Content-Security-Policy", "value": "default-src 'self'"}],
        }
    ]
}


class CheckWebsiteCspTest(unittest.TestCase):
    def setUp(self) -> None:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.site = Path(directory.name)
        (self.site / "vercel.json").write_text(json.dumps(POLICY), encoding="utf-8")
        (self.site / "index.html").write_text(
            '<!doctype html><script type="module" src="page.js"></script>\n', encoding="utf-8"
        )

    def check(self, script: str) -> subprocess.CompletedProcess[str]:
        (self.site / "page.js").write_text(script, encoding="utf-8")
        return subprocess.run(
            (sys.executable, str(SCRIPT), "--website", str(self.site)),
            capture_output=True,
            text=True,
        )

    def test_a_script_that_only_reaches_this_origin_passes(self) -> None:
        result = self.check(
            'const list = document.querySelector(".signs");\n'
            'list.innerHTML = \'<li class="sign">Exit 1</li>\';\n'
            'fetch("/releases/");\n'
            'element.style.transform = "translateY(12px)";\n'
        )
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_markup_with_a_style_attribute_in_a_script_is_reported(self) -> None:
        result = self.check('list.innerHTML = \'<li style="color: red">Exit 1</li>\';\n')
        self.assertEqual(result.returncode, 1)
        self.assertIn("page.js:1: style attribute", result.stdout)

    def test_a_script_reaching_another_origin_is_reported(self) -> None:
        result = self.check(
            'import { road } from "https://cdn.example.com/road.js";\n'
            'fetch("https://api.example.com/stars");\n'
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("page.js:1: script loads https://cdn.example.com/road.js", result.stdout)
        self.assertIn("page.js:2: script loads https://api.example.com/stars", result.stdout)


if __name__ == "__main__":
    unittest.main()
