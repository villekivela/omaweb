#!/usr/bin/env python3
"""No reader-facing string literal is left unwrapped in the chrome's QML or C++.

ADR 0056 has every string a reader sees go through `qsTr`. This reads the
property assignments that carry reader-visible text in `src/ui`, `src/ui-lab`
and the engine view, and fails on a literal that is not inside a translation
call. It is a heuristic over source text, not a QML parser: an identifier
with an underscore or a hyphen (`"smart_toy"`) is taken for an icon or a
token, a literal compared with `===` is a value, a date format and the
product name are skipped, and a token passed to `.arg()` is a value rather
than prose. A plain lowercase word is a section label until it is named in
`TOKENS`, so `text: "privacy"` fails.

C++ is read for prose: a literal that starts with a capital and has two words, outside a
`tr(` or `translate(` call and outside a log statement. A literal that is English on
purpose is named in `CPP_ENGLISH` with the reason, and the files whose text is wire text
or SQL, not the chrome's, are named in `CPP_NOT_CHROME`.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ElementTree
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCANNED = ("src/ui", "src/ui-lab", "src/engine/qt")

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
IDENTIFIER = re.compile(r"^[a-z][a-z0-9]*[_.\-][a-z0-9_.\-]*$")
# A literal that selects rather than speaks: one side of a comparison, a `case`,
# or the kind handed to `mediaLabel`, which wraps what the reader sees.
COMPARED = re.compile(
    r"(?:[=!]==?\s*\"[^\"]*\"|\"[^\"]*\"\s*[=!]==?|\bcase\s+\"[^\"]*\"|mediaLabel\(\s*\"[^\"]*\")"
)

# Single lowercase words that are not prose, by the file that carries them: an
# icon's glyph name, a colour, and a program's name. A word that is read goes
# through `qsTr`.
TOKENS = {
    ("FindBar.qml", "search"),
    ("TabRow.qml", "close"),
    ("SpaceOutline.qml", "warning"),
    ("SpaceOutline.qml", "lock"),
    ("SpaceOutline.qml", "shield"),
    ("NightRoad.qml", "white"),
    ("NightSky.qml", "white"),
    ("GameOfLife.qml", "white"),
    ("VectorTerrain.qml", "white"),
    ("Radar.qml", "white"),
    ("Hyperspace.qml", "white"),
    ("SettingsPage.qml", "claude"),
}


SCANNED_CPP = ("src", "modules")
CPP_SUFFIXES = {".cpp", ".mm"}

# Files whose text is not the chrome's: an Agent's wire text must not change with the
# reader's locale, and the rest is SQL. The UI lab is a developer tool and is not shipped.
CPP_NOT_CHROME = (
    "Agent",
    "ControlSocket",
    "SqliteSessionStore",
    "HistoryQuery",
    "HistorySearch",
    "EngineCapabilities",
)

# Prose-looking literals that stay English by design, as {file: {literal: reason}}.
CPP_ENGLISH = {
    "BrowserController.cpp": {
        "New tab": "the title a tab is stored under, translated by the tab list when shown",
        "Agent activity": "the title the Agent activity tab is stored under",
        "Brave Search": "a search engine's name",
        "CLEAR ALL": "the word a reader types, compared by value",
        "A window's controller is handed to QML, not built there.": "a developer assertion",
    },
    "TabListModel.cpp": {
        "New tab": "compared with the stored title, then translated",
        "Agent activity": "compared with the stored title, then translated",
    },
    "Downloads.cpp": {"A window is given the downloads its controller holds.": "an assertion"},
    "FontSettings.cpp": {"The reader's font settings are handed to QML, not built there.": "an assertion"},
    "PaymentCards.cpp": {
        "American Express": "a card network's name",
        "Diners Club": "a card network's name",
    },
    "SecretServiceKeyring.cpp": {
        "Omaweb payment card": "the label every card's keyring item is stored under",
    },
    "KnownExtensions.cpp": {
        "Bitwarden Password Manager": "a product name",
        "AgileBits Inc.": "a publisher's name",
    },
    "ContentBlocker.cpp": {"EasyList Cookie": "a filter list's name"},
    "ThemeController.cpp": {"JetBrains Mono": "a font family", "DejaVu Sans Mono": "a font family"},
    "QtContentBlocker.cpp": {"Omaweb Global Privacy Control": "a script's name inside the engine"},
    "LinuxSecretStore.cpp": {"Omaweb Sync": "the label the keyring stores a secret under"},
    "SyncModule.cpp": {"Omaweb Sync": "the author name written into a commit"},
    "LinuxMediaAnnouncer.cpp": {"D-Bus Interface": "the name of a D-Bus interface"},
    "LinuxRunningBrowser.cpp": {"D-Bus Interface": "the name of a D-Bus interface"},
    "GitHubForge.cpp": {
        "Encrypted Omaweb browser synchronization": "the description sent to GitHub",
    },
}
LOGGING = re.compile(r"^(?:q(?:Debug|Info|Warning|Critical|Fatal)|qC\w+|Q_ASSERT\w*|QCOMPARE\w*)$")
CPP_TOKEN = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:[^\"\\\n]|\\.)*\"|[A-Za-z_][A-Za-z_0-9:]*|[();{}]|\S", re.S
)
CPP_PROSE = re.compile(r"[A-Za-z]{3,} [A-Za-z]{2,}")


def cpp_violations(path: Path, source: str) -> list[tuple[int, str]]:
    """Prose literals outside a translation call and outside a log statement."""
    found = []
    calls: list[str] = []
    logging = False
    previous = ""
    for match in CPP_TOKEN.finditer(source):
        token = match.group()
        if token == "(":
            calls.append(previous.split("::")[-1])
        elif token == ")":
            if calls:
                calls.pop()
        elif token in ";{}":
            logging = False
        elif token.startswith('"'):
            literal = token[1:-1]
            if (
                CPP_PROSE.search(literal)
                and literal[:1].isupper()
                and not logging
                and not any(call in ("tr", "translate", "QT_TRANSLATE_NOOP") for call in calls)
                and not any(LOGGING.match(call) for call in calls)
                and literal not in CPP_ENGLISH.get(path.name, {})
            ):
                found.append((source.count("\n", 0, match.start()) + 1, literal))
        elif LOGGING.match(token.split("::")[-1]):
            logging = True
        if not token.isspace() and not token.startswith(("//", "/*")):
            previous = token
    return found


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
        statement = COMPARED.sub("", statement)
        statement = re.sub(r"\.arg\(\s*\"[^\"]*\"\s*\)", "", statement)
        for literal in LITERAL.findall(statement.split(":", 1)[1]):
            if not re.search(r"[A-Za-z]{2}", literal) or IDENTIFIER.match(literal):
                continue
            if literal in PRODUCT_NAMES or DATE_FORMAT.match(literal):
                continue
            if (path.name, literal) in TOKENS:
                continue
            found.append((start + 1, literal))
    return found


def scan(root: Path) -> list[str]:
    report = []
    for directory in SCANNED:
        for path in sorted((root / directory).rglob("*.qml")):
            if path.name in NOT_CHROME:
                continue
            for line, literal in violations(path, path.read_text(encoding="utf-8")):
                report.append(f"{path.relative_to(root)}:{line}: {literal!r}")
    for directory in SCANNED_CPP:
        for path in sorted((root / directory).rglob("*")):
            if path.suffix not in CPP_SUFFIXES or "ui-lab" in path.parts:
                continue
            if path.name.startswith(CPP_NOT_CHROME):
                continue
            for line, literal in cpp_violations(path, path.read_text(encoding="utf-8")):
                report.append(f"{path.relative_to(root)}:{line}: {literal!r}")
    return report


def untranslated(catalogue: Path) -> list[str]:
    """The entries of a Qt Linguist catalogue a translator has not finished.

    An entry is unfinished when it says so or when its translation is empty, which is what
    `lupdate` writes for a string it has just found. A plural is unfinished when any form is.
    """
    found = []
    for context in ElementTree.parse(catalogue).getroot().iter("context"):
        name = context.findtext("name")
        for message in context.iter("message"):
            translation = message.find("translation")
            if translation is None or translation.get("type") in ("vanished", "obsolete"):
                continue
            forms = [form.text or "" for form in translation.iter("numerusform")] or [
                translation.text or ""
            ]
            if translation.get("type") == "unfinished" or not all(form.strip() for form in forms):
                found.append(f"{name}: {message.findtext('source')!r}")
    return found


def messages(catalogue: Path) -> set[tuple[str, str, str]]:
    found = set()
    for context in ElementTree.parse(catalogue).getroot().iter("context"):
        for message in context.iter("message"):
            found.add(
                (
                    context.findtext("name") or "",
                    message.findtext("source") or "",
                    message.findtext("comment") or "",
                )
            )
    return found


def out_of_date(lupdate: Path, root: Path, catalogue: Path) -> list[str]:
    """What a refresh of the catalogue from the sources would add or drop.

    Runs the refresh `update_translations` runs, into a copy, so the checkout is untouched.
    A string wrapped without a catalogue entry is `added`, and an entry whose string is gone
    from the sources is `dropped`.
    """
    with tempfile.TemporaryDirectory() as directory:
        refreshed = Path(directory) / catalogue.name
        shutil.copy(catalogue, refreshed)
        subprocess.run(
            [str(lupdate), "-locations", "none", "-no-obsolete", "-silent"]
            + list(SCANNED_CPP)
            + ["-ts", str(refreshed)],
            cwd=root,
            check=True,
        )
        before, after = messages(catalogue), messages(refreshed)
    return [f"added: {m[0]}: {m[1]!r}" for m in sorted(after - before)] + [
        f"dropped: {m[0]}: {m[1]!r}" for m in sorted(before - after)
    ]


def report_all(root: Path) -> list[str]:
    """Every problem the gate reports, which is what the test asserts piece by piece."""
    catalogue = root / "translations" / "omaweb_fi.ts"
    problems = scan(root)
    problems += [f"untranslated: {entry}" for entry in untranslated(catalogue)]
    lupdate = os.environ.get("OMAWEB_LUPDATE") or shutil.which("lupdate")
    if lupdate:
        problems += out_of_date(Path(lupdate), root, catalogue)
    else:
        print("no lupdate found: the catalogue was not compared with the sources", file=sys.stderr)
    return problems


if __name__ == "__main__":
    problems = report_all(ROOT)
    print("\n".join(problems))
    sys.exit(1 if problems else 0)
