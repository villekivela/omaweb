# Keep optional features out of the core

Tanto implements optional first-party capabilities such as Account and Sync as independently compiled shared libraries that load only when enabled. Each module declares the internal contract version it supports. The initial module contract remains private rather than promising a stable third-party binary API. Core browsing, Spaces, sessions, Pinned tabs, content blocking, themes, keyboard navigation, and Developer tools work locally without an Account or any Feature module.
