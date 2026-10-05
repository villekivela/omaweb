# Code style and quality gates

These rules apply to first-party source, tests, build files, and automation. Vendored, generated,
cached, and archived files keep their upstream or historical formatting.

## Common rules

- Use UTF-8, LF line endings, and one final newline.
- Remove trailing whitespace and indentation on blank lines.
- Use spaces for indentation. Makefiles may use tabs where the format requires them.
- Target 100 characters per line in first-party code and Markdown prose. Formatter output,
  unbreakable URLs, and generated identifiers may exceed the target.
- Keep changes focused. Do not mix behavior changes with mechanical formatting unless the behavior
  change requires it.

## Languages

- C++ uses C++23 and the repository's `.clang-format`. CMake enables `-Wall`, `-Wextra`, and
  `-Wpedantic` for first-party targets.
- QML uses `.qmlformat.ini`, including semicolons for JavaScript statements. Qt's `qmllint` rejects
  syntax errors and the high-signal warning categories configured in `.qmllint.ini`.
- A QML position worked out by halving, such as centring by hand, is wrapped in `Math.round`. On
  half a pixel, borders and text are drawn soft and a page's texture splits along its diagonal
  (#567). `anchors.centerIn` and the centre anchors round already.
- JavaScript uses the repository's Prettier configuration and strict equality.
- Python follows PEP 8 with four-space indentation. Scripts must run with the supported Python 3
  interpreter and use only declared dependencies.
- CMake uses lowercase commands, two-level four-space indentation, and quoted paths.
- Shell scripts use POSIX shell unless the shebang names Bash. Quote expansions unless splitting is
  the intended behavior.

## User-facing strings

Every string a reader sees in the chrome is wrapped for translation, as
[ADR 0056](../adr/0056-translate-the-chrome-with-qt-linguist.md) records.

- QML: `qsTr("Text")`. C++: `tr("Text")` in a `QObject`, otherwise
  `QCoreApplication::translate("Context", "Text")`. The context is the QML file's name or the C++
  class's, so each surface has its own part of the catalogue.
- A value goes in as `%1`, `.arg()` on the wrapped string: `qsTr("Closed %1").arg(title)`. Never
  join translated pieces into a sentence, because word order differs between languages.
- A count is a plural: `qsTr("%n tab(s)", "", count)`. Finnish gets the singular and the plural form
  from the catalogue. English has no catalogue, so `%n tab(s)` would read "(s)" there: where English
  must read as prose, write the singular and the plural as two strings.
- A short string that is ambiguous alone takes a disambiguation comment as the second argument:
  `qsTr("Open", "verb: open a page")`.
- Wrap a string only if a reader reads it. Object names, log messages, identifiers, file formats and
  `objectName`s are not translated. An accessible name a screen reader speaks is.
- A new string is added to `translations/omaweb_fi.ts` in the same change, translated, following
  [the translation guide](../localization.md).

### The translation gate

The `omaweb-wrapped-strings` test, part of `ctest --preset ci`, fails the build when:

- A QML property that carries reader text (`text`, `label`, `title`, `note`, `accessibleName` and
  the like) holds a literal outside `qsTr`. A plain lowercase word counts: `text: "privacy"` fails.
  A name with an underscore or a hyphen is a token. A literal compared with `===`, a `case` label
  and the kind passed to `mediaLabel` are values. An icon's glyph name, a colour or a program's name
  that is one plain word goes in `TOKENS` in `scripts/check_wrapped_strings.py`, under its file.
- A C++ literal that starts with a capital and has two words sits outside `tr(`,
  `QCoreApplication::translate(` and a log statement. Text that stays English on purpose, such as a
  stored title or a product name, goes in `CPP_ENGLISH` with its reason. An Agent's wire text and
  SQL are not the chrome's and are skipped by file name.
- `translations/omaweb_fi.ts` has an unfinished or empty entry, lacks a string the sources wrap, or
  keeps one they no longer do. Refresh it with
  `cmake --build --preset ui --target update_translations`, then translate the new entries.

`scripts/check_wrapped_strings.py` prints the same report without a build. The comparison with the
sources needs `lupdate`, which the test takes from the build and the script from `PATH`. The scan is
a heuristic over source text and reads one-word C++ strings and a QML property it does not know as
English, so review remains responsible for those.

## Required gates

Format or check the files changed from `origin/main` during normal development:

```sh
scripts/format.sh --changed origin/main
scripts/check_changed.sh origin/main
```

A decision recorded under `docs/adr/` takes the next free number, which
`scripts/check_adr_numbers.py` prints and CI enforces. Two branches open at once have already landed
two decisions numbered 0038, so the number a branch chose is checked against the tree it merges into
rather than the tree it was written in.

Use the full-tree mode after changing format rules or upgrading a formatter:

```sh
scripts/format.sh --all
scripts/check_format.sh --all
scripts/check_qml.sh --all
```

Run these remaining gates before merge:

```sh
git diff --check
cmake --build --preset ci
ctest --preset ci
```

The build and the tests are the gate for a change a compiler reads. A change confined to `docs/`,
`website/` or Markdown is not one, and CI skips its five Arch jobs for the same reason;
`scripts/source_changed.sh <base>` answers which kind a range is.

CI runs the formatters and rejects any resulting diff. It then runs `qmllint`, the build, and the
test suite with compiler warnings enabled, under both clang and GCC, because warnings are `-Werror`
here and the two compilers do not report the same set. No preset names a compiler, so the commands
above use whichever the host has, and either one is a configuration CI also checks. A change is
ready to merge only when every applicable command passes.

## Code that calls something this repository does not run

A change that calls an external service is exercised against that service before it merges, not
after. A stub answers the way the author expected; the service answers the way it answers, and the
difference between those two is where this kind of change fails.

Give it a way to be run against the real thing without publishing anything: a dry-run input, a flag
that writes what it would have written, a single-item mode. Then run it, read the output, and say in
the pull request what came back.

The release notes rewrite is what this rule is made of. It was tested against a stubbed API, merged,
and cut a release that published the generated commit list instead of notes, because the model's
thinking spent a token budget sized for the answer alone. Five further pull requests found the rest
by publishing them: a heading with nothing under it, notes that could not be read back, the
project's own vocabulary, a fenced command, an underlined title. Each was one real call away from
being found before it shipped.
