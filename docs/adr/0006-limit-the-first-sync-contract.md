# Limit the first Sync contract

Sync uses a replaceable provider and initially covers Space names and colors, ordinary and Pinned
open tabs, an allowlist of settings, keybindings, and filter subscriptions. It excludes theme and
cookies, active logins, history, downloads, passwords, and private-window data because synchronizing
secrets would require a separate security design. The browser core does not assume a hosted Omaweb
service.

The exact allowlist, encryption boundary, and provider design are defined by
[ADR 0039](0039-sync-configuration-through-a-git-remote.md): settings sync per key rather than as
one document, because some settings are machine-shaped, and content-blocking configuration means
filter list subscriptions alone, because the per-site list of sites with blocking turned off names
sites the reader visited.
