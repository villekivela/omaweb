# Isolate Space storage on disk

Each Space owns a directory containing its browser database and separate state directories for each
engine. `SpaceStorage` owns that layout for everyone who asks where something belongs: built from a
data root and an engine name, it says where a Space's database and that engine's Engine profile go
and touches nothing on disk, so asking never creates. Making a directory is a separate, named call.
The recording adapter is the exception, and only inwards: it makes a Space's own directory as it
opens the database in it. A window that keeps nothing holds no `SpaceStorage` at all. SQLite stores
sessions, tabs, pins, history, and other frequently changing structured data, while versioned JSON
files store settings. User-editable configuration, meaning keybindings and an optional theme
palette, lives outside this layout in the configuration directory described in ADR 0016. One small
global database tracks the Space list and application-level state. Omaweb uses direct SQL behind a
narrow migration layer rather than an ORM.

Omaweb initially relies on operating-system file permissions and full-disk encryption. It does not
add custom database encryption while secrets remain inside engine-managed profiles and Omaweb has no
password store.
