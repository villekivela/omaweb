# Omaweb product requirements

## Product

Omaweb is a keyboard-driven browser. Linux with first-class Wayland support is its distribution
platform. The Qt build may run on macOS for development and testing, but Omaweb does not distribute
macOS builds. The main application window is frameless, uses vertical tabs, and has transparent
browser-owned surfaces.

QtWebEngine is the Development engine. Ladybird is the Target engine. They ship as separate
application build variants and share Omaweb's browser model, interface, settings, and
engine-contract tests. The Qt build may satisfy a capability before Ladybird; the Ladybird build
reports the gap and remains experimental rather than imitating behavior it cannot provide.

## Required browsing model

- One persistent ordinary main window with no title bar or visible window controls. Omaweb does not
  detach tabs or create additional ordinary browser windows.
- Any number of named Spaces. Each Space isolates logins, cookies, site data, permissions, history,
  sessions, ordinary tabs, and pinned tabs.
- Only the active Space keeps live pages by default. A Pinned tab with Keep active enabled continues
  running while its Space is inactive, as does a tab while Developer tools remain attached. Omaweb
  identifies every retained tab and its resource cost.
- Private browsing uses separate frameless windows and one temporary identity shared until the last
  Private window closes.
- Site-requested Auxiliary windows are allowed for authentication, payment, and similar flows. A
  page's other new-window requests open a Glance over the page on show, or a tab where the reader
  has turned the Glance off; a background-tab request, from a middle click or the background link
  hint, opens a tab behind either way.
- Pinned tabs belong to one Space and restore with it. Bookmarks are not part of Omaweb.
- Downloads are application-wide. Private downloads remain on disk but do not enter persistent
  download history.
- Zoom and mute belong to a tab, survive navigation and restart, and never become origin-wide
  preferences. New tabs start at 100 percent and unmuted.
- A split shows two ordinary tabs of the Space on show side by side, listed as one sidebar row of
  two halves, with a draggable divider that starts in the middle and is remembered only while the
  window lives. The focused pane's tab is the active tab: the address field, find bar, notices,
  prompt bars, loading indicator, zoom, lock and navigation follow it, and the tab beside shows its
  page and nothing else until it is focused. Both panes run; a split of an away Space freezes like
  its other pages. Focus moves by a press in the other pane, by `Primary+;`, or by the key that
  moves the keyboard to the region on that side. A split is entered from another row's Add split
  view, which pairs it with the active tab, or from the active row's, which puts a blank tab beside
  it and focuses it; the command panel's `add-split` offers a chooser of the unpaired ordinary tabs,
  headed by a blank tab. Selecting any other tab shows it alone and leaves the row in place; either
  half brings the split back, and tab cycling treats the row as one stop entered on its last-focused
  half. Separate split view puts two adjacent ordinary rows back. Closing either tab, or moving one
  to another Space, ends the split. Pinned tabs are never paired, a tab is in at most one split, and
  a split's tab is separated before it can be pinned or moved. The pairing is kept with the Space's
  tabs and restored after a restart, and is not part of the Sync projection.
- Each Space retains its 25 most recently closed tabs across restart. Reopening restores address,
  title, pin state, zoom, and mute in reverse closing order. A Private session keeps the same stack
  only in memory.
- Startup restores the last active Space, tabs, Pinned tabs, retained-tab settings, sidebar state,
  zoom, and mute. A command returns the active Space to rest. Private windows never restore.

## Interface

- The vertical sidebar shows the active Space. From top to bottom, it contains the navigation row,
  current-address trigger, pinned tabs, ordinary tabs, and footer. Pinned tabs use icon-only buttons
  in full-width rows with capacity for three to five tabs. Incomplete rows divide their width among
  their tabs. Ordinary tabs use single-line rows. The navigation row puts the sidebar and
  command-panel buttons first, followed by back, forward, and reload. The footer contains the Space
  letters, Spaces menu, active Download mark, and settings. Private windows replace the Space
  letters with a mask. New tabs remain available through the Omnibar and keyboard commands rather
  than a sidebar button.
- A tab with no address to load shows the Start page in place of a webpage, never an empty viewport:
  the browser commands and the keys that run them, read from the live keymap so the sheet cannot
  promise a key the window does not answer. That covers a Space at rest and an `about:blank` the
  reader navigated to. Standing in for a page, the Start page takes the sidebar's fill and
  translucency rather than the opaque backing a webpage needs, and no engine is spent behind it.
- A Space at rest, one whose only ordinary tab is blank, also lists no ordinary tab row. Any other
  blank tab is a tab in its own right and keeps its row, its close button, and the engine a page's
  new-window request was handed to.
- A Glance is one accent-bordered panel over the page area, the page blurred under the sheet tint
  around it, grown out of the link that asked for it and retreating into it when it closes. Its head
  names its page and address, offers Open as tab and close, and says that `Escape` closes it. It
  takes the opener's engine profile, so logins hold, and its visits enter the Space's history. It is
  not a tab: the outline does not list it, the session store and the Sync projection do not carry
  it, and the address trigger, find bar, prompt bars and page commands keep answering for the tab
  beneath. It ends with `Escape`, its close button, a click outside its panel, the close-tab
  command, and anything that changes the tab on show: a tab or Space switch, the tab closing,
  settings or history opening. `Primary+Shift+Return` or the head's button makes it an ordinary tab
  after the tab it stood over, keeping its engine and so its history, scroll and form state. A
  new-tab request from inside a Glance opens a tab, which ends the Glance; a second request from the
  page beneath replaces it. Settings' interface section turns the Glance off, locally.
- The same sheet answers the Keyboard shortcuts command on demand, from the command panel or
  `Primary+/` and `?`. Summoned over a live page it closes with `Escape` or its close button; asking
  for it while it already stands in for the page does nothing.
- An Omaweb surface that takes the whole page area, the Start page summoned over a page and the
  settings page, blurs the page beneath it rather than sealing it off, so the reader can still see
  the place they left without being asked to read a webpage through it. It takes the sidebar's
  colour but its own semantic opacity: the sidebar is read against the desktop, and a sheet is read
  against a page whose contrast is unknown, so at the sidebar's value a dark page shows through as
  nothing. Where there is no page to blur, as in a Space at rest, the surface takes the sidebar's
  translucency instead, and the desktop behind it is left to the window system, which blurs it or
  not exactly as it does behind the sidebar.
- Tabs can show site favicons or a two-character host code. The reader can turn favicons off and can
  choose whether favicon artwork is recoloured to the host-derived tint. With favicons off, the host
  code is drawn in the colour of the site's own favicon, and in a neutral colour where the favicon
  has none to give. Pinned-tab icons are larger than ordinary-tab icons.
- Browser chrome does not occupy a toolbar above the webpage. The webpage uses the full height
  beside the sidebar. While the sidebar is hidden, the navigation controls, the sidebar toggle and
  the command-panel trigger float over the bottom-left of the page instead of taking a band from it.
- `Primary+B` hides the sidebar entirely. In that chromeless state the page keeps the whole window,
  the floating controls appear, and a stripe in the Space colour along the window's leading edge
  keeps the browsing identity visible. Hiding or showing the sidebar eases the seam and the page
  travels with it, keeping one width for the whole movement: the page lays out once rather than at
  every width the seam crosses, and the sidebar's rows keep their own width as it goes.
- The floating controls and the eased seam are defaults rather than fixtures. Settings' interface
  section turns each off: without the floating controls a hidden sidebar leaves the page the whole
  window, and without the ease the seam and the page arrive at their settled widths in one step.
  Both survive a restart, and the keys that hide and show the sidebar work the same either way.
- The size Omaweb's own type is drawn at is the theme's until the reader sets it. Settings'
  interface section steps it up and down a pixel at a time within a supported range and resets it,
  the change reaches every Omaweb surface at once, Private windows included, and it survives a
  restart. Reset removes the override, so a theme switch changes the size only while no override
  stands. A page and its zoom are not changed by it. The control names its value, its default and
  its actions to accessibility tools.
- A page's fonts are the engine's own until the reader names one. The same section's page fonts
  group offers a standard family, a fixed-width family, a default size and a minimum size, each
  showing the engine's own value while unset and returning to it on reset. Families are chosen from
  the fonts the host has, the fixed-width slot offers the interface's own family beside them, and a
  chosen family the host has since lost reads as the engine's again. A page that names no family is
  drawn in the chosen one; a page that names a size under the minimum is drawn at the minimum, and a
  tab's zoom multiplies that. The values apply to every Space and Private window, reach open pages
  without a reload, and survive a restart. A page's own declarations are never overridden. The
  controls name their value, their default and their actions to accessibility tools.
- The sidebar's width belongs to the reader. The seam between it and the page drags, and
  `Primary+Shift+]` and `Primary+Shift+[` move that same seam from the keyboard, so a resize never
  depends on a pointer. The width is clamped so a tab row stays readable and the page keeps at least
  half the window; `Primary+Shift+B` returns it to the default. It survives a restart. A Private
  window neither reads nor records it.
- `Primary+E` moves the keyboard into the outline, landing on the row the reader is already reading,
  and `Escape` or `Primary+Shift+E` hands it back to the page. Focusing a hidden outline shows it
  first.
- `Alt+H`, `Alt+J`, `Alt+K` and `Alt+L` move the keyboard left, down, up and right between the
  regions on screen: the outline, the page area, which is a pane at a time while a split is on show,
  and the Developer tools dock. A region that is hidden is skipped rather than shown, and a move
  with nothing in that direction leaves the keyboard where it is.
- In Settings, an unhandled letter selects the next section whose name begins with it and moves the
  keyboard onto that name in the rail. Repeated presses cycle through matching sections. A field
  keeps the letters typed into it.
- Security state and the blocked-request count ride inline in the address trigger, and open the
  site-information panel.
- Clicking the sidebar's current-address trigger or pressing `Primary+L` opens a centered Omnibar
  for the current tab.
- `Primary+T` opens the Omnibar with a new-tab intent. Omaweb creates the tab only after the user
  commits a destination.
- The Omnibar searches addresses, the active Space's local history, open tabs, Spaces, and browser
  commands. Remote search suggestions are off by default.
- `Primary+K` opens the same panel in command mode: every action Omaweb can perform is
  fuzzy-searchable there, and each result shows the keys that invoke it, so the panel is also how
  the keymap is learned. An action that cannot be reached from the panel is a defect.
- Target-specific page actions are the exception to the command-panel rule. An Omaweb-owned page
  context menu opens by pointer or `Shift+F10`; the command panel exposes Open page context menu,
  while actions such as copy link, save image, and Inspect element remain inside the menu because
  they require its target.
- Tabs reorder by pointer and keyboard. Pinned tabs stay within the Pinned section and ordinary tabs
  within the ordinary section. Duplicate tab opens the current address in a new ordinary tab without
  copying navigation history, form state, or a live page. The ordinary-tab context menu can close
  the other ordinary tabs or the ordinary tabs below it and never includes Pinned tabs.
- Themes reload at runtime from versioned JSON. A theme defines type as well as colour: font
  families, sizes and label spacing, and the tinting of tab tiles. Semantic opacity values control
  transparent surfaces, which fall back to an opaque color where accessibility settings require it.
  Blurring the desktop behind them is the window system's and is not required for them to read.
- Quiet text is content, not decoration: a tab's title, a Space's letters, the footer's controls.
  Whatever a theme names for it, Omaweb holds it to WCAG AA against every ordinary and Private
  surface it is drawn on, and therefore clear of the disabled rendering of ordinary text. A reader
  must never have to guess whether something is merely quiet or actually unavailable. Borders clear
  the WCAG AA non-text threshold against every surface they separate. Contrast repair keeps a
  colour's theme-supplied hue. Where a palette's own surfaces put a threshold out of reach, the
  colour that reads best on the worst of them is used and the rest of the theme is kept: the browser
  always follows the desktop's colours.
- Private windows must remain visually distinct. Reduced-motion, increased-contrast, and
  reduced-transparency system settings override themes.
- A page that asks follows the theme. A document carrying `<meta name="omaweb-palette">` in its head
  is given the window's palette as `--omaweb-bg`, `--omaweb-sidebar`, `--omaweb-fg`,
  `--omaweb-accent`, `--omaweb-urgent` and `--omaweb-muted` on its root element, before it paints
  and again when the theme changes, and at no specificity, so the page's own definitions win. A page
  that does not ask is given nothing: which theme a reader runs is a fact about them, and a page
  learns it only by asking where the reader can see the asking. The website asks.
- A page is painted on the canvas every browser gives it: white, or the dark canvas the engine draws
  under a page that declares `color-scheme: dark`. A page that sets no background of its own is
  white under a dark theme, as its author saw it. The theme's colour is shown only where no page has
  painted yet, from the moment a document is created until its first paint, so a navigation under a
  dark theme never flashes a bright rectangle through the chrome, and the page on show keeps its
  canvas while the next document is fetched.

## Daily browser operations

- Find belongs to one tab. Hiding the find interface retains its query and current match for that
  tab; navigation clears the matches and keeps the query ready to run again.
- Zoom belongs to one tab and supports increase, decrease, and reset. Normal reload respects cache,
  Reload bypassing cache does not, and Stop loading ends the current load without clearing page
  state.
- Browser fullscreen and site-requested fullscreen are separate. Site-requested fullscreen begins
  with a visible origin notice and always exits with `Escape`.
- Printing uses the native print dialog and includes the operating system's PDF destination. Where
  an engine provides a sandboxed PDF viewer, Omaweb opens PDFs inline with search, zoom, print, and
  download; another engine downloads the document and reports the missing capability.
- Open file uses the native file picker for an explicitly selected HTML, text, image, or PDF file.
  Local pages receive no broad filesystem access; applications that need several local resources use
  a local server.
- File selection and Save link as use native dialogs. Omaweb owns tab-modal JavaScript and
  HTTP-authentication prompts. Repeated JavaScript prompts can be stopped for the page, and HTTP
  credentials last only for the engine session.
- External-protocol requests name the application, scheme, origin, and full destination before
  leaving Omaweb. An allow decision may be remembered for one origin and scheme within one Space;
  Private windows never remember it.
- Audio indicators and mute controls remain visible for audible tabs. Muted autoplay is allowed,
  while audible autoplay waits for interaction with the origin. A retained Pinned tab may continue
  playback while its Space is inactive.
- The Sounding tab is announced to the desktop as one media player for the browser. It carries what
  the page declares about what is playing, falling back to the tab's title, and answers the play,
  pause, next, previous, and stop the desktop sends by invoking the page's own handlers. A paused
  page keeps the player and yields it to a tab that is still playing; a page that stops playing
  withdraws it. A Private window announces playback and the controls, and nothing that says what is
  playing.
- Video decodes on the GPU where the host has a working VA-API driver, and in software where it has
  none. A missing driver is not a refusal to start, and the driver packages are `optdepends` rather
  than dependencies because which one a host needs depends on its GPU.
- Omaweb supplies no spellchecker, translation, Reader mode, page-screenshot command, View source
  command, or installed-web-application model.

## Browser-owned pages

- History is a browser-owned full-page sheet using no web engine. Over a live page it blurs that
  page; in a Space at rest it uses the native window backdrop. It searches only the active Space and
  deletes one visit, one origin, a time range, or the entire Space history. Private windows record
  none.
- Settings clears selected cookies, storage, cache, permissions, and history for one Space and time
  range by default. Clearing every Space is a separate explicit choice. Deleting a Space removes all
  of its browser-managed data after confirmation.
- A configurable local search-engine list stores a name, query URL, and optional keyword. Omaweb
  ships DuckDuckGo (`d`), Google (`g`), Bing (`b`), Brave Search (`br`), Kagi (`k`), Ecosia (`e`)
  and Startpage (`sp`) configured, with DuckDuckGo as the default. A list saved before they shipped
  is given the ones it lacks once, and an engine deleted afterwards stays deleted. Remote
  suggestions remain off.
- A keyword is matched case-insensitively and stored lowercased, so two keywords that differ only in
  case cannot both be saved. The Omnibar names the engine a typed keyword selects while it is typed:
  the space after the keyword moves the engine into a chip ahead of the terms, Backspace on empty
  terms puts the keyword back as text, and the destination row reads "Search <engine> for <terms>",
  or the default engine for text with no keyword. The keyword alone opens the engine's front page.
  While the text could still become a keyword, the Omnibar offers each matching engine other than
  the default as a row beneath the history rows.
- Bare public hosts try HTTPS first and offer an explicit insecure-HTTP retry after failure. Bare
  localhost addresses, IP literals, explicit ports, and reserved `.test` and `.localhost` names
  resolve as local addresses rather than searches. Explicit schemes remain unchanged.

## Developer tools

- Open developer tools attaches the current engine's inspector to one tab. The Qt adapter docks the
  bundled Chromium DevTools frontend on the right of the tab through `WebEngineView.devToolsView`; a
  draggable width survives restart. An engine without an inspector reports the command unavailable.
- One Developer-tools view may inspect a tab. It survives navigation and Space switches without
  changing tabs, hides while another tab is visible, and returns with the inspected tab. Closing the
  tab, moving it to another Space, or closing Developer tools detaches it. Developer tools never
  restore after restart.
- Inspect element in the page context menu opens Developer tools and selects its target, and
  `Primary+Alt+C` does the same from the keyboard. Private tabs may be inspected, and their
  Developer-tools storage expires with the Private session.
- Developer tools are drawn in the active theme: the interface, its type, and the colours source,
  markup, and stylesheets are read in. Markup structure, its brackets, separators and quotes, is
  drawn quieter than the names between it, as an editor draws them. The theme names the syntax
  colours; the rest of the inspector follows the same palette. An inspector Omaweb cannot colour is
  left in the engine's own palette rather than approximated.
- Remote debugging is available only through the explicit `--remote-debugging[=port]` launch option,
  bound to loopback. Omaweb prints its address and a warning, never enables it during an ordinary
  release session, and disables Private windows for that launch.

## Page context menu

- Right-clicking a page opens an Omaweb-drawn menu in the active theme. The engine reports what was
  under the pointer as plain values, a position, the addresses under it, the selection and whether
  the target takes typing, and draws no menu of its own.
- The menu offers what Omaweb can do with what was pointed at: a link opens in a new or background
  tab and its address copies; an image or media address opens and copies; a selection copies.
  Navigation, the page address, and Inspect element are always listed.
- A command that cannot run here is listed and unavailable rather than hidden, and the keyboard
  passes over it. Arrow keys move, `Return` runs, `Escape` closes and hands the keyboard back to the
  page.
- The menu does not offer Save page or View page source. Developer tools already show a page's
  source, and a file worth keeping is a download.

## Keyboard navigation

Keyboard navigation is a built-in setting, not a Web extension. Native commands control browser
chrome. An engine adapter injects a small page script for scrolling, link hints, and page commands.

Every binding lives in one versioned JSON file, so rebinding, sharing, or backing up a keymap is
editing or copying that file. It holds two maps: `bindings` for page commands the injected script
performs, and `browser` for commands Omaweb performs itself. Chords stay live at all times;
single-key browser commands follow the Keyboard navigation setting, because only they can be
confused with typing on a page. Editable controls receive typing regardless, and `Escape` leaves a
focused field so the bindings come back, except where the site has asked to keep the key.

The feature has no persistent Normal or Insert state. Sites may bypass selected conflicting keys or
all page-level keys.

The default browser commands include:

- `H` and `L` for history, `r` to reload, `o` to open an address.
- `gt` and `gT` to move between tabs, `1`–`9` to jump to one, `x` to close, `u` to reopen, `t` for a
  new tab, `p` to pin.
- `gs` for the next Space and `Primary+1`–`Primary+9` for a specific one.
- `Primary+B` to hide the sidebar, `Primary+E` to focus it, `Primary+,` for settings, and
  `Primary+K` or `:` for the command panel.
- `Alt+H`, `Alt+J`, `Alt+K` and `Alt+L` to move the keyboard between the regions on screen.
- `Primary+Shift+I` for Developer tools, `Primary+Alt+C` to inspect an element, and
  `Primary+Shift+C` to copy the address of the page on show.
- `Primary+F` or `/` to find in the page, `Primary+G` or `n` for the next match and
  `Primary+Shift+G` or `N` for the previous one.
- `Primary+=`, `Primary+-` and `Primary+0` to zoom the tab in, out and back to 100 percent.
- `Primary+Shift+R` or `R` to reload bypassing cache, `Primary+.` to stop loading, `Primary+P` to
  print, and `Primary+Shift+F` for fullscreen.

The default page commands include:

- `f` labels click targets and activates the selected target in the current tab.
- `Shift+F` labels the same targets and opens the selected target in a background tab.
- Vim-style scrolling commands such as `j`, `k`, `gg`, and `G` when the site does not own those
  keys. Presses add to one glide rather than queueing separate animations, so a held key is one
  continuous run. A jump to either end covers the same last stretch whatever the page's length. A
  scroll from anywhere else, a wheel or the page itself, ends the glide where it landed, and
  `prefers-reduced-motion` moves the page in one step.

## Privacy and security

- No telemetry, advertising identifier, browser account, hosted Omaweb service, Google push service,
  or automatic crash upload.
- Sync is an optional Linux Feature module. Its forge login is an identity, not an Omaweb Account,
  and a daily-driver build remains local-only until the reader connects it.
- Every automatic network request is documented.
- Every build refuses to start when its command line or `QTWEBENGINE_CHROMIUM_FLAGS` disables the
  renderer sandbox. On Linux, a failed sandbox prerequisite stops startup with a diagnostic that
  identifies the setting and required value. Omaweb reports renderer isolation only after verifying
  it. It does not describe QtWebEngine's in-process network service as sandboxed. Flags Omaweb adds
  to the engine command line itself are audited by the same rule as flags from its command line and
  from `QTWEBENGINE_CHROMIUM_FLAGS`.
- The Ladybird variant is experimental and unsuitable for sensitive browsing while Ladybird remains
  pre-alpha.
- Site permissions belong to an origin within one Space and support allow once, persistent allow,
  and block.
- Camera, microphone, geolocation, and notifications use the three Site-permission decisions.
  Clipboard read and screen sharing require approval each time. USB, Bluetooth, serial, and MIDI
  remain outside the first daily-driver contract.
- Native notifications name both origin and Space; activating one switches to its tab. Only a Pinned
  tab with Keep active enabled can generate a notification while its Space is inactive. A Private
  window raises none: a desktop notification records the origin in a list that outlives the private
  session and is read by whoever is at the machine.
- Third-party cookies are blocked by default. Authentication and payment flows may receive a
  temporary origin-specific allowance visible and revocable in the site-information panel.
- Global Privacy Control is on by default and browser-wide. While it is on, every request from every
  Engine profile carries `Sec-GPC: 1`, subresources and the engine's own requests on a page's behalf
  included, and `navigator.globalPrivacyControl` reads `true` in every frame. Spaces and Private
  windows send it alike. The privacy section of Settings shows the setting and turns it off, which
  turns off both the header and the property, and the choice survives a restart. Omaweb sends no Do
  Not Track header and offers no per-site exception.
- A page's WebRTC calls are offered the public interface only, on by default and browser-wide. Every
  Engine profile, a Space's and the Private windows' shared one, carries the engine's
  `WebRTCPublicInterfacesOnly` policy, so a page gathering candidates reads the address of the
  default route and not the reader's other interfaces, a VPN's hidden one included. A call still
  connects through that route or a TURN relay. The privacy section of Settings shows the setting and
  turns it off for a reader whose peer is on the same network, which reaches the open profiles
  without a restart, and the choice survives one. Site information does not report it: it is a
  browser policy, not a page's state. A build running on a Qt it was not compiled against cannot set
  the policy, leaves the engine's default of every interface, and says so in the setting's place.
  Omaweb offers no per-site exception and cannot disable WebRTC outright.
- The address trigger reports secure connection, insecure connection, or certificate error only from
  facts the adapter can prove. The site-information panel shows origin, connection state,
  Space-specific permissions, blocked-request count, stored-data size, and confirmed actions to
  clear site data or reset permissions.
- Certificate failures block by default. A Local-development site's main frame may receive a
  one-time exception only when the engine marks the failure overridable. Subresource, fatal,
  public-site, and remembered exceptions are refused, and the address trigger keeps the exception
  visible. Omaweb writes no exception down, so a later session asks again; an engine that holds an
  accepted certificate for the rest of its profile's life and offers no way back is why the trigger
  keeps reporting the error rather than trusting the engine to report it.
- Active mixed content remains blocked. Omaweb never enables an engine-wide insecure-content
  override.
- URL reputation is not part of the daily-driver contract. Omaweb documents that it does not provide
  phishing, malware, or download-reputation verdicts and does not present Content blocking as
  equivalent protection. A future provider integration must first resolve licensing, credentials,
  update format, privacy, false positives, bypass behavior, and offline operation.
- Site data is cleared at whichever scope the engine can reach, and the panel names which that is:
  cookies and cache for a whole Space, and one origin's storage, databases, caches and service
  workers emptied from inside its own page.
- Executables, scripts, installers, disk images, and common archives require confirmation before
  download. The prompt identifies the file type. Omaweb never opens a download, removes its execute
  permissions, and records its source address in operating-system metadata. Automatic downloads and
  a site's second concurrent download require a revocable Site permission.
- Ordinary downloads start in the configured directory. This directory is global reader
  configuration, not Space data. Every window uses it, and Private windows cannot change it. The
  downloads list shows progress and the actions available for each item. A reader can cancel an
  active download, retry an interrupted one, reveal a finished file, or remove its history record
  without deleting the file.
- The Download mark in the sidebar footer shows the number and combined progress of active
  downloads. Pointer and keyboard focus reveal progress by file, and activating the mark opens the
  downloads section. Files without a declared length show an unknown size instead of a percentage.
  The mark uses the window's live downloads, so it works in Private windows. It remains in a
  finished state until the saved-file notice closes. Chromeless windows have notices but no sidebar
  footer or Download mark.
- Save link as, and a download whose name is already taken, use the native save dialog.
- CI checks the approved QtWebEngine and Chromium security baseline weekly. Settings reads the same
  baseline file. A Qt patch with security fixes requires an Omaweb update within seven days.
  Settings marks builds below the baseline as unsupported previews.
- Public builds do not enable proprietary media codecs until distribution rights receive review.

## Distribution

- The first daily-driver package is a native Arch package for Linux and Wayland. Linux
  default-browser integration registers HTTP and HTTPS handling and changes the default browser only
  through an explicit command.
- Release and update delivery belongs to the Linux package. Omaweb does not ship an
  application-level updater.
- macOS application bundles are development and test artifacts. Omaweb does not sign, notarize,
  publish, or qualify them for end-user distribution.
- Open-codec audio and video, camera, microphone, and screen sharing belong to the daily-driver
  contract. DRM streaming and proprietary codecs remain unsupported until their distribution rights
  are resolved.
- Omaweb follows operating-system proxy settings and trust stores. It may report detected state and
  document launch-time development overrides, but it stores no proxy credentials and owns no
  certificate-authority database.

## Content blocking

Content blocking is built in and engine-neutral. EasyList and EasyPrivacy are subscribed and enabled
on a first run, and either can be disabled or removed. It supports subscribed lists, network and
plain CSS cosmetic rules, exceptions, common resource types, first-party and third-party matching,
domain restrictions, entity domains, automatic updates, per-site disabling, user lists, and a
visible blocked-request count.

The first run is recorded, so a list a user removes stays removed across restarts. When there are no
subscriptions, Settings says so and offers the two default lists back.

EasyList Cookie, the easylist.to list that hides consent banners, is a Known list: Settings names it
with its source and license, and one action subscribes it. A first run does not subscribe it, so a
site's consent choice stays the reader's unless the reader asked the browser to take it. Subscribed,
it is a subscription like EasyList, with the same updates, per-site disabling, and Refusal tally,
and a reader who removes it is offered it again rather than given it. Fanboy's Annoyances,
anti-adblock lists, and regional lists are not offered.

A list's `$popup` rules decide which windows a page gets to open, and a window they refuse counts as
a blocked request. A middle- or ctrl-clicked link, which opens a background tab, is the user asking
rather than the page, and is never refused.

Hiding rules written against a page's own hostname are in the document before the page's markup
renders, so a hidden element never appears first. Rules written against no particular site are
matched against the classes and ids the page actually carries rather than sent in full, and a site
with a `$generichide` exception is not matched against them at all. Each frame of a page is surveyed
for those names as soon as its DOM is parsed and watched afterwards: an element added later that
carries a class or id the survey did not see is hidden within a fraction of a second, whether a
script filled an ad slot or a detection page inserted its bait. The watch asks only about names the
frame has not been asked about before, the first of them at once and the rest at most once per short
interval, and a document that keeps producing new names stops being asked about after a fixed number
of surveys. A frame that was not surveyed is not watched, and a navigation starts the new document's
survey and count from nothing.

A `##+js(...)` rule names a function from the vendored uBlock Origin scriptlet library and supplies
its arguments; a list never supplies code. The named function runs in the page before the page's own
scripts, because a check it neutralises has otherwise already run. A site the user turned blocking
off for runs none. The scriptlets uBlock Origin gates behind trust are refused outright, as are
names the bundled library does not carry, and a rule naming either is reported as unsupported rather
than counted among the rules the list contributed.

A `$redirect=` rule names a substitute resource from the vendored uBlock Origin library, and Omaweb
serves that body in place of the request rather than refusing it outright, so a page waiting on a
tracker finishes loading instead of stalling. A `$redirect-rule=` serves its substitute only once a
separate rule has refused the request. A rule naming a body the bundled library does not carry is
reported as unsupported rather than counted among the rules the list contributed. A `$removeparam`
rule refuses nothing: the request goes out with the tracking parameters the rule names stripped off
the address the site receives.

The element whose request was refused, or answered with a substitute, is taken out of the layout: an
image, frame, object, embed, video, or audio element whose address Content blocking refused reads
`display: none`, in the main frame and in every subframe, so a reader sees no broken-image icon
where an ad was and a page measuring its own bait reads it as hidden. An element whose request
failed for any other reason is left as the engine draws it, the collapse does not move the Refusal
tally, and a refused script or stylesheet has no element to collapse.

Omaweb does not claim full uBlock Origin compatibility. Procedural selectors, response rewriting,
content security policies, HTML filtering, dynamic rules, and CNAME uncloaking are outside the first
contract. A subscribed list keeps the rules this contract does parse; Settings reports what each
list contributed and what it skipped.

## Daily-driver non-goals

- Bookmarks
- Integrated terminal
- Web extensions other than a Known extension
- Browser Account system
- Password management by Omaweb itself
- Browser data import
- Installed web applications
- Additional ordinary browser windows or detached tabs
- Translation, Reader mode, browser-owned screenshots, View source, or spellchecking
- DRM streaming and proprietary media codecs before distribution review
- USB, Bluetooth, serial, or MIDI permissions
- macOS distribution
- AppImage or Flatpak packaging
