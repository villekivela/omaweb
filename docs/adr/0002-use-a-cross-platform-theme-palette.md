# Use a cross-platform theme palette

Tanto loads its theme palette from a versioned JSON file and watches both the file and its parent directory for changes on macOS and Linux. This survives theme managers that atomically replace a generated directory.

On Omarchy Quattro, Tanto ships a `tanto.json.tpl` user template. Omarchy renders it into `~/.local/state/omarchy/current/theme/tanto.json`, which Tanto detects directly. Normal theme switching does not depend on a synchronous `theme-set` hook or D-Bus call. An explicit non-blocking reload command may remain as a fallback. This keeps QML independent of Omarchy and makes the same palette contract testable on macOS.

A user may also place a `theme.json` in Tanto's configuration directory, and `scripts/import_terminal_theme.py` derives one from the colours of the terminal it runs in for desktops that have no theme manager of their own. ADR 0016 records the lookup order between those sources.

The palette defines semantic opacity values for Tanto-owned surfaces such as the vertical sidebar, address bar, overlays, and empty window background. Those surfaces request native background blur where the platform or compositor provides it, fall back to alpha transparency, and then to an opaque theme color. Webpage viewports remain opaque, and transparent surfaces continue to receive pointer input.

Every theme also defines a distinct Private-window treatment. Tanto enforces a minimum visual difference so a custom palette cannot make private and persistent browsing indistinguishable.

System reduced-motion, increased-contrast, and reduced-transparency preferences override the active palette. Transparent surfaces fall back to opaque colors when accessibility settings require it. Keyboard commands update state without waiting for interface animations.
