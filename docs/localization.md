# Localization

The chrome is translated with Qt Linguist.
[ADR 0056](adr/0056-translate-the-chrome-with-qt-linguist.md) records the mechanism, and
[code style](agents/code-style.md#user-facing-strings) states how a string is written. This page is
for the translator and for whoever resolves a catalogue conflict.

## Finnish wording

Follow the terms the established Finnish desktop translations use: KDE, GNOME and Firefox's Finnish
localization. When they disagree, GNOME and Firefox win over KDE, because Omaweb's readers come from
a browser. Write the plain singular imperative for commands (Avaa, Tallenna, Sulje), the noun for labels, and
sentence case. Keep a key name as the keyboard prints it.

Omaweb's own words are translated once, here, so every batch agrees:

| English        | Finnish               | Note                                                                                  |
| -------------- | --------------------- | ------------------------------------------------------------------------------------- |
| Space          | Tila                  | A browsing identity. Chosen over "työtila", which Omarchy readers know as Hyprland's workspace. |
| Omnibar        | Omnibar               | Kept, it is a product term. Inflect it: "Omnibarissa".                                |
| Glance         | Pikakatsaus           | The page shown over the tab it came from.                                             |
| Agent          | Agentti               | A program that drives the browser.                                                    |
| Agent tab      | Agentin välilehti     | The tab an Agent is attached to.                                                      |
| Start page     | Aloitussivu           |                                                                                       |
| Tab            | Välilehti             |                                                                                       |
| Pinned tab     | Kiinnitetty välilehti |                                                                                       |
| Sidebar        | Sivupalkki            |                                                                                       |
| Private window | Yksityinen ikkuna     |                                                                                       |
| Shortcuts      | Pikanäppäimet         |                                                                                       |
| History        | Historia              |                                                                                       |
| Downloads      | Lataukset             |                                                                                       |
| Settings       | Asetukset             |                                                                                       |

Add a row when a batch has to translate a new Omaweb term.

## Resolving a catalogue conflict

Two branches that add strings both edit `translations/omaweb_fi.ts`. Contexts are separate, so a
conflict is usually two additions side by side. When a rebase stops on the file:

1. Take main's file: `git checkout --ours translations/omaweb_fi.ts` during a rebase, which is main
   there.
2. Rerun the refresh, `cmake --build --preset ui --target update_translations`, which adds the
   branch's strings to it as unfinished entries.
3. Copy the branch's translations back in for those entries.
   `git show ORIG_HEAD:translations/omaweb_fi.ts` is the branch's file. Keep both sides'
   translations and take neither wholesale.
4. Check that no `type="unfinished"` entry is left that the branch translated, then
   `git add translations/omaweb_fi.ts` and `git rebase --continue`.
