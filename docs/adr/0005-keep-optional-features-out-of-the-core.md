# Keep optional features out of the core

Tanto implements optional first-party capabilities such as Account, Sync, and Diagnostics as independently compiled shared libraries that load only when enabled. Each module declares the internal contract version it supports. The initial module contract remains private rather than promising a stable third-party binary API. Core browsing, Spaces, sessions, pinned tabs, content blocking, themes, and keyboard navigation work locally without an Account or any Feature module.
