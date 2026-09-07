# Omaweb architecture

## Build variants

`omaweb-qt` is the default development and current daily-driver variant. `omaweb-ladybird` is
configured and built separately against a pinned Ladybird revision. Ladybird never enters the
default Qt configure or compile graph.

The variants share core state, persistence, QML, themes, platform code, and engine-contract tests.
Engine-specific headers stay inside adapter targets. Live tabs do not migrate between engines, and
each Space keeps separate engine-managed login state.

## Targets

- `omaweb-core` owns Spaces, tabs, sessions, commands, persistence policy, history, permissions,
  downloads, and errors.
- `omaweb-ui` contains shared QML and read-only UI-facing state, and drives the vendored kit's
  `qs.Commons` colour and type singletons from the theme palette.
- `omaweb-engine-api` defines navigation, lifecycle, input, content-blocking, accessibility, and
  Developer-tools capabilities.
- `omaweb-engine-qt` supplies the default engine view without leaking QtWebEngine imports into
  shared QML.
- `omaweb-engine-ladybird` is an optional build outside the default graph.
- `omaweb-quickshell-shim` registers the `Quickshell` and `Quickshell.Io` QML types the vendored
  Omarchy component kit imports, and shadows the real Quickshell an Omarchy host installs into Qt's
  qml directory.
- `omaweb-platform` contains the small window-system services the browser cannot supply itself:
  window integrations, the desktop's print dialog and notification centre, and what the operating
  system says one process holds.
- `modules/*` contains independently compiled first-party Feature modules.
- `omaweb-ui-lab` loads the shared QML against fake browser state and a mock engine.

QML renders read-only snapshots and submits commands. It does not mutate durable browser state
directly.

## Storage

One global SQLite database records application state and the Space list. Each Space owns a directory
containing its browser database and separate Qt and Ladybird engine-profile directories. Versioned
JSON stores settings, keybindings, and themes.

Structural state commits transactionally. High-frequency presentation state saves on a short
debounce. Engine caches and compiled blocker data are disposable and never enter Sync.

A Space keeps its 5,000 most recent visits. The bound is restored as visits arrive, once every 256
of them, so a session that never restarts stays inside the bound plus one batch rather than growing
until the Space database is next opened. The cleanup finds the oldest visit worth keeping through
the recency index and deletes below it, naming that visit by id as well as by timestamp because a
redirect chain lands several visits in one millisecond.

## History search

The Omnibar searches a Space's history on a thread of its own, through SQLite connections that
thread opens and closes: the query scans and groups every matching visit, which is work the
interface cannot do between keystrokes. One search runs and one waits, so typing faster than the
store answers replaces the waiting request instead of queueing another.

Each request carries a generation and the Space it was asked about. The core drops an answer whose
generation is behind, whose Space is no longer the active one, or whose history was deleted while it
ran, so a stale answer never replaces the rows the reader is looking at. A Private window has no
history to search and is answered with none.

## Content blocker

The Qt adapter calls a separately built `adblock-rust` shared library through a narrow C interface.
Its version matches the pinned Ladybird revision. Ladybird uses its internal copy. Both adapters run
the same conformance fixtures.

Request matching uses an immutable in-memory snapshot. Subscription updates compile off the request
path and atomically replace the active matcher.

## Tab lifecycle

Order, pinning, duplication and the sweeping closes are core commands: the row asks for a step or a
destination and the core decides what is reachable, because a tab moves within its own section and
only the core knows where that section ends. An arrangement the reader made is written through at
once rather than waiting in the coalescing window a page's own title reports use.

Each Space keeps its own most recent closes in its own database, newest first, bounded so the stack
does not become a second history. A record holds what the session holds about a tab, its address,
title, pinning, zoom and muting, and never the page: reopening loads the address again in a new tab.
A Private session keeps the same stack in memory and writes none of it down.

Putting a Space away stops its pages rather than taking them: every engine the Space holds is
frozen, and selecting the Space again finds each page where it was. What an away Space costs is a
renderer per tab the reader actually opened in it, because a tab of a restored Space that has never
been selected has no engine to keep. The core still names two tabs at the moment of suspension, a
Pinned tab the reader marked Keep active and the tab an inspector is attached to, but that names
which pages are identified, listed with their cost, and started again in a Space that has never been
selected, rather than which ones exist. The engine host is told which tabs those are, because the
answer is only knowable while that Space is still the active one. Retained tabs come back after the
visible Space rather than with it, because the reader is waiting for the page in front of them, and
a Pinned tab marked Keep active is started even in a Space that has never been selected, which is
what a restart, or a session that outlived a crash, has to restore. Every retained tab is listed in
settings with the Space it belongs to, why it is running, and the resident memory its renderer
actually holds, asked of the operating system rather than estimated.

Whether a page exists is separate from whether it runs. A page the reader cannot see is frozen: it
keeps its document, its process and everything it holds, and stops its timers, animations and
script, so selecting the tab continues the page rather than loading it again. The engine host
decides this for every engine it owns, from one rule, so a background tab of the visible Space and
every page of a Space that is away are answered the same way. Three hidden pages are exempt: the tab
an inspector is attached to, a tab making sound, and a Pinned tab marked Keep active, which is the
reader asking for a page that goes on running while they are elsewhere. The first two stop being
exempt as soon as the reason does, and the third when the setting is taken away.

A tab holding the camera, a microphone or the screen is not named in that rule and does not need to
be. The engine refuses to freeze a page that is capturing and says so by recommending Active for a
view that is hidden, and the Qt adapter applies the shell's decision against that recommendation
rather than commanding a state the engine may refuse. Qt reports capture as a permission a page
asked for and never as a state a page is in, so Omaweb could not write this exemption itself; it is
an engine fact the browser relies on, and `tests/engine/tst_qtenginecontract.cpp` pins it.

## Downloads

`DownloadPolicy` classifies a download from its proposed filename. It uses the declared media type
only when the filename is inconclusive. Both values are untrusted. A rule in `omaweb-core` combines
the file type, the reader's prior decision for the origin, and the destination directory. It returns
a named disposition such as accept, confirm, refuse, or save-as.

The engine requires a synchronous decision and cannot pause a download while a prompt is open. For a
Held download, the adapter cancels the request before it writes any bytes. If the reader accepts,
the page requests the file again. The policy runs again so confirmation does not also authorize
overwriting an existing file. A filename collision opens the native save dialog.

`omaweb-platform` records the source address in the finished file's metadata and removes execute
permissions. The download directory is global reader configuration, not Space data. All windows use
it, and Private windows cannot change it.

The shell keeps active downloads in memory, keyed by engine runtime ID. The footer derives its
Download mark from this map instead of querying a Space's download history for each progress update.
This also makes the mark work in Private windows, which do not record downloads.

## Developer tools

Core exposes one Open developer tools command. An adapter with an inspector attaches it to one tab
and reports that capability; an adapter without one leaves the command unavailable. The Qt adapter
hosts the bundled Chromium DevTools frontend in a second `WebEngineView` docked beside the inspected
page. Omaweb does not expose Chromium's debugging protocol during ordinary browsing.

An inspected tab stays active when another Space is selected because Qt requires both the inspected
page and its Developer-tools view to remain active. Closing Developer tools releases that exception.
Moving the tab to another Space closes Developer tools before the engine profile changes.

The frontend is themed by naming its own CSS custom properties again on the page it is drawn on, so
Omaweb ships no patched frontend and speaks no debugging protocol to it. Chromium themes it through
the tonal ramp its browser UI theme serves at `devtools://theme/colors.css`, and every one of the
frontend's design tokens is a tone of that ramp, so the Qt adapter names the ramp as well as the
tokens it is sure about. The theme palette carries a `syntax` section for the colours code is read
in, and the frontend's interface takes the theme's type family.

Which construct is drawn in which of those colours is one decision, made where a desktop's colours
become a palette: `integrations/omarchy/omaweb.json.tpl` for a desktop that renders the theme, and
`SYNTAX_SLOTS` in `scripts/import_terminal_theme.py` for a palette imported from a terminal. Both
land where an editor rendering the same theme lands rather than where the slot names suggest — a
tag's name sits in the magenta family with the keywords and an attribute's name in the cyan family
with the types, each pair taking the two strengths of its family rather than one colour; a plain
identifier is body text rather than a hue at all; and red stays out of code, because it carries
"this failed" everywhere else in the interface. Neither names a comment or a punctuation colour:
those are the theme's own text turned down, and `ThemeController` derives both for every theme it
loads, because a desktop palette has one quiet colour to give and a theme asked for both draws every
bracket as dim as an aside. A named colour too dark to read against the surfaces code is drawn on
keeps its hue and gives up only the lightness that made it unreadable. The frontend's DOM tree draws
a tag's brackets and its name in one token, so the adapter also wraps the frontend's `attachShadow`
before its scripts run and adopts one stylesheet into every shadow tree it opens; that is the only
place a rule, rather than a token, is needed.

Remote debugging exists only behind the `--remote-debugging[=port]` launch option. Omaweb binds it
to loopback, refuses to start when a Chromium debugging switch reaches the engine through the
command line or the environment, and takes Private windows away from that session.

## Everyday page commands

Find, zoom, reload, Reload bypassing cache, Stop loading, printing and site-requested fullscreen are
engine-view-contract operations, each with a capability beside it. A command whose capability an
adapter does not report is listed and unavailable, and running it reports the gap rather than doing
nothing.

Find belongs to a tab because it lives on that tab's adapter: the query and the match position are
the adapter's properties, so hiding the interface leaves both, and only a navigation clears the
matches. Zoom belongs to the tab in the other direction: the core owns the factor, writes it to the
session with the rest of the tab, and hands it to whichever adapter draws that tab. Zoom moves along
one fixed ladder rather than by a percentage, so both directions land on the same sizes and the ends
are bounded.

Printing is split between the adapter, which renders the page to a PDF in an Omaweb-owned spool
directory, and `omaweb-platform`, which presents it in the desktop's own print dialog, including
that dialog's PDF destination, and removes the spooled file afterwards. macOS presents it through
AppKit and PDFKit. Linux hands the rendered document to `org.freedesktop.portal.Print` as an open
descriptor, so the spooled copy can go while the portal still holds it. A desktop with no print
dialog behind either reports the capability off.

Notifications are split the same way. They arrive from a Space's profile rather than from one page,
so the origin is all there is to identify the sender by; the shell asks the core which tab in that
Space owns the origin and whether it is entitled to interrupt: any page of the Space on show, and
otherwise only a retained tab. What it hands the desktop always names origin and Space, and the
answer comes back by a key of Omaweb's own, because the page waiting on it belongs to a tab in a
Space and none of that is the desktop's to keep. A desktop with no notification service reports so,
and the page is told its notification closed rather than being left waiting on an answer nobody can
give. A Private window raises none at all: a desktop notification records the origin in a list that
outlives the private session.

A page may start playing on its own, and what waits for the reader is the sound rather than the
playback: a silent video interrupts nobody, and refusing playback outright costs pages that work in
every other browser. Chromium's gesture requirement cannot separate the two, so the shell allows
playback and holds the sound instead. Every tab on an origin the reader has not dealt with is muted
at the engine, and the whole origin is heard the moment they deal with it, by touching the page or
by asking the row. That silence is never the reader's own muting: it is not written to the session,
and the row offers the sound back in the same place it offers to mute.

The Qt adapter draws PDFs in Chromium's sandboxed viewer, which carries its own find, zoom, print
and download in a toolbar inside the page; Omaweb's own find bar does not reach into that plugin. An
adapter without a sandboxed viewer downloads the document instead, and the shell reports the missing
capability.

Site-requested fullscreen is a state the shell is in rather than something an engine does to the
window: the adapter accepts the request and reports it with the origin, and the shell takes the
window fullscreen, stands the outline aside, names the origin in a notice, and always exits on
`Escape`. The fullscreen the reader asks Omaweb for is tracked separately, so handing one back never
takes the other away. The window is also the desktop's to move, since a platform menu command or
shortcut takes it in and out of fullscreen without asking Omaweb, so the shell reads its own state
off the window's visibility, and a page left holding a screen the reader has already taken back is
told it no longer has one.

## Page context menu

Omaweb draws the page's context menu. The engine adapter accepts the engine's own context-menu
request, so no engine menu is ever shown, and reports the target as plain values rather than as the
engine's request object: no engine menu model or media enumeration crosses the engine-view contract.
The shell builds the rows from those values and runs them through the same commands the keyboard
uses.

## Keyboard navigation

Core loads and validates the versioned keybinding and site-passthrough policy. Engine adapters
receive the resolved configuration for the active URL and the shared page-command script through the
engine-view contract. The Qt adapter injects that script at document readiness; the UI-lab mock
accepts the same inputs, and the Ladybird adapter must inject the same script when ticket #7 adds
it.

Page commands ignore editable controls, IME composition, and unbound keys. Link hints use DOM
accessibility labels, CSS lengths that follow page zoom, and system-color keywords that remain
visible in forced-color modes.

## Window composition

Main and Private windows are frameless and expose explicit system-move and resize regions. Omaweb
does not draw visible window controls. Platform-native menus and keyboard commands retain minimize,
close, and quit behavior.

Qt Quick owns the browser chrome. Webpage viewports stay opaque. Omaweb-owned surfaces are alpha
over the desktop, with an opaque fallback for accessibility. Blurring what shows through is the
window system's: a small AppKit adapter installs it on macOS, while a Wayland compositor applies its
own and takes nothing from the client, so `installWindowChrome` is empty on Linux.

The macOS adapter applies that chrome when a window's surface is created and again after every
fullscreen transition. AppKit rebuilds a window's frame view across one, which takes back the
standard window buttons, drops the backdrop that was a subview of the old frame view, and returns
the window to an opaque background; nothing in the adapter reads the state it sets, so re-applying
it is the same as applying it. A window filling the screen squares its corners, and the backdrop's
mask follows. Auxiliary windows are deliberately framed dialogs and take none of this.

## Fast path

The default build uses C++23, Qt 6.11, CMake 3.30 or newer, Ninja, and Clang. Qt, Chromium,
Ladybird, and the Rust blocker never compile during ordinary application edits.

On the initial M2 Max development machine, provisional budgets are:

- QML-only change visible in under 2 seconds
- Ordinary incremental C++ rebuild and launch under 10 seconds
- Clean Omaweb build under 60 seconds, excluding prebuilt dependencies
- UI lab interactive within 500 milliseconds
- Browser chrome visible within 1 second warm and 2 seconds cold

These are budgets rather than measurements. `scripts/benchmark_build.sh` measures configure, clean
build, incremental rebuild, and no-op build against them. The recorded baseline is in
[the development guide](development.md#performance).
