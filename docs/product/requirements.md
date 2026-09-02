# Omaweb product requirements

## Product

Omaweb is a keyboard-driven browser for developers. The Qt build must become a macOS daily driver before the same browser contract moves to Linux with first-class Wayland support. The main application window is frameless, uses vertical tabs, and has transparent browser-owned surfaces.

QtWebEngine is the Development engine. Ladybird is the Target engine. They ship as separate application build variants and share Omaweb's browser model, interface, settings, and engine-contract tests. The Qt build may satisfy a capability before Ladybird; the Ladybird build reports the gap and remains experimental rather than imitating behavior it cannot provide.

## Required browsing model

- One persistent ordinary main window with no title bar or visible window controls. Omaweb does not detach tabs or create additional ordinary browser windows.
- Any number of named Spaces. Each Space isolates logins, cookies, site data, permissions, history, sessions, ordinary tabs, and pinned tabs.
- Only the active Space keeps live pages by default. A Pinned tab with Keep active enabled continues running while its Space is inactive, as does a tab while Developer tools remain attached. Omaweb identifies every retained tab and its resource cost.
- Private browsing uses separate frameless windows and one temporary identity shared until the last Private window closes.
- Site-requested Auxiliary windows are allowed for authentication, payment, and similar flows. Other new-window requests become tabs.
- Pinned tabs belong to one Space and restore with it. Bookmarks are not part of Omaweb.
- Downloads are application-wide. Private downloads remain on disk but do not enter persistent download history.
- Zoom and mute belong to a tab, survive navigation and restart, and never become origin-wide preferences. New tabs start at 100 percent and unmuted.
- Each Space retains its 25 most recently closed tabs across restart. Reopening restores address, title, pin state, zoom, and mute in reverse closing order. A Private session keeps the same stack only in memory.
- Startup restores the last active Space, tabs, Pinned tabs, retained-tab settings, sidebar state, zoom, and mute. A command returns the active Space to rest. Private windows never restore.

## Interface

- The vertical sidebar is an outline of the active Space, read top to bottom: the navigation row, the current-address trigger, pinned tabs as icon-only buttons in full-width rows with capacity for three to five tabs, and ordinary tabs as single-line rows. The navigation row carries the sidebar toggle and the command-panel trigger at its leading edge and back, forward and reload at its trailing edge. The footer closes the outline with the Space letters, the Spaces menu and settings, and a private window shows a mask there in place of the letters. Incomplete pinned rows divide their full width among the tabs they contain. New tabs remain available through the Omnibar and keyboard commands rather than a sidebar button.
- A tab with no address to load shows the Start page in place of a webpage, never an empty viewport: the browser commands and the keys that run them, read from the live keymap so the sheet cannot promise a key the window does not answer. That covers a Space at rest and an `about:blank` the reader navigated to. Standing in for a page, the Start page takes the sidebar's fill and translucency rather than the opaque backing a webpage needs, and no engine is spent behind it.
- A Space at rest — one whose only ordinary tab is blank — also lists no ordinary tab row. Any other blank tab is a tab in its own right and keeps its row, its close button, and the engine a page's new-window request was handed to.
- The same sheet answers the Keyboard shortcuts command on demand, from the command panel or `Primary+/` and `?`. Summoned over a live page it closes with `Escape` or its close button; asking for it while it already stands in for the page does nothing.
- A Omaweb surface that takes the whole page area — the Start page summoned over a page, and the settings page — blurs the page beneath it rather than sealing it off, so the reader can still see the place they left without being asked to read a webpage through it. It takes the sidebar's colour but its own semantic opacity: the sidebar is read against the desktop, and a sheet is read against a page whose contrast is unknown, so at the sidebar's value a dark page shows through as nothing. Where there is no page to blur — a Space at rest — the surface takes the sidebar's translucency instead, and the window's own native backdrop blurs the desktop behind it as it does behind the sidebar.
- Tabs can show site favicons or a two-character host code. The reader can turn favicons off and can choose whether favicon artwork is recoloured to the host-derived tint. With favicons off, the host code is drawn in the colour of the site's own favicon, and in a neutral colour where the favicon has none to give. Pinned-tab icons are larger than ordinary-tab icons.
- Browser chrome does not occupy a toolbar above the webpage. The webpage uses the full height beside the sidebar. While the sidebar is hidden, the navigation controls, the sidebar toggle and the command-panel trigger float over the bottom-left of the page instead of taking a band from it.
- `Primary+B` hides the sidebar entirely. In that chromeless state the page keeps the whole window, the floating controls appear, and a stripe in the Space colour along the window's leading edge keeps the browsing identity visible.
- The sidebar's width belongs to the reader. The seam between it and the page drags, and `Primary+Shift+]` and `Primary+Shift+[` move that same seam from the keyboard, so a resize never depends on a pointer. The width is clamped so a tab row stays readable and the page keeps at least half the window; `Primary+Shift+B` returns it to the default. It survives a restart. A Private window neither reads nor records it.
- `Primary+E` moves the keyboard into the outline, landing on the row the reader is already reading, and `Escape` or `Primary+Shift+E` hands it back to the page. Focusing a hidden outline shows it first.
- Security state and the blocked-request count ride inline in the address trigger, and open a status panel.
- Clicking the sidebar's current-address trigger or pressing `Primary+L` opens a centered Omnibar for the current tab.
- `Primary+T` opens the Omnibar with a new-tab intent. Omaweb creates the tab only after the user commits a destination.
- The Omnibar searches addresses, the active Space's local history, open tabs, Spaces, and browser commands. Remote search suggestions are off by default.
- `Primary+K` opens the same panel in command mode: every action Omaweb can perform is fuzzy-searchable there, and each result shows the keys that invoke it, so the panel is also how the keymap is learned. An action that cannot be reached from the panel is a defect.
- Target-specific page actions are the exception to the command-panel rule. A Omaweb-owned page context menu opens by pointer or `Shift+F10`; the command panel exposes Open page context menu, while actions such as copy link, save image, and Inspect element remain inside the menu because they require its target.
- Tabs reorder by pointer and keyboard. Pinned tabs stay within the Pinned section and ordinary tabs within the ordinary section. Duplicate tab opens the current address in a new ordinary tab without copying navigation history, form state, or a live page. The ordinary-tab context menu can close the other ordinary tabs or the ordinary tabs below it and never includes Pinned tabs.
- Themes reload at runtime from versioned JSON. A theme defines type as well as colour: font families, sizes and label spacing, and the tinting of tab tiles. Semantic opacity values control transparent surfaces. Native blur falls back to alpha transparency and then an opaque color.
- Private windows must remain visually distinct. Reduced-motion, increased-contrast, and reduced-transparency system settings override themes.

## Daily browser operations

- Find belongs to one tab. Hiding the find interface retains its query and current match for that tab; navigation clears the matches and keeps the query ready to run again.
- Zoom belongs to one tab and supports increase, decrease, and reset. Normal reload respects cache, Reload bypassing cache does not, and Stop loading ends the current load without clearing page state.
- Browser fullscreen and site-requested fullscreen are separate. Site-requested fullscreen begins with a visible origin notice and always exits with `Escape`.
- Printing uses the native print dialog and includes the operating system's PDF destination. Where an engine provides a sandboxed PDF viewer, Omaweb opens PDFs inline with search, zoom, print, and download; another engine downloads the document and reports the missing capability.
- Open file uses the native file picker for an explicitly selected HTML, text, image, or PDF file. Local pages receive no broad filesystem access; applications that need several local resources use a local server.
- File selection and Save link as use native dialogs. Omaweb owns tab-modal JavaScript and HTTP-authentication prompts. Repeated JavaScript prompts can be stopped for the page, and HTTP credentials last only for the engine session.
- External-protocol requests name the application, scheme, origin, and full destination before leaving Omaweb. An allow decision may be remembered for one origin and scheme within one Space; Private windows never remember it.
- Audio indicators and mute controls remain visible for audible tabs. Muted autoplay is allowed, while audible autoplay waits for interaction with the origin. A retained Pinned tab may continue playback while its Space is inactive.
- Omaweb supplies no spellchecker, translation, Reader mode, page-screenshot command, View source command, or installed-web-application model.

## Browser-owned pages

- History is a browser-owned full-page sheet using no web engine. Over a live page it blurs that page; in a Space at rest it uses the native window backdrop. It searches only the active Space and deletes one visit, one origin, a time range, or the entire Space history. Private windows record none.
- Settings clears selected cookies, storage, cache, permissions, and history for one Space and time range by default. Clearing every Space is a separate explicit choice. Deleting a Space removes all of its browser-managed data after confirmation.
- A configurable local search-engine list stores a name, query URL, and optional keyword. Omaweb ships DuckDuckGo as the initial default. Remote suggestions remain off.
- Bare public hosts try HTTPS first and offer an explicit insecure-HTTP retry after failure. Bare localhost addresses, IP literals, explicit ports, and reserved `.test` and `.localhost` names resolve as local addresses rather than searches. Explicit schemes remain unchanged.

## Developer tools

- Open developer tools attaches the current engine's inspector to one tab. The Qt adapter docks the bundled Chromium DevTools frontend on the right of the tab through `WebEngineView.devToolsView`; a draggable width survives restart. An engine without an inspector reports the command unavailable.
- One Developer-tools view may inspect a tab. It survives navigation and Space switches without changing tabs, hides while another tab is visible, and returns with the inspected tab. Closing the tab, moving it to another Space, or closing Developer tools detaches it. Developer tools never restore after restart.
- Inspect element in the page context menu opens Developer tools and selects its target, and `Primary+Alt+C` does the same from the keyboard. Private tabs may be inspected, and their Developer-tools storage expires with the Private session.
- Developer tools are drawn in the active theme: the interface, its type, and the colours source, markup, and stylesheets are read in. Markup structure — brackets, separators, quotes — is drawn quieter than the names between it, as an editor draws them. The theme names the syntax colours; the rest of the inspector follows the same palette. An inspector Omaweb cannot colour is left in the engine's own palette rather than approximated.
- Remote debugging is available only through the explicit `--remote-debugging[=port]` launch option, bound to loopback. Omaweb prints its address and a warning, never enables it during an ordinary release session, and disables Private windows for that launch.

## Page context menu

- Right-clicking a page opens a Omaweb-drawn menu in the active theme. The engine reports what was under the pointer as plain values — a position, the addresses under it, the selection, whether the target takes typing — and draws no menu of its own.
- The menu offers what Omaweb can do with what was pointed at: a link opens in a new or background tab and its address copies; an image or media address opens and copies; a selection copies. Navigation, the page address, and Inspect element are always listed.
- A command that cannot run here is listed and unavailable rather than hidden, and the keyboard passes over it. Arrow keys move, `Return` runs, `Escape` closes and hands the keyboard back to the page.
- The menu does not offer Save page or View page source. Developer tools already show a page's source, and a file worth keeping is a download.

## Keyboard navigation

Keyboard navigation is a built-in setting, not a Web extension. Native commands control browser chrome. An engine adapter injects a small page script for scrolling, link hints, and page commands.

Every binding lives in one versioned JSON file, so rebinding, sharing, or backing up a keymap is editing or copying that file. It holds two maps: `bindings` for page commands the injected script performs, and `browser` for commands Omaweb performs itself. Chords stay live at all times; single-key browser commands follow the Keyboard navigation setting, because only they can be confused with typing on a page. Editable controls receive typing regardless, and `Escape` leaves a focused field so the bindings come back, except where the site has asked to keep the key.

The feature has no persistent Normal or Insert state. Sites may bypass selected conflicting keys or all page-level keys.

The default browser commands include:

- `H` and `L` for history, `r` to reload, `o` to open an address.
- `gt` and `gT` to move between tabs, `1`–`9` to jump to one, `x` to close, `u` to reopen, `t` for a new tab, `p` to pin.
- `gs` for the next Space and `Primary+1`–`Primary+9` for a specific one.
- `Primary+B` to hide the sidebar, `Primary+E` to focus it, `Primary+,` for settings, and `Primary+K` or `:` for the command panel.
- `Primary+Shift+I` for Developer tools, `Primary+Alt+C` to inspect an element, and `Primary+Shift+C` to copy the address of the page on show.
- `Primary+F` or `/` to find in the page, `Primary+G` or `n` for the next match and `Primary+Shift+G` or `N` for the previous one.
- `Primary+=`, `Primary+-` and `Primary+0` to zoom the tab in, out and back to 100 percent.
- `Primary+Shift+R` or `R` to reload bypassing cache, `Primary+.` to stop loading, `Primary+P` to print, and `Primary+Shift+F` for fullscreen.

The default page commands include:

- `f` labels click targets and activates the selected target in the current tab.
- `Shift+F` labels the same targets and opens the selected target in a background tab.
- Vim-style scrolling commands such as `j`, `k`, `gg`, and `G` when the site does not own those keys.

## Privacy and security

- No telemetry, advertising identifier, browser account, cloud sync, Google push service, or automatic crash upload.
- Account and Sync are optional first-party Feature modules. A daily-driver build remains local-only without either.
- Every automatic network request is documented.
- Release builds refuse to start when the renderer sandbox is explicitly disabled. A Linux host that cannot meet its sandbox prerequisites receives a blocking diagnostic. Omaweb claims renderer isolation only where it can verify it and never describes QtWebEngine's in-process network service as sandboxed.
- The Ladybird variant is experimental and unsuitable for sensitive browsing while Ladybird remains pre-alpha.
- Site permissions belong to an origin within one Space and support allow once, persistent allow, and block.
- Camera, microphone, geolocation, and notifications use the three Site-permission decisions. Clipboard read and screen sharing require approval each time. USB, Bluetooth, serial, and MIDI remain outside the first daily-driver contract.
- Native notifications name both origin and Space; activating one switches to its tab. Only a Pinned tab with Keep active enabled can generate a notification while its Space is inactive. A Private window raises none: a desktop notification records the origin in a list that outlives the private session and is read by whoever is at the machine.
- Third-party cookies are blocked by default. Authentication and payment flows may receive a temporary origin-specific allowance visible and revocable in the site-information panel.
- The address trigger reports secure connection, insecure connection, or certificate error only from facts the adapter can prove. The site-information panel shows origin, connection state, Space-specific permissions, blocked-request count, stored-data size, and confirmed actions to clear site data or reset permissions.
- Certificate failures block by default. A Local-development site's main frame may receive a one-time exception only when the engine marks the failure overridable. Subresource, fatal, public-site, and remembered exceptions are refused, and the address trigger keeps the exception visible.
- Active mixed content remains blocked. Omaweb never enables an engine-wide insecure-content override.
- Public daily-driver builds require engine-neutral phishing and malware URL reputation. Provider selection must resolve licensing, update format, privacy, false positives, bypass behavior, and offline operation. An alpha without reputation protection labels that omission conspicuously.
- Executables, scripts, installers, disk images, and common archives require confirmation before download. Omaweb never opens them automatically and applies operating-system quarantine metadata. Automatic and multiple downloads require a Site permission.
- CI checks the approved QtWebEngine and Chromium security baseline weekly. A security-bearing Qt patch produces a Omaweb update within seven days; a build below the approved baseline remains an unsupported preview.
- Public builds do not enable proprietary media codecs until distribution rights receive review.

## Distribution

- The first daily-driver package is a signed and notarized macOS application. Omaweb registers HTTP and HTTPS handling and changes the default browser only through an explicit command.
- Omaweb checks a signed update manifest once per day and on manual request. Settings can disable automatic checks, every request is documented, and installation requires approval.
- Open-codec audio and video, camera, microphone, and screen sharing belong to the daily-driver contract. DRM streaming and proprietary codecs remain unsupported until their distribution rights are resolved.
- Omaweb follows operating-system proxy settings and trust stores. It may report detected state and document launch-time development overrides, but it stores no proxy credentials and owns no certificate-authority database.

## Content blocking

Content blocking is built in and engine-neutral. EasyList and EasyPrivacy are subscribed and enabled on a first run, and either can be disabled or removed. It supports subscribed lists, network and plain CSS cosmetic rules, exceptions, common resource types, first-party and third-party matching, domain restrictions, entity domains, automatic updates, per-site disabling, user lists, and a visible blocked-request count.

A list's `$popup` rules decide which windows a page gets to open, and a window they refuse counts as a blocked request. A middle- or ctrl-clicked link, which opens a background tab, is the user asking rather than the page, and is never refused.

Hiding rules written against a page's own hostname are in the document before the page's markup renders, so a hidden element never appears first. Rules written against no particular site are matched against the classes and ids the page actually carries rather than sent in full, and a site with a `$generichide` exception is not matched against them at all.

A `##+js(...)` rule names a function from the vendored uBlock Origin scriptlet library and supplies its arguments; a list never supplies code. The named function runs in the page before the page's own scripts, because a check it neutralises has otherwise already run. A site the user turned blocking off for runs none. The scriptlets uBlock Origin gates behind trust are refused outright, as are names the bundled library does not carry, and a rule naming either is reported as unsupported rather than counted among the rules the list contributed.

A `$redirect=` rule names a substitute resource from the vendored uBlock Origin library, and Omaweb serves that body in place of the request rather than refusing it outright, so a page waiting on a tracker finishes loading instead of stalling. A `$redirect-rule=` serves its substitute only once a separate rule has refused the request. A rule naming a body the bundled library does not carry is reported as unsupported rather than counted among the rules the list contributed. A `$removeparam` rule refuses nothing: the request goes out with the tracking parameters the rule names stripped off the address the site receives.

Omaweb does not claim full uBlock Origin compatibility. Procedural selectors, response rewriting, content security policies, HTML filtering, dynamic rules, and CNAME uncloaking are outside the first contract. A subscribed list keeps the rules this contract does parse; Settings reports what each list contributed and what it skipped.

## Daily-driver non-goals

- Bookmarks
- Integrated terminal
- Third-party WebExtensions
- Account or Sync
- Password management
- Browser data import
- Installed web applications
- Additional ordinary browser windows or detached tabs
- Translation, Reader mode, browser-owned screenshots, View source, or spellchecking
- DRM streaming and proprietary media codecs before distribution review
- USB, Bluetooth, serial, or MIDI permissions
- Mac App Store distribution
- AppImage or Flatpak packaging
