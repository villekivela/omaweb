#!/usr/bin/env python3
"""The wrapped-strings gate: what it reports, and that the tree passes it."""

from __future__ import annotations

import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import check_wrapped_strings as gate  # noqa: E402

scan = gate.scan

CATALOGUE = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="fi">
<context>
    <name>Probe</name>
    <message><source>Done</source><translation>Valmis</translation></message>
    <message><source>Waiting</source><translation type="unfinished"></translation></message>
    <message><source>Blank</source><translation></translation></message>
    <message numerus="yes"><source>%n tab(s)</source><translation>
        <numerusform>%n välilehti</numerusform><numerusform></numerusform></translation></message>
    <message><source>Kept</source><translation type="vanished">Säilytetty</translation></message>
</context>
</TS>
"""


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

    def test_a_lowercase_word_is_prose_unless_it_is_a_named_token(self):
        with tempfile.TemporaryDirectory() as directory:
            ui = Path(directory) / "src" / "ui"
            ui.mkdir(parents=True)
            (ui / "Probe.qml").write_text(
                'Item {\n'
                '    SectionLabel { text: "privacy" }\n'
                '    Text { text: "smart_toy" }\n'
                '    Text { text: kind === "history" ? "" : qsTr("Open") }\n'
                '}\n',
                encoding="utf-8",
            )
            (ui / "TabRow.qml").write_text(
                'Item {\n    Text { text: "close" }\n}\n', encoding="utf-8"
            )
            report = scan(Path(directory))
        self.assertEqual(1, len(report), report)
        self.assertIn("'privacy'", report[0])

    def test_unwrapped_cpp_prose_is_reported_and_wrapped_or_logged_text_is_not(self):
        with tempfile.TemporaryDirectory() as directory:
            core = Path(directory) / "src" / "core"
            core.mkdir(parents=True)
            (core / "Probe.cpp").write_text(
                'QString a() { return QStringLiteral("Could not save the file"); }\n'
                'QString b() {\n'
                '    return QCoreApplication::translate(\n'
                '        "Probe", "Wrapped on its own line");\n'
                '}\n'
                'void c() { qCInfo(log) << "A log line the reader never sees"; }\n'
                'QString d() { return tr("Wrapped") + QStringLiteral("Open file"); }\n'
                'QString e() { return QStringLiteral("New tab"); }\n',
                encoding="utf-8",
            )
            (core / "AgentProbe.cpp").write_text(
                'QString f() { return QStringLiteral("Wire text for an Agent"); }\n',
                encoding="utf-8",
            )
            report = scan(Path(directory))
        self.assertEqual(3, len(report), report)
        self.assertIn("Probe.cpp:1: 'Could not save the file'", report[0])
        self.assertIn("Probe.cpp:7: 'Open file'", report[1])
        self.assertIn("Probe.cpp:8: 'New tab'", report[2])

    def test_an_untranslated_catalogue_entry_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "omaweb_fi.ts"
            path.write_text(CATALOGUE, encoding="utf-8")
            report = gate.untranslated(path)
        self.assertEqual(["Probe: 'Waiting'", "Probe: 'Blank'", "Probe: '%n tab(s)'"], report)

    def test_the_finnish_catalogue_has_no_untranslated_entry(self):
        self.assertEqual([], gate.untranslated(ROOT / "translations" / "omaweb_fi.ts"))

    def test_the_finnish_catalogue_holds_every_wrapped_string_and_nothing_else(self):
        lupdate = os.environ.get("OMAWEB_LUPDATE") or shutil.which("lupdate")
        if not lupdate:
            self.skipTest("no lupdate; CMake hands the tests the one the build uses")
        self.assertEqual(
            [], gate.out_of_date(Path(lupdate), ROOT, ROOT / "translations" / "omaweb_fi.ts")
        )


if __name__ == "__main__":
    sys.exit(unittest.main())
