# Isolate Space storage on disk

Each Space owns a directory containing its browser database and separate state directories for each engine. SQLite stores sessions, tabs, pins, history, and other frequently changing structured data, while versioned JSON files store settings, keybindings, and theme palettes. One small global database tracks the Space list and application-level state. Tanto uses direct SQL behind a narrow migration layer rather than an ORM.

Tanto initially relies on operating-system file permissions and full-disk encryption. It does not add custom database encryption while secrets remain inside engine-managed profiles and Tanto has no password store.
