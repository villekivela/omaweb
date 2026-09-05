---
status: superseded by ADR-0027
---

# Keep debugged tabs active across Spaces

A tab with a live Debugging session remains active when the user switches Spaces, along with the
minimum engine-profile processes it requires. This is a deliberate exception to ADR 0003's rule that
only the active Space keeps live pages: detaching on every Space switch would break long-running
agent work, while silently suspending the tab would leave clients connected to a page that cannot
progress.

Omaweb shows every retained Space and its resource cost in the session view. Detaching restores
normal suspension, and moving the tab to another Space ends its session because the move changes
browsing identity and reloads under another engine profile.
