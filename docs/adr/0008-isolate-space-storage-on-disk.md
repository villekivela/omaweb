# Isolate Space storage on disk

Each Space owns a directory containing its browser database and separate state directories for each engine. SQLite stores sessions, tabs, pins, history, and other frequently changing structured data, while versioned JSON files store settings. User-editable configuration, meaning keybindings and an optional theme palette, lives outside this layout in the configuration directory described in ADR 0016. One small global database tracks the Space list and application-level state. Omaweb uses direct SQL behind a narrow migration layer rather than an ORM.

Omaweb initially relies on operating-system file permissions and full-disk encryption. It does not add custom database encryption while secrets remain inside engine-managed profiles and Omaweb has no password store.
