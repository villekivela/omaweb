# Use a cross-platform theme palette

Omaweb loads its theme palette from a versioned JSON file and watches both the file and its parent directory for changes on macOS and Linux. This survives theme managers that atomically replace a generated directory.

On Omarchy Quattro, Omaweb ships a `omaweb.json.tpl` user template and installs it itself: Omarchy is the platform Omaweb is built for, so following the desktop's theme is the default rather than three commands the reader has to be told about. The first start on a machine that has Omarchy writes the template to `~/.config/omarchy/themed/` and asks Omarchy to render the theme that is already active. A template already there is never overwritten, because it is a customisation of the reader's or of a theme's; the decision is logged when it differs from the shipped one. `OMAWEB_NO_OMARCHY_TEMPLATE` stops the write for a configuration directory the reader generates or tracks themselves. Omarchy renders the active file into `~/.local/state/omarchy/current/theme/omaweb.json`, which Omaweb detects directly. Normal theme switching does not depend on a synchronous `theme-set` hook or D-Bus call. An explicit non-blocking reload command may remain as a fallback. This keeps QML independent of Omarchy and makes the same palette contract testable on macOS.

A user may also place a `theme.json` in Omaweb's configuration directory, and `scripts/import_terminal_theme.py` derives one from the colours of the terminal it runs in for desktops that have no theme manager of their own. ADR 0016 records the lookup order between those sources.

The palette defines semantic opacity values for Omaweb-owned surfaces such as the vertical sidebar, address bar, overlays, and empty window background. Those surfaces request native background blur where the platform or compositor provides it, fall back to alpha transparency, and then to an opaque theme color. Webpage viewports remain opaque, and transparent surfaces continue to receive pointer input.

Every theme also defines a distinct Private-window treatment. Omaweb enforces a minimum visual difference so a custom palette cannot make private and persistent browsing indistinguishable.

System reduced-motion, increased-contrast, and reduced-transparency preferences override the active palette. Transparent surfaces fall back to opaque colors when accessibility settings require it. Keyboard commands update state without waiting for interface animations.
