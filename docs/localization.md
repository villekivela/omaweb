# Localization

The chrome is translated with Qt Linguist.
[ADR 0056](adr/0056-translate-the-chrome-with-qt-linguist.md) records the mechanism, and
[code style](agents/code-style.md#user-facing-strings) states how a string is written. This page is
for the translator and for whoever resolves a catalogue conflict.

## Finnish wording

Follow the terms the established Finnish desktop translations use: KDE, GNOME and Firefox's Finnish
localization. When they disagree, GNOME and Firefox win over KDE, because Omaweb's readers come from
a browser. Write the plain singular imperative for commands (Avaa, Tallenna, Sulje), the noun for
labels, and sentence case. Keep a key name as the keyboard prints it.

Omaweb's own words are translated once, here, so every batch agrees:

| English          | Finnish               | Note                                                                                            |
| ---------------- | --------------------- | ----------------------------------------------------------------------------------------------- |
| Space            | Tila                  | A browsing identity. Chosen over "työtila", which Omarchy readers know as Hyprland's workspace. |
| Omnibar          | Omnibar               | Kept, it is a product term. Inflect it: "Omnibarissa".                                          |
| Glance           | Pikakatsaus           | The page shown over the tab it came from.                                                       |
| Agent            | Agentti               | A program that drives the browser.                                                              |
| Agent tab        | Agentin välilehti     | The tab an Agent is attached to.                                                                |
| Allow agents     | Salli agentit         | The setting that lets Agents read and act in pages.                                             |
| Agent command    | Agentin komento       | What `:ask` runs in the reader's terminal.                                                      |
| Start page       | Aloitussivu           |                                                                                                 |
| Tab              | Välilehti             |                                                                                                 |
| Pinned tab       | Kiinnitetty välilehti |                                                                                                 |
| Sidebar          | Sivupalkki            |                                                                                                 |
| Private window   | Yksityinen ikkuna     |                                                                                                 |
| Shortcuts        | Pikanäppäimet         |                                                                                                 |
| History          | Historia              |                                                                                                 |
| Downloads        | Lataukset             |                                                                                                 |
| Settings         | Asetukset             |                                                                                                 |
| Engine           | Moottori              | The web engine that renders pages.                                                              |
| Sync             | Synkronointi          | The feature and its settings section. Inflect it: "synkronoinnin".                              |
| Site information | Sivuston tiedot       | The panel under the address. Its own labels are lowercase, as the English ones are.             |

Add a row when a batch has to translate a new Omaweb term.

## Plurals in English

English has no catalogue, so `qsTr("%n request(s) blocked", "", count)` shows "1 request(s) blocked"
to an English reader. Use `%n` where the English wording does not change with the count ("and %n
more"). Where a noun does change, wrap the singular and the plural as two strings and pick between
them, as `RefusalTally.qml` does.

## What C++ translates

`lupdate` reads `src/` and `modules/`. A message a reader reads is wrapped in the class that owns
it, or in `QCoreApplication::translate("Context", ...)` where there is no `QObject`. Some text stays
English on purpose:

- Logs, including the startup line that repeats an input method diagnostic.
- Agent verbs, refusals and tool descriptions. An Agent is a program, and its wire text must not
  change with the reader's locale. Command names and descriptions belong to the commands' own batch.
- Values written to disk or compared by value. The Content blocker keeps `updateStatus` as the
  English code and translates it in the report it hands the UI, and a tab titled "New tab" is stored
  as such and translated by the tab list when it is shown.

## Resolving a catalogue conflict

Two branches that add strings both edit `translations/omaweb_fi.ts`. Contexts are separate, so a
conflict is usually two additions side by side. Bring main into the branch with a merge, not a
rebase: a merge needs no force push, and the commit message check skips merge commits. When
`git merge origin/main` stops on the file:

1. Take main's file: `git checkout --theirs translations/omaweb_fi.ts`. In a merge, `--theirs` is
   main and `--ours` is the branch.
2. Rerun the refresh, `cmake --build --preset ui --target update_translations`, which adds the
   branch's strings to it as unfinished entries.
3. Copy the branch's translations back in for those entries.
   `git show HEAD:translations/omaweb_fi.ts` is the branch's file. Keep both sides' translations and
   take neither wholesale.
4. Check that no `type="unfinished"` entry is left that the branch translated, then
   `git add translations/omaweb_fi.ts` and `git commit`.
