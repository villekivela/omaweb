# Isolate browsing identities by Space

Omaweb presents each persistent browsing identity as a Space backed by an isolated engine profile.
Cookies, site storage, permissions, history, sessions, and tabs stay within their Space, while
application settings such as the theme, keybindings, downloads, filter lists, and search
configuration remain shared.

Only the active Space keeps live pages. Omaweb serializes and suspends inactive Spaces, then
restores their tabs when selected. Applying the same lifecycle to both engines is a deliberate
memory policy. Ladybird's named profiles can now run side by side, so Ladybird no longer forces this
restriction.

Within the active Space, Omaweb may suspend old background tabs under memory pressure. It does not
automatically suspend tabs that play or capture media, download data, contain edited form state, or
are explicitly marked `Keep active`. A pin does not imply `Keep active`.

First launch creates one renameable Space named `Personal`. Moving a tab between Spaces transfers
its address and pinned status, warns about edited form state, and reloads under the destination
Space's browsing identity. Deleting a populated Space requires typing its name and removes its
database and engine-profile directories.
