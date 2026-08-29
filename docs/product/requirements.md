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

- The vertical sidebar contains navigation controls, the current-address trigger, pinned tabs in a compact icon grid, the Space switcher and heading, ordinary tabs, security and permission status, content-blocking status, and the new-tab action.
- Browser chrome does not occupy a toolbar above the webpage. The webpage uses the full height beside the sidebar.
- Clicking the sidebar's current-address trigger or pressing `Primary+L` opens a centered Omnibar for the current tab.
- `Primary+T` opens the Omnibar with a new-tab intent. Tanto creates the tab only after the user commits a destination.
- The Omnibar searches addresses, the active Space's local history, open tabs, Spaces, and browser commands. Remote search suggestions are off by default.
- Themes reload at runtime from versioned JSON. Semantic opacity values control transparent surfaces. Native blur falls back to alpha transparency and then an opaque color.
- Private windows must remain visually distinct. Reduced-motion, increased-contrast, and reduced-transparency system settings override themes.

`Primary` means `Cmd` on macOS and `Ctrl` on Linux.

## Keyboard navigation

Keyboard navigation is a built-in setting, not a Web extension. Native commands control browser chrome. An engine adapter injects a small page script for scrolling, link hints, and page commands.

The feature has no persistent Normal or Insert state. Editable controls receive typing. Sites may bypass selected conflicting keys or all page-level keys. Bindings live in versioned JSON.

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

Content blocking is built in and engine-neutral. It supports subscribed lists, network and plain CSS cosmetic rules, exceptions, common resource types, first-party and third-party matching, domain restrictions, automatic updates, per-site disabling, user lists, and a visible blocked-request count.

Tanto does not claim full uBlock Origin compatibility. Scriptlets, procedural selectors, response rewriting, HTML filtering, dynamic rules, CNAME uncloaking, redirects, and resource replacement are outside the first contract.

## Non-goals for the first milestone

- Bookmarks
- Integrated terminal
- Third-party WebExtensions
- Account or Sync
- Diagnostics or agent access
- Password management
- Mac App Store distribution
- AppImage or Flatpak packaging
