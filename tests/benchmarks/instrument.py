#!/usr/bin/env python3
"""Adds measurement-only counters to a worktree under benchmark.

The counters record how often the cosmetic path is entered. They add no
decision of their own, so a revision instrumented here behaves exactly as it
does when it is built normally. The same anchors exist in both compared
revisions, which is what keeps the two instrumented builds comparable.
"""

import argparse
import pathlib
import sys

ENGINE_VIEW = "src/engine/qt/EngineView.qml"
MATCHER_HEADER = "src/content-blocking/ContentMatcher.h"
MATCHER_SOURCE = "src/content-blocking/ContentMatcher.cpp"
BLOCKER_HEADER = "src/content-blocking/omaweb_blocker.h"
BLOCKER_SOURCE = "third_party/content-blocker/src/lib.rs"

COUNTER_PROPERTIES = """    property bool cosmeticRulesInjected: false
    // Benchmark instrumentation. Counts the scripts the cosmetic path sends
    // into the page and the survey replies it takes back.
    property int cosmeticScriptCalls: 0
    property int cosmeticSurveyCallbacks: 0
"""

GENERIC_CLEAR_CALL = (
    "        webView.runJavaScript(root.styleSheetSnippet("
    'root.genericCosmeticElementId, ""));\n'
)

ENGINE_VIEW_EDITS = [
    ("    property bool cosmeticRulesInjected: false\n", COUNTER_PROPERTIES),
    (
        "        webView.runJavaScript(root.styleSheetSnippet(root.cosmeticElementId, css));\n",
        "        root.cosmeticScriptCalls += 1;\n"
        "        webView.runJavaScript(root.styleSheetSnippet(root.cosmeticElementId, css));\n",
    ),
    (
        GENERIC_CLEAR_CALL,
        "        root.cosmeticScriptCalls += 1;\n" + GENERIC_CLEAR_CALL,
    ),
    (
        "        webView.runJavaScript(",
        "        root.cosmeticScriptCalls += 1;\n        webView.runJavaScript(",
        "const classes = new Set(), ids = new Set();",
    ),
    (
        '+ "})()", function (survey) {\n',
        '+ "})()", function (survey) {\n'
        "                                  root.cosmeticSurveyCallbacks += 1;\n",
    ),
    (
        "                                  root.genericCosmeticRulesInjected = true;\n",
        "                                  root.genericCosmeticRulesInjected = true;\n"
        "                                  root.cosmeticScriptCalls += 1;\n",
    ),
]

LOOKUP_COUNTER_FIELD = """pub struct OmawebBlocker {
    engine: Engine,
    // Benchmark instrumentation. Counts calls that reach url_cosmetic_resources.
    cosmetic_lookups: AtomicU64,
"""

LOOKUP_COUNTER_METHOD = """
impl OmawebBlocker {
    fn counted_cosmetic_resources(
        &self,
        url: &str,
    ) -> adblock::cosmetic_filter_cache::UrlSpecificResources {
        self.cosmetic_lookups.fetch_add(1, Ordering::Relaxed);
        self.engine.url_cosmetic_resources(url)
    }
}

fn input(value: *const c_char) -> Option<String> {"""

LOOKUP_COUNTER_EXPORT = """
#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be a live matcher.
pub unsafe extern "C" fn omaweb_blocker_cosmetic_lookup_count(
    blocker: *const OmawebBlocker,
) -> u64 {
    unsafe { blocker.as_ref() }.map_or(0, |blocker| {
        blocker.cosmetic_lookups.load(Ordering::Relaxed)
    })
}
"""


def replace(text, old, new, near=None):
    """Replace one anchored occurrence, or stop because the anchor moved."""
    if near is not None:
        end = text.find(near)
        if end < 0:
            sys.exit(f"the anchor {near!r} is gone")
        start = text.rfind(old, 0, end)
        if start < 0:
            sys.exit(f"no {old!r} before {near!r}")
        return text[:start] + new + text[start + len(old) :]
    if text.count(old) != 1:
        sys.exit(f"{text.count(old)} occurrences of {old!r}, expected one")
    return text.replace(old, new)


def instrument_engine_view(root):
    path = root / ENGINE_VIEW
    text = path.read_text()
    if "cosmeticScriptCalls" in text:
        return
    for edit in ENGINE_VIEW_EDITS:
        text = replace(text, edit[0], edit[1], edit[2] if len(edit) > 2 else None)
    path.write_text(text)


def instrument_lookup_counter(root):
    """Adds the counter the implementation already carries to a revision without it."""
    source = root / BLOCKER_SOURCE
    if "cosmetic_lookups" in source.read_text():
        return False
    text = source.read_text()
    text = text.replace(
        "use std::sync::LazyLock;",
        "use std::sync::LazyLock;\nuse std::sync::atomic::{AtomicU64, Ordering};",
    )
    text = text.replace("pub struct OmawebBlocker {\n    engine: Engine,\n", LOOKUP_COUNTER_FIELD)
    text = text.replace(
        "\nfn input(value: *const c_char) -> Option<String> {", LOOKUP_COUNTER_METHOD
    )
    text = text.replace(
        "blocker.engine.url_cosmetic_resources(&url)", "blocker.counted_cosmetic_resources(&url)"
    )
    text = text.replace(
        "        Box::into_raw(Box::new(OmawebBlocker {\n            engine,\n",
        "        Box::into_raw(Box::new(OmawebBlocker {\n            engine,\n"
        "            cosmetic_lookups: AtomicU64::new(0),\n",
    )
    source.write_text(text.rstrip("\n") + "\n" + LOOKUP_COUNTER_EXPORT)

    header = root / BLOCKER_HEADER
    text = header.read_text()
    text = text.replace("#include <stdbool.h>", "#include <stdbool.h>\n#include <stdint.h>")
    text = text.replace(
        "void omaweb_blocker_string_free(char *value);",
        "uint64_t omaweb_blocker_cosmetic_lookup_count(const OmawebBlocker *blocker);\n"
        "void omaweb_blocker_string_free(char *value);",
    )
    header.write_text(text)

    matcher_header = root / MATCHER_HEADER
    text = matcher_header.read_text()
    text = text.replace(
        "    bool cosmeticSurveyWanted(const QUrl &url) const;",
        "    // Benchmark instrumentation. Counts assembled URL resources.\n"
        "    quint64 cosmeticLookupCount() const;\n"
        "    bool cosmeticSurveyWanted(const QUrl &url) const;",
    )
    matcher_header.write_text(text)

    matcher_source = root / MATCHER_SOURCE
    text = matcher_source.read_text()
    text = text.replace(
        "bool ContentMatcher::cosmeticSurveyWanted(const QUrl &url) const",
        "quint64 ContentMatcher::cosmeticLookupCount() const\n"
        "{\n"
        "    return omaweb_blocker_cosmetic_lookup_count(d->blocker);\n"
        "}\n\n"
        "bool ContentMatcher::cosmeticSurveyWanted(const QUrl &url) const",
    )
    matcher_source.write_text(text)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("worktree", type=pathlib.Path)
    arguments = parser.parse_args()
    root = arguments.worktree
    instrument_engine_view(root)
    added = instrument_lookup_counter(root)
    print(f"instrumented {root}: lookup counter {'added' if added else 'already present'}")


if __name__ == "__main__":
    main()
