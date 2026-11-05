# Keep a Folder's members in a record of its own

A Folder's name, position, Space and ordered tab ids are one Sync record, in a new `folders` kind,
and the session store keeps them the same way beside the Space's tabs. A tab carries nothing about
its Folder (#918). This extends what [0039](0039-sync-configuration-through-a-git-remote.md) says of
records: each Space, tab and Folder is an independently encrypted record.

## Why not a field on the tab

An older build rebuilds a Tab record from a fixed list of fields, so a `folderId` on the tab would
be lost the first time that build wrote the record back. A record kind it doesn't name is left
alone: its history cleanup walks `spaces` and `tabs` by name and snapshots the whole checkout, so
readers need not update every machine at once. `compactionKeepsARecordKindItDoesNotKnow` holds that
to the code, and every release from v0.3.0 to v0.20.0 has the same cleanup.

## What follows

- Two machines changing one Folder's members at once keep the newer change, as for any record.
- A tab id a machine doesn't have is ignored when the Folder is read, and stays in the stored record
  until the Folder next changes.
- A tab added on another machine while a Folder closes follows the newer record. If the close is
  newer, the tab stays as an ordinary tab, because no tab says which Folder it was in. If the add is
  newer, its record wins and the Folder stays, holding only that tab, since the tabs that closed are
  ids nothing here has any more.
- The window holds a Space's Folders in memory and writes them whole, so a Folder-only change has to
  count as a change to the local Sync fingerprints in both directions. If it didn't, a rename would
  never be pushed, and a remote one would be written away by the next local change.

Whether a Folder is collapsed stays on this machine: the store keeps it, and the record never holds
it.
