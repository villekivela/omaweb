# Translate the chrome with Qt Linguist

The chrome was English only. It is translated with Qt's own tooling, so that no catalogue format,
runtime or dependency is added.

- **Source strings are the English text.** QML wraps a string in `qsTr()`, C++ in `tr()` inside a
  `QObject` or in `QCoreApplication::translate()` elsewhere. A locale without a catalogue shows the
  source, so English needs no catalogue.
- **A context per surface.** `lupdate` names a QML file's context after the file, so `StartPage.qml`
  is the context `StartPage`. A C++ class is its own context. Two batches wrapping different
  surfaces touch different parts of the catalogue, which is what lets them merge.
- **Catalogues are `translations/omaweb_<locale>.ts`.** They are committed, written with
  `-locations none -no-obsolete`, so a refresh does not rewrite a line a change did not touch. The
  build compiles each to `omaweb_<locale>.qm` with `lrelease`, and the package installs them under
  `share/omaweb/translations`.
- **The locale is the reader's POSIX one.** `LC_ALL`, then `LC_MESSAGES`, then `LANG`, then the
  system locale. Omaweb loads `omaweb_<locale>.qm`, then the region-less name, from the build tree,
  then from `../share/omaweb/translations` beside the executable. The package's prefix is not baked
  into the binary.
- **Placeholders are `%1`, and plurals are `%n`.** `qsTr("%1 tabs")` and
  `qsTr("%n tab(s)", "", count)`. A string is never built by joining translated pieces.
- **The UI lab takes `--locale <name>`**, so a mock is reviewed in Finnish without changing the
  shell's environment.

Finnish is the first locale, and the first surface wrapped is the Start page. The remaining surfaces
follow in separate changes, each adding strings to the same catalogue.
