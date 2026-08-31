# Tanto product requirements

## Product

Tanto is a keyboard-driven browser for developers. It must work on macOS during development and is intended to become a Linux daily driver with first-class Wayland support. The main application window is frameless, uses vertical tabs, and has transparent browser-owned surfaces.

QtWebEngine is the development engine. Ladybird is the target engine. They ship as separate application build variants and share Tanto's browser model, interface, settings, and engine-contract tests.

## Required browsing model

- One persistent main window with no title bar or visible window controls.
- Any number of named Spaces. Each Space isolates logins, cookies, site data, permissions, history, sessions, ordinary tabs, and pinned tabs.
- Only the active Space keeps live pages. Inactive Spaces serialize and suspend their tabs.
- Private browsing uses separate frameless windows and one temporary identity shared until the last Private window closes.
- Site-requested Auxiliary windows are allowed for authentication, payment, and similar flows. Other new-window requests become tabs.
- Pinned tabs belong to one Space and restore with it. Bookmarks are not part of Tanto.
- Downloads are application-wide. Private downloads remain on disk but do not enter persistent download history.

## Interface

- The vertical sidebar is an outline of the active Space, read top to bottom: the navigation row, the current-address trigger, pinned tabs as icon-only buttons in full-width rows with capacity for three to five tabs, and ordinary tabs as single-line rows. The navigation row carries the sidebar toggle and the command-panel trigger at its leading edge and back, forward and reload at its trailing edge. The footer closes the outline with the Space letters, the Spaces menu and settings, and a private window shows a mask there in place of the letters. Incomplete pinned rows divide their full width among the tabs they contain. New tabs remain available through the Omnibar and keyboard commands rather than a sidebar button.
- Tabs can show site favicons or a two-character host code. The reader can turn favicons off and can choose whether favicon artwork is recoloured to the host-derived tint. With favicons off, the host code is drawn in the colour of the site's own favicon, and in a neutral colour where the favicon has none to give. Pinned-tab icons are larger than ordinary-tab icons.
- Browser chrome does not occupy a toolbar above the webpage. The webpage uses the full height beside the sidebar. While the sidebar is hidden, the navigation controls, the sidebar toggle and the command-panel trigger float over the bottom-left of the page instead of taking a band from it.
- `Primary+B` hides the sidebar entirely. In that chromeless state the page keeps the whole window, the floating controls appear, and a stripe in the Space colour along the window's leading edge keeps the browsing identity visible.
- The sidebar's width belongs to the reader. The seam between it and the page drags, and `Primary+Shift+]` and `Primary+Shift+[` move that same seam from the keyboard, so a resize never depends on a pointer. The width is clamped so a tab row stays readable and the page keeps at least half the window; `Primary+Shift+B` returns it to the default. It survives a restart. A Private window neither reads nor records it.
- `Primary+E` moves the keyboard into the outline, landing on the row the reader is already reading, and `Escape` or `Primary+Shift+E` hands it back to the page. Focusing a hidden outline shows it first.
- Security state and the blocked-request count ride inline in the address trigger, and open a status panel.
- Clicking the sidebar's current-address trigger or pressing `Primary+L` opens a centered Omnibar for the current tab.
- `Primary+T` opens the Omnibar with a new-tab intent. Tanto creates the tab only after the user commits a destination.
- The Omnibar searches addresses, the active Space's local history, open tabs, Spaces, and browser commands. Remote search suggestions are off by default.
- `Primary+K` opens the same panel in command mode: every action Tanto can perform is fuzzy-searchable there, and each result shows the keys that invoke it, so the panel is also how the keymap is learned. An action that cannot be reached from the panel is a defect.
- Themes reload at runtime from versioned JSON. A theme defines type as well as colour: font families, sizes and label spacing, and the tinting of tab tiles. Semantic opacity values control transparent surfaces. Native blur falls back to alpha transparency and then an opaque color.
- Private windows must remain visually distinct. Reduced-motion, increased-contrast, and reduced-transparency system settings override themes.

## Keyboard navigation

Keyboard navigation is a built-in setting, not a Web extension. Native commands control browser chrome. An engine adapter injects a small page script for scrolling, link hints, and page commands.

Every binding lives in one versioned JSON file, so rebinding, sharing, or backing up a keymap is editing or copying that file. It holds two maps: `bindings` for page commands the injected script performs, and `browser` for commands Tanto performs itself. Chords stay live at all times; single-key browser commands follow the Keyboard navigation setting, because only they can be confused with typing on a page. Editable controls receive typing regardless, and `Escape` leaves a focused field so the bindings come back, except where the site has asked to keep the key.

The feature has no persistent Normal or Insert state. Sites may bypass selected conflicting keys or all page-level keys.

The default browser commands include:

- `H` and `L` for history, `r` to reload, `o` to open an address.
- `gt` and `gT` to move between tabs, `1`–`9` to jump to one, `x` to close, `u` to reopen, `t` for a new tab, `p` to pin.
- `gs` for the next Space and `Primary+1`–`Primary+9` for a specific one.
- `Primary+B` to hide the sidebar, `Primary+E` to focus it, `Primary+,` for settings, and `Primary+K` or `:` for the command panel.

The default page commands include:

- `f` labels click targets and activates the selected target in the current tab.
- `Shift+F` labels the same targets and opens the selected target in a background tab.
- Vim-style scrolling commands such as `j`, `k`, `gg`, and `G` when the site does not own those keys.

## Privacy and security

- No telemetry, advertising identifier, browser account, cloud sync, Google push service, or automatic crash upload.
- Account, Sync, and Diagnostics are optional first-party Feature modules.
- Every automatic network request is documented.
- Engine sandboxes remain enabled. Scripts reject sandbox-disabling and single-process flags.
- The Ladybird variant is experimental and unsuitable for sensitive browsing while Ladybird remains pre-alpha.
- Site permissions belong to an origin within one Space and support allow once, persistent allow, and block.
- Public builds do not enable proprietary media codecs until distribution rights receive review.

## Content blocking

Content blocking is built in and engine-neutral. EasyList and EasyPrivacy are subscribed and enabled on a first run, and either can be disabled or removed. It supports subscribed lists, network and plain CSS cosmetic rules, exceptions, common resource types, first-party and third-party matching, domain restrictions, entity domains, automatic updates, per-site disabling, user lists, and a visible blocked-request count.

Hiding rules written against a page's own hostname are in the document before the page's markup renders, so a hidden element never appears first. Rules written against no particular site are matched against the classes and ids the page actually carries rather than sent in full, and a site with a `$generichide` exception is not matched against them at all.

Tanto does not claim full uBlock Origin compatibility. Scriptlets, procedural selectors, response rewriting, HTML filtering, dynamic rules, popup blocking, CNAME uncloaking, redirects, and resource replacement are outside the first contract. A subscribed list keeps the rules this contract does parse; Settings reports what each list contributed and what it skipped.

## Non-goals for the first milestone

- Bookmarks
- Integrated terminal
- Third-party WebExtensions
- Account or Sync
- Diagnostics or agent access
- Password management
- Mac App Store distribution
- AppImage or Flatpak packaging
