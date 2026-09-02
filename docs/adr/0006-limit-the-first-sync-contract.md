# Limit the first Sync contract

Sync uses a replaceable provider and initially covers Space names and colors, pinned tabs, open-tab sessions, settings, keybindings, theme selection, and content-blocking configuration. It excludes cookies, active logins, history, downloads, passwords, and private-window data because synchronizing secrets would require a separate security design. The browser core does not assume a hosted Omaweb service.
