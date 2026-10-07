#pragma once

#include <QString>

namespace omaweb {

class SessionStore;

// Builds `settings.json` from where an earlier version kept the reader's
// settings (ADR 0061): `interface.json`, `downloads.json`, `privacy.json` and
// the session store's preference rows. The agent keys go to `agents.json`.
// The old files and rows are deleted once the new file is written.
//
// Runs only while `settings.json` does not exist. One that does, from the
// reader's dotfiles say, is theirs: an old file beside it is logged and left
// for Settings to name. False when the new file could not be written, which
// leaves every old place as it was.
bool migrateSettings(const QString &configRoot, SessionStore &store);

} // namespace omaweb
