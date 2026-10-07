# Keep the reader's settings in one file

Refines [0016](0016-separate-user-configuration-from-application-data.md), which put the reader's
configuration in the configuration directory. Keybindings, search engines and the theme were files
there that a reader could edit, diff and keep in their dotfiles. The rest of their settings were
not: three undocumented files, `interface.json`, `downloads.json` and `privacy.json`, and about ten
preference rows in the session store that only the Settings page could reach. The rows could not be
versioned, reviewed or copied to a new machine without Sync, and only some of the files were
followed while Omaweb ran.

## One file, sparse and typed

`settings.json` in the configuration directory is the source of truth for the reader's settings, and
the session store no longer holds them. It is flat JSON with `"version": 1`, the keys keep the names
they had (`"sidebar-side": "right"`, `"https-only": false`) and the values are typed rather than the
store's strings. `page-fonts` stays an object, and the Known extension switches are one,
`"known-extensions": {"bitwarden": true}`. The download directory was `directory` in a file of its
own, which says nothing in a flat file, so it is `download-directory`.

The file holds only what the reader changed. Choosing the default in Settings removes the key, so a
default that changes in a later build reaches a reader who never chose otherwise.

What stays out:

- `allow-agents` and `agent-command` are in `agents.json` beside it, watched as `privacy.json` was.
  A reader who copies `settings.json` to a new machine does not turn Agents on there by accident
  ([0051](0051-hand-the-browser-to-an-agent.md)). Space grants, Agent Space labels and projects stay
  in SQLite.
- Machine state stays in the store: the sidebar and developer tools widths, the release check's
  dates, the Known extension check dates, `put-away-notice-given` and `readers-space-shown`.
- The clear-data categories and range stay in the store, because
  [0031](0031-distinguish-a-selection-from-a-setting.md) calls them selections.
- Content blocking's subscriptions, user rules and disabled sites stay in their own document.

## One writer, and a watch

`SettingsFile` owns every read, write and merge of the file. A write reads the file again under a
lock file and writes it through `QSaveFile`, so two writers in one process or two never drop each
other's keys. It keeps keys it does not know and the order the reader wrote the keys in, puts a new
key at the end, and indents with two spaces. Sync's restore writes through it too.

The file is watched as `theme.json` is. A change applies each changed key through the path a
Settings change takes, so what applies live from the page applies live from the file, and a Private
window reads the same file and writes there what it can change.

## A file Omaweb cannot read is not overwritten

A file that does not parse, or names another version, leaves the values read last standing, or the
defaults at start. Settings says why at its top, the Settings button is marked for attention, and
the controls that would write the file are disabled until it is fixed, so Omaweb never overwrites a
file it could not read. Sync's restore refuses to write into it too. A file without a `version` is
read as version 1, so a file a reader writes by hand from the keys alone is read.

A single value of the wrong type falls back to its default, and Settings names it. A key Omaweb does
not know is kept in the file and listed as ignored. Every case is logged to stderr.

## Sync

Sync carries the same three settings as before (`floating-controls`, `use-favicons`,
`tint-favicons`) as the same per-key records, with the conflict rule of
[0039](0039-sync-configuration-through-a-git-remote.md). Restore merges them into `settings.json`
and the browser's watch applies them, and capture reads them from the file. A record still holds its
value as text, as every version of the record has. A key taken out of the file is a change to sync
only where this machine applied or captured a value for it before. A machine that never chose one
has nothing to tell the others.

## Migration

At start, if `settings.json` does not exist, Omaweb builds it from `interface.json`,
`downloads.json`, `privacy.json` and the store's rows, writes the agent keys to `agents.json`, and
then deletes the old files and rows. The Start page's old road switch is read once more there, so a
reader who turned it off arrives on the None Scene. If `settings.json` exists, from the reader's
dotfiles say, an old file beside it is ignored, logged, and named in Settings for the reader to
delete. An older build after the migration starts from default settings, and the release notes say
so.
