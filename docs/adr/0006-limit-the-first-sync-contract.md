# Limit the first Sync contract

Sync uses a replaceable provider and initially covers Space names and colors, pinned tabs, open-tab
sessions, settings, keybindings, theme selection, and content-blocking configuration. It excludes
cookies, active logins, history, downloads, passwords, and private-window data because synchronizing
secrets would require a separate security design. The browser core does not assume a hosted Omaweb
service.

Two parts of this contract are narrowed by
[ADR 0039](0039-sync-configuration-through-a-git-remote.md): settings sync per key rather than as
one document, because some settings are machine-shaped, and content-blocking configuration means
filter list subscriptions alone, because the per-site list of sites with blocking turned off names
sites the reader visited. The rest of this decision stands.
