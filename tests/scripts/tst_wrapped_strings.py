#!/usr/bin/env python3
"""No reader-facing string literal is left unwrapped in the chrome's QML.

ADR 0056 has every string a reader sees go through `qsTr`. This reads the
property assignments that carry reader-visible text in `src/ui`, `src/ui-lab`
and the engine view, and fails on a literal that is not inside a translation
call. It is a heuristic over source text, not a QML parser: an identifier
(`"smart_toy"`, a lowercase word with no space) is taken for an icon or a
token, a date format and the product name are skipped, and a token passed to
`.arg()` is a value rather than prose.
"""

from __future__ import annotations

import re
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCANNED = ("src/ui", "src/ui-lab", "src/engine/qt")

# Files a localization ticket has not wrapped yet. Empty now that every chrome file is.
OWNED_BY_OTHER_TICKETS: set[str] = set()

# Content the lab's stand-in engine draws as the web page, not as the chrome.
NOT_CHROME = {"MockEngineView.qml": ("samplePage", "documentReview")}

PROPERTY = re.compile(
    r"(?:^|\{)\s*(?P<name>label|text|title|placeholder|placeholderText|accessibleName|"
    r"Accessible\.name|Accessible\.description|toolTip|message|detail|note|"
    r"cancelHint|confirmHint|headingText|panelName|\"label\"|\"note\"|\"message\"|"
    r"\"detail\"|\"title\"):\s*(?P<rest>.*)$"
)
NEW_STATEMENT = re.compile(r"^\s*(?:[\w.]+|\"[\w ]+\"):|^\s*[{}\]]|^\s*(?:function|property|signal)\b")
TRANSLATED = re.compile(r"qsTr(?:anslate)?\(\s*(?:\"(?:[^\"\\]|\\.)*\"\s*[,+]?\s*)+", re.S)
LITERAL = re.compile(r"\"((?:[^\"\\]|\\.)*)\"")
DATE_FORMAT = re.compile(r"^[yMdhmsz\-: .]+$")
PRODUCT_NAMES = {"Omaweb"}
IDENTIFIER = re.compile(r"^[a-z][a-z0-9_.\-]*$")


def violations(path: Path, source: str) -> list[tuple[int, str]]:
    found = []
    lines = source.splitlines()
    index = 0
    while index < len(lines):
        match = PROPERTY.search(lines[index])
        if not match or "property " in lines[index]:
            index += 1
            continue
        start = index
        statement = lines[index]
        index += 1
        while index < len(lines) and not NEW_STATEMENT.match(lines[index]):
            statement += "\n" + lines[index]
            index += 1
        statement = re.sub(r"(?<!:)//.*", "", statement)
        statement = re.sub(r"'[^'\n]*'", "''", statement)
        statement = TRANSLATED.sub("", statement)
        statement = re.sub(r"\.arg\(\s*\"[^\"]*\"\s*\)", "", statement)
        for literal in LITERAL.findall(statement.split(":", 1)[1]):
            if not re.search(r"[A-Za-z]{2}", literal) or IDENTIFIER.match(literal):
                continue
            if literal in PRODUCT_NAMES or DATE_FORMAT.match(literal):
                continue
            found.append((start + 1, literal))
    return found


def scan(root: Path) -> list[str]:
    report = []
    for directory in SCANNED:
        for path in sorted((root / directory).rglob("*.qml")):
            if path.name in OWNED_BY_OTHER_TICKETS or path.name in NOT_CHROME:
                continue
            for line, literal in violations(path, path.read_text(encoding="utf-8")):
                report.append(f"{path.relative_to(root)}:{line}: {literal!r}")
    return report


class WrappedStrings(unittest.TestCase):
    def test_the_chrome_has_no_unwrapped_literal(self):
        report = scan(ROOT)
        self.assertEqual([], report, "wrap each in qsTr (docs/agents/code-style.md)")

    def test_an_unwrapped_literal_is_reported_and_a_wrapped_one_is_not(self):
        with tempfile.TemporaryDirectory() as directory:
            ui = Path(directory) / "src" / "ui"
            ui.mkdir(parents=True)
            (ui / "Probe.qml").write_text(
                'Item {\n'
                '    Text { text: "Shown to the reader" }\n'
                '    ActionButton {\n'
                '        label: qsTr("Wrapped")\n'
                '        accessibleName: ready ? "Open" :\n'
                '                                qsTr("Closed")\n'
                '        icon: "smart_toy"\n'
                '    }\n'
                '}\n',
                encoding="utf-8",
            )
            report = scan(Path(directory))
        self.assertEqual(2, len(report), report)
        self.assertIn("'Shown to the reader'", report[0])
        self.assertIn("'Open'", report[1])


if __name__ == "__main__":
    sys.exit(unittest.main())
