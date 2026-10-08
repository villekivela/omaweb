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
  create additional ordinary browser windows. A tab can pop out into a Tab window of its own and
  stays a tab of its Space, listed in the main window's sidebar
  ([ADR 0062](../adr/0062-pop-a-tab-out-instead-of-opening-windows.md)).
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
  it and focuses it; the Omnibar's Add split view command offers a chooser of the unpaired ordinary
  tabs, headed by a blank tab. Selecting any other tab shows it alone and leaves the row in place;
  either half brings the split back, and tab cycling treats the row as one stop entered on its
  last-focused half. Separate split view puts two adjacent ordinary rows back. Closing either tab,
  or moving one to another Space, ends the split. Pinned tabs are never paired, a tab is in at most
  one split, and a split's tab is separated before it can be pinned or moved. The pairing is kept
  with the Space's tabs and restored after a restart, and is not part of the Sync projection.
- Each Space keeps a Tab jump list: the order its tabs became active, without repeats, at most 32
  entries. `Primary+O` jumps back to the tab before the one on show in that list and `Primary+I`
  jumps forward again. A jump changes no entry. A tab selected by any other route after jumping back
  is added at the end, after the tab it was selected from, so the entries ahead are kept and a jump
  back returns to that tab. Each half of a split is its own entry. A tab that closes leaves the
  list. The list survives a switch to another Space and back, starts afresh on the tab a restart
  restores, and is never written down or synced. A Space at rest jumps between its Pinned tabs as
  any Space does. At either end of the list the keys do nothing and show nothing.
- Each Space retains its 25 most recently closed tabs across restart. Reopening restores address,
  title, pin state, zoom, and mute in reverse closing order. A Private session keeps the same stack
  only in memory.
- An ordinary tab not on show for longer than the Settings row "Put away unused tabs" allows (Off, 1
  hour, 12 hours, 1 day or 1 week, 12 hours by default) closes into its Space's put-away list. Its
  age is the wall-clock time since it was last on show, counted across sleep, restarts and time in
  other Spaces, and kept with the Space's tabs; a tab stored before the time was kept counts from
  the first start that finds it. Pinned tabs, Keep active tabs, a tab making sound, an Agent tab
  while an Agent is attached, both tabs of a split, a tab with Developer tools attached, a row the
  reader is dragging and each Space's active tab are never put away. Omaweb checks at startup, on a
  Space switch, before the Space arriving is shown, and every five minutes, across every Space. The
  list keeps each tab's address, title, zoom and mute for 30 days, is listed newest first in the
  History sheet's `put away` group, by title, host and age, and is ranked with History in the
  Omnibar. Reopening one opens a new tab as reopening a closed tab does, and removes it from the
  list; the recently closed stack holds only the reader's own closes. Clearing a Space's history
  over a range clears what it put away in that range, and deleting the Space deletes the list. The
  list and the setting are outside the Sync projection. The first put-away of an installation shows
  a notice once, with a way to the setting. Private windows never put tabs away and have no list.
- Startup restores the last active Space, tabs, Pinned tabs, retained-tab settings, sidebar state,
  zoom, and mute. A command returns the active Space to rest. Private windows never restore.

## Interface

- The vertical sidebar shows the active Space. From top to bottom, it contains the navigation row,
  current-address trigger, pinned tabs, ordinary tabs, and footer. Pinned tabs use icon-only buttons
  in full-width rows with capacity for three to five tabs. Incomplete rows divide their width among
  their tabs. Ordinary tabs use single-line rows. The navigation row puts the sidebar and
  command-scope buttons first, followed by back, forward, and reload. The footer contains the
  Spaces, active Download mark, and settings. Private windows replace the Spaces with a mask. New
  tabs remain available through the Omnibar and keyboard commands rather than a sidebar button.
- Each Space has a Space colour, one of six that Omaweb owns: orange, yellow, green, teal, blue and
  violet. A theme does not choose them, and a theme's `spaces` are not read. Each is a fixed hue and
  chroma in OKLCH, and per theme only its lightness moves, until it reaches 3:1 against every ground
  a Space colour is drawn on, so it follows a theme change. Where sRGB cannot show that chroma at
  that lightness, the colour takes as much of it as sRGB can, keeping its hue. Each also keeps a 20°
  OKLCH hue gap from the theme's urgent, Private and Agent colours and from the other five: one that
  lands too near is turned just far enough to clear it, so every theme offers six. A grey accent has
  no hue to keep clear of. A new Space, the reader's or an Agent's, takes the colour fewest of the
  reader's Spaces have, the first in hue order on a tie, and taking an Agent Space over gives it one
  the same way. The reader sets any of the six in Settings' Spaces section, where each of the
  reader's Spaces offers them in hue order as small squares, the chosen one larger, and two Spaces
  may share one. There is no custom colour. The colour is part of the Space in Sync. A Space stored
  before Spaces had colours is given one at start, in footer order, and one stored as
  `bright_yellow`, `bright_green` or `bright_blue` is renamed `orange`, `teal` or `violet`. An older
  build gives a name it does not know one of its own and Sync carries that back, so the release
  notes ask readers to update every machine they sync.
- A switch at the top of Settings' Spaces section turns Space colour off. It is on by default and
  local to the machine, as the Scene choice is. With it off, the footer's squares, the Agent mark
  standing in for one of the reader's Spaces, the menu of the Spaces left out, and the Omnibar's
  Space squares and names are drawn in the muted text colour, and Settings hides the Spaces' colour
  squares. The Space on show is still the larger square and hovering a Space still names it. Agent
  Spaces are drawn as they are with colour on. Each Space keeps its colour while colour is off, and
  a new Space still takes the colour fewest Spaces have, so turning colour on shows Spaces already
  told apart.
- The footer draws each of the reader's Spaces as a small square in its colour, with no letter, and
  the Space on show as the larger square, with no plate or border around it. Agent Spaces follow as
  small Agent marks. One of the reader's Spaces with an Agent attached is drawn as the Agent mark in
  the Space's own colour, in its square's place, until the Agent leaves. Hovering a Space names it.
  The reader's Spaces come first and Agent Spaces after them, so `select-space` 1 to 9 and the Key
  labels count the reader's from 1 and an Agent making a Space never renumbers them. Taking an Agent
  Space over makes it the last of the reader's. Spaces the row has no room for are left out, last
  first, and a `+N` count in muted text says how many; nothing scrolls. The Space on show is never
  left out: it takes the last place there is room for, and the Space that stood there is counted
  instead. Pressing the count opens a menu of the Spaces left out, and each stays reachable by its
  key.
- While the sidebar holds the keyboard, `j` and `k` move the Sidebar cursor through its rows, pins
  first, without changing the page. `l` or `Return` opens the row under it and focuses the page, and
  `h` returns the cursor to the tab on show. A single key the reader binds to a command takes
  precedence.
- Holding Primary on its own for 400 ms shows Key labels over the navigation row, the address
  trigger, the Spaces and the first nine tabs, read from the live keymap. Releasing Primary,
  pressing another key, or the window losing the keyboard removes them.
- Every key the chrome shows is drawn as the website's `kbd`: a key cap 22 px tall and at least 22
  px wide with a 4 px radius, a 1 px accent border at 55% with a 3 px bottom edge, a ground of the
  accent at 10% over the plate, and the key in the accent in the mono face at weight 500 and 12 px,
  all scaled with the interface font size. A chord is a row of caps, one to a key. A Key label's cap
  keeps the ground when Primary finishes the chord it names, and is outlined without it for a key
  pressed on its own, so the difference the old filled and outlined labels made is kept. A special
  key is a symbol, not a word: Return and Enter `↵`, the arrows `↑ ↓ ← →`, Tab `⇥`, Backspace `⌫`,
  Delete `⌦`, Shift `⇧` and Space `␣`, as a Material Symbols icon where the font carries a matching
  glyph and as the Unicode symbol in the mono face where it does not. Escape is "Esc", and Ctrl,
  Alt, Home, End, Page Up and Page Down stay words. A cap keeps the key's spoken name.
- The window title names the page and the Space on show, and a Space switch shows the Space notice
  at the top of the page. A Private window's title names neither, and it shows no notice.
- An Agent tab's row ends with an Agent mark in the Agent accent, in the place the close button
  takes on hover. Hovering the row names the connection and says the tab stays rendered while
  attached. While an Agent tab is on show, its page is framed in the Agent accent, with a label at
  the top-right corner naming the connection and its last act. An Agent Space shows the mark in the
  Agent accent while an Agent is attached and muted otherwise, and one of the reader's Spaces
  holding an Agent tab shows it in the Space's own colour. Each mark pulses only while one of the
  Agent's commands is in flight, so idle chrome draws no frames.
- Opening an Agent Space shows a notice at the top of the page that an Agent made it, with Take over
  and Dismiss, and the Take over Space command does the same. Taking it over removes the mark and
  keeps a temporary Agent Space after its connection closes. A Private window is never an Agent's
  and shows none of these marks.
- On Linux, turning on Allow agents in Settings' agents section links the Agent skill the
  `omaweb-cli` package installs into `~/.agents/skills` and into the skills directory of each agent
  whose home exists: Claude Code, Codex, pi and Hermes. No directory is made for an agent that is
  not installed. Turning it off removes only the links that point at the packaged skill. An `omaweb`
  entry that is anything else, such as a directory or a link to a checkout, is the reader's and is
  never replaced. A reader who already had Allow agents on when Omaweb began offering the skill is
  linked once, at the first start of that version, and never again. While Allow agents is on, a
  muted caption under the switch says what the switch or a button last added, for which agent, and
  what it left alone, and an Agent setup row offers Add skill and Add MCP server. Add skill links
  the skill for an agent installed since. Add MCP server runs
  `claude mcp add -s user omaweb -- omaweb mcp` and `codex mcp add omaweb -- omaweb mcp` for
  whichever is on the `PATH`, after the agent says it has no server named `omaweb`, and is hidden
  when neither is. Turning Allow agents off leaves the registration in place.
- A tab with no address to load shows the Start page in place of a webpage, never an empty viewport.
  That covers a Space at rest and an `about:blank` the reader navigated to, and no engine is spent
  behind it. The Start page is the Omnibar at rest above the Scene's horizon, centred in the page
  area, focused, over the Scene the reader chose. The field is the website's dash:
  `min(page area width - 32 px, 720 px)` wide, its top edge 50 px above the horizon, which lies at
  half the page area's height, with a 50 px field and 16 px of padding. At rest with no results the
  panel holds the field alone, with no hint row under it, and the empty field holds only its
  placeholder and, at its right end, where Return goes. A muted `?` key cap and "shortcuts" stand in
  the page area's lower right corner, 24 px in from its edges and clear of the sidebar, while the
  Omnibar rests on the Start page with an empty field. They go once the reader types, and an Omnibar
  summoned over a page has none. `?` typed into the empty field opens the Shortcut sheet.
- Settings' interface section chooses the Scene, locally: Night road, Night sky, Game of Life,
  Vector terrain, Radar, Hyperspace, or None, which leaves the Omnibar over the sidebar's fill. It
  is a grid of small thumbnails, four to a row, in that order, each a still of its Scene drawn by
  the Scene itself, without the CRT glass, and None as the plain sidebar fill, each named under it
  and the chosen one marked in the accent. A click chooses, and the keyboard walks between them in
  reading order. The choice takes effect at once and survives a restart. A reader who had turned the
  road off before there was a choice arrives on None, and anyone else on Night road. A Private
  window shows the ordinary window's choice. "CRT glass over the Scene", below it, applies to every
  Scene and is offered unless None is chosen.
- A Scene fills the page area, or the one pane of a split it stands in, and is never drawn under the
  sidebar, which stands on its own fill as it does with None. It is drawn at the page area's size,
  centred on it; the road has the composition the website draws in a viewport of that size. With the
  sidebar hidden the page area is the whole window. A Scene moves only while the Start page is on
  show and the window is on screen and holds the keyboard.
- The road is drawn from the palette and follows a live theme change: a horizon and a banded sun,
  mountains falling to where the road runs out, a star field, the road from the bottom edge toward
  the horizon and lane marks drifting toward the reader. It is shown as a monochrome pixel display
  in the accent, dithered, with a dark seam between display pixels. The road is always night: a
  light theme's night is drawn from its dark text. A Private window's road has its lights off, with
  no sun, stars, lane marks or posts.
- The night sky is drawn from the palette in the same way and follows a live theme change, and it is
  always night, a light theme's night drawn from its dark text. A dark planet rises from the bottom,
  after Navigator's globe: its face from the palette's dark, a thin glow in the palette along its
  curved limb. The planet lies wholly under the resting Omnibar: its top, the glow included, stands
  a small gap below it, so the curve shows whole. The Omnibar keeps the place it has over the road.
  Over it a still star field, and a comet or a shooting star now and then on a slow diagonal,
  falling above the limb and going out behind it. There is no mark: the Omnibar is the one thing in
  the middle. A comet passing near the field briefly catches the Omnibar's rim. On commit stars fall
  thick and fast as streaks until the page paints, then the page takes over as it does from the
  road. Reduced motion holds one frame, the star field and one comet part way across, with no glint.
  A Private window's sky has its lights out: the planet and its limb stay, with no glow, stars,
  comets or glint, even on commit.
- The Game of Life is Conway's, drawn from the palette in the same way and always night, a light
  theme's ground drawn from its dark text. Each cell is one of the Scene's square pixels, and the
  board wraps at its edges. Live cells are the accent, and a cell that has just died leaves a short
  trail, dimmer for each generation since. At rest the board moves about eight generations a second.
  Random soups fed by a glider gun or two keep it alive, a gun on a small board and two on a page
  area's, and it is seeded again when it dies out or stalls, and for a new size. On commit
  generations run flat out, one a frame, until the page paints, then the page takes over as it does
  from the road. It casts no light on the Omnibar. Reduced motion holds a board with a few gliders
  mid-flight, each with its trail. Its thumbnail shows that board twice as close as the other
  Scenes' thumbnails, so the gliders read at that size. A Private window's board is frozen, its
  cells dimmed and without trails, even on commit.
- The vector terrain is after Battlezone and the vector arcade games, drawn from the palette in the
  same way and always night. Wireframe mountains stand on a horizon below the Omnibar, no peak
  higher than a small gap below it, as the sky's planet, and each hides what stands behind it. A
  perspective grid lies on the ground before them, and a wireframe crescent moon hangs low in the
  sky beside the Omnibar, or just above it where the page area has no room beside it. Its lines are
  thin, the mountains and the grid in the accent, the farther ridge dimmer, the moon in the theme's
  light, each with a vector monitor's glow. At rest the ridges drift by in slow parallax, the nearer
  faster, and the grid holds still. On commit the grid's lines rush toward the reader while the
  mountains hold the horizon, until the page paints, then the page takes over as it does from the
  road. It casts no light on the Omnibar. Reduced motion holds one frame of the mountains, the grid
  and the moon. A Private window's terrain is the wireframe alone, its lines the theme's light
  dimmed, with no moon and no glow, and it holds still, even on commit.
- The radar is after a plan-position display, drawn from the palette in the same way and always
  night. The whole page area is its face, with faint range rings and spokes running out past its
  edges from a centre behind the Omnibar's field. A phosphor sweep in the accent turns clockwise
  from there once every six seconds, with about two seconds of afterglow behind the beam. Each open
  tab in the Space has a blip at a bearing a scatter of its site gives, between the Omnibar and the
  page area's edge, so a tab's blip stays put while others open and close and none stands behind the
  Omnibar; the sweep lights it as it passes and it fades with the afterglow. The picker's thumbnail,
  with no Space behind it, shows a few blips all the same. The sweep faintly catches the Omnibar's
  rim where the beam leaves the field, so the light rides round the rim as it turns. On commit the
  sweep spins fast, and a new blip for the page being opened brightens where its tab's blip will
  stand, until the page paints, then the page takes over as it does from the road. Reduced motion
  holds the sweep at one bearing with its afterglow, and the tabs' blips lit. A Private window's
  radar is the sweep, the rings and the spokes alone, its lines the theme's light dimmed, with no
  blips, no glow and no light on the rim, and it holds still, even on commit.
- Hyperspace is the view out of a cockpit window, drawn from the palette in the same way and always
  night. A deep star field drifts slowly toward the reader from a centre behind the Omnibar's field,
  over a faint nebula in the accent. On commit the stars stretch into long streaks from that centre
  and rush past until the page paints, and the streaks hold while the page replaces the Scene, with
  no tunnel and no collapse back to stars. It casts no light on the Omnibar. It is an homage to an
  effect, not to a film: there is no logo, opening crawl, ship or film's name. Reduced motion holds
  the stars and the nebula still, with no streaks. A Private window's hyperspace is the nebula
  alone, very dim and in the theme's light, with no stars and no jump, even on commit.
- On commit the lane marks speed up and the road keeps driving until the page first paints, then the
  page takes over. After two seconds the road hands over to the page loading indicator. A load
  error, an HTTPS-only page or a certificate error ends it at once.
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
- The Shortcut sheet lists the browser commands and the keys that run them, read from the live
  keymap so it cannot promise a key the window does not answer. The Keyboard shortcuts command
  summons it, from the Omnibar or with `Primary+/` and `?`, over a page or over the Start page,
  where `?` typed into the empty field summons it. It closes with `Escape` or its close button.
- An Omaweb surface that takes the whole page area, the Shortcut sheet and the settings page, blurs
  the page beneath it rather than sealing it off, so the reader can still see the place they left
  without being asked to read a webpage through it. It takes the sidebar's colour but its own
  semantic opacity: the sidebar is read against the desktop, and a sheet is read against a page
  whose contrast is unknown, so at the sidebar's value a dark page shows through as nothing. Where
  there is no page to blur, as in a Space at rest, the surface takes the sidebar's translucency
  instead, which is the reader's Sidebar opacity where they set one, and the desktop behind it is
  left to the window system, which blurs it or not exactly as it does behind the sidebar.
- A bar at the top of the page, a page question or a page prompt, keeps its translucent ground and
  blurs the page under that ground, so the page shows through as colour and shape but its text
  cannot be read against the bar's.
- Tabs can show site favicons or a two-character host code. The reader can turn favicons off and can
  choose whether favicon artwork is recoloured to the host-derived tint. With favicons off, the host
  code is drawn in the colour of the site's own favicon, and in a neutral colour where the favicon
  has none to give. Pinned-tab icons are larger than ordinary-tab icons.
- A tab's favicon is the icon its loaded page reports. Each Space stores the icon each page showed
  in its own database, by address and site, and a tab whose page has not loaded, after a restart, in
  a new window or before its page reports an icon, shows the stored icon for its address, or else
  the newest one for its site. The lookup runs off the interface thread, and the host code shows
  while it is pending. A site the Space has never loaded keeps its host code. A Private window keeps
  its pages' icons in memory for that window alone and never reads a Space's.
- Browser chrome does not occupy a toolbar above the webpage. The webpage uses the full height
  beside the sidebar. While the sidebar is hidden, the navigation controls, the sidebar toggle and
  the command-scope trigger float over the page's top corner on the sidebar's side instead of taking
  a band from it.
- The sidebar stands against the window's left edge until the reader chooses the right one under
  Sidebar side in Settings' interface section. The choice moves the sidebar, its seam and resize
  handle, the page beside it, the edge it hides toward and peeks from, and the floating controls at
  once, and it survives a restart. The find bar stands across the page from the sidebar's side.
- `Primary+B` hides the sidebar entirely. In that chromeless state the page keeps the whole window,
  the floating controls appear. Hiding or showing the sidebar eases the seam and the page travels
  with it, keeping one width for the whole movement: the page lays out once rather than at every
  width the seam crosses, and the sidebar's rows keep their own width as it goes.
- The floating controls are a default rather than a fixture. Settings' interface section turns them
  off, and without them a hidden sidebar leaves the page the whole window. The choice survives a
  restart, and the keys that hide and show the sidebar work the same either way.
- The chrome moves what the pointer did and settles what a key did. A click, a drag or a tap that
  hides or shows the sidebar, switches Space or tab, or opens or closes the Omnibar eases there, so
  the eye can follow where something went. The same action from a key binding, from the keymap or
  from the Omnibar's command scope, a clicked command row included, arrives in its settled state in
  the frame it was asked for. Every other eased transition of the chrome follows the same rule: the
  sheets, the panels, the Glance and the page's nudge. Motion that tells the reader something moves
  for both: the Space notice naming the Space they landed in, and an Agent mark's pulse while one of
  its commands is in flight. Omaweb offers no setting of its own for any of this.
- The desktop's reduced-motion preference stills the chrome whatever started it. Omaweb follows four
  sources, and any one asking for reduced motion is enough: the desktop portal's
  `org.freedesktop.appearance` `reduced-motion`, Hyprland's `animations:enabled` read when the
  browser starts and again on each config reload, GNOME's `org.gnome.desktop.interface`
  `enable-animations` through the same portal, and macOS's Reduce motion. Under it the sidebar, a
  peek, a Space, a tab, the Omnibar, sheets and panels arrive settled, the Space notice appears and
  goes where it stands, the page loading indicator holds still, a busy Agent mark is held dim
  instead of pulsing, and a button or a toggle changes state without a fade or a slide.
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
- A drag on the sidebar's navigation strip or on the outline's empty space below and around its rows
  moves the window. A drag on a row still reorders it.
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
- Security state and the blocked-request count ride inline in the address trigger. The lock opens
  Site information at its top, and the shield beside the count opens it at the blocked requests.
- Clicking the sidebar's current-address trigger or pressing `Primary+L` opens the Omnibar for the
  current tab. The Omnibar has one place whatever it opens over: centred in the page area with its
  top edge 50 px above the Start page's horizon, where a new tab shows it. Its rows grow down from
  the field and never move it; a short page area lists fewer rows at a time instead.
- The Omnibar field is drawn as the website's dash: the Omaweb mark as its prompt in the palette's
  accent, drawn 32 px across as the website draws it, the typed text and a blinking block caret, and
  a `→` go mark that commits as `Return` does. In command scope the mark gives way to `:`. The field
  asks "Where to?", as the website's does, for a new tab too, since the label at its right end says
  "New Tab". While the field is empty at rest, the placeholder is followed by a gap, a `?` key cap
  and "shortcuts" in the muted text, the way to the Shortcut sheet, which `?` typed into the empty
  field opens; both go with the placeholder once the reader types. Command scope and an engine
  keyword keep their own prompts. The label at the field's right end, before the arrow, says where
  Return goes: "This Tab", "New Tab" or "Command", muted and at the size of the rows' own labels, in
  title case in English and as plain words in Finnish. A screen reader hears what the field takes,
  "Address, search, tabs and Spaces", rather than the prompt. The typed text and the caret carry no
  glow, rim light, blur or shadow, and the text clears 4.5:1 against the field's glass in every
  theme, over the darkest and the lightest the road can put behind it. Over the road the glass blurs
  it heavily under a nearly opaque tint of the overlay, so the rows read cleanly, and over a page it
  blurs the page. The sun's light on the field's edge is drawn at half the website's strength.
- `Primary+T` and `t` show the Start page in place of the page on show. Omaweb creates the tab only
  after the user commits a destination. Choosing an open tab from it switches to that tab and
  creates none. `Escape` brings back the page that was on show; in a Space at rest it releases the
  field instead: the caret goes, the field is drawn unfocused and the keyboard is the browser's, so
  every bare key of the key map works as over a page, and `o`, `Primary+L` or a click on the field
  give the field back. In a split the Start page covers both panes, where the committed tab lands,
  and `Escape` brings the split back. While the Start page is on show, `Primary+L` and `o` focus its
  Omnibar.
- While there are results, a hint row under the Omnibar's field, over a rule, names the keys that
  work the list, as the website's dash does, with `?` and "shortcuts" at its right end: `↑↓` as key
  caps and "select", `↵` and "go", in 11 px dim text over a rule. Command scope says "run" for `↵`
  and adds `⌫` and "back", which leaves the scope. The field answers the keys the key map names for
  the Omnibar, and the row names the same keys, so a key the row shows is a key the field answers.
  The words are translated. With no results there is no row. An item can join it beside `?`.
- The Omnibar ranks the typed text against the open tabs, Spaces, the active Space's local history,
  search keywords, and browser commands in one list. The tab on show is never a row, and a Private
  window lists no Spaces and no history.
- The Omnibar searches every Space's open tabs. The active Space's tab rows come before any other
  Space's, however weakly they hold the typed text, and the other Spaces' follow in Space order. A
  tab row from another Space names its Space as its label at the right edge, in the text colour,
  beside a small square in the Space colour, or for an Agent Space in the Agent accent while an
  Agent is attached and muted otherwise. Colour carries no text there, so it needs only 3:1. With
  Space colour off, the name and the square are muted, and the name turns to the text colour on the
  selected row. Committing it switches to that Space with the tab on show as one action: a tab the
  Space no longer holds leaves the reader where they were. The rows are read from what the session
  keeps of each Space, so listing them resumes, loads or thaws no page. A Private window lists only
  its own tabs.
- Engine suggestions, an installation-wide setting in Settings' network section, are off by default
  and stay out of Sync. With them on, the Omnibar asks a search engine for Engine suggestions only
  when Return on the current text would search: never for an address such as `github.com/foo` or
  `localhost:8080`, in command scope, for empty terms, or in a Private window. It asks the engine in
  the keyword chip, or the default engine, and only one with a suggest URL. It waits about 150 ms
  after the last keystroke, drops an answer for text that is no longer current, and gives up after
  about a second. A failed or slow answer lists nothing and reports nothing. The request goes
  through the browser's own network client with no cookies, no Space and the `Omaweb` user agent,
  and only an OpenSearch suggestions answer (`["typed", ["s1", "s2"]]`) is read.
- At most four Engine suggestion rows list below every local row, and one equal to the typed terms
  is dropped. A row shows the proposing engine's site tile, drawn as a keyword row's is, then the
  suggestion with the typed prefix in regular weight and the rest in bold, and `search →` at its
  right edge. Its screen-reader name is "Search <engine> for <suggestion>". Choosing it searches
  that engine for the suggestion, which is never opened as an address even when it looks like one.
  The Settings switch's note names the default engine, or says that engine doesn't offer
  suggestions.
- Each Omnibar row leads with a picture of what it names. A tab row draws the site's tile as the
  sidebar does, following the Use favicons and tint settings, so with favicons off it is the host
  code in the site's tint. A history row draws the same tile for its page and a keyword row for its
  engine's site. They take the favicon of an open tab on the same site in the same window, and
  otherwise the one the Space stored for the page or its site, as a restored tab does, with the host
  code and tint for a site the Space never loaded. They never show another Space's or window's
  artwork. Another Space's tab row draws the favicon that Space stored for its page, as the Space's
  outline does once it is on show. A Space row shows the Space's colour, and a command row its
  group's symbol, in the accent: full on the selected row and softer on the others. Omaweb never
  fetches an icon from the network to fill a row.
- A tab or history row reads as the title, then the host in the muted colour; a history row gives
  its full address to a screen reader as the row's description. At its right edge a row says what
  committing it does in a muted word at the row's own size, then an arrow in a key cap:
  `Switch to Tab`, `Switch to Space`, `Open`, `Reopen`, or a keyword followed by `Search`, in title
  case in English and as plain sentences in Finnish. A tab of another Space has that Space's name
  there instead, which is said once and not again beside the host. On the selected row the word
  turns to the text colour and the cap fills with the accent, its arrow in the panel's ground
  colour. The selected row is the website's: the accent at 14% over the panel, so the glass shows
  through it, with a 2 px accent bar at its left edge. The action is bright on the selected row and
  muted on the others. A command row shows its keys there instead, as key caps: one to a key, the
  bare binding before the chord, a quiet gap between bindings and no dot, right-aligned so the caps
  line up down the list, and the title elides before the caps clip. A keyword stays typed text.
  Every row's accessible name still says what it does.
- `Return` commits the typed address or search, even when a row matches elsewhere in its title. When
  an open tab's title or host starts with the typed text, that tab's row is selected instead and
  `Return` switches to it, and to another Space's tab only when no tab of the active Space holds the
  typed text. A field still holding the preset address commits it as typed.
- `Primary+K` and `:` open the Omnibar in command scope, shown as a leading `:` in the field: every
  action Omaweb can perform is fuzzy-searchable there, and each result shows the keys that invoke
  it, so the Omnibar is also how the keymap is learned. Its rows look like the rest of the
  Omnibar's: the group's symbol, then the title, with no group name. Typing `:` into the field
  narrows it to commands, and backspacing the `:` widens it to every row for the same text, so text
  that starts with `:` is never searched as typed. An action that cannot be reached from the Omnibar
  is a defect.
- Target-specific page actions are the exception to the Omnibar rule. An Omaweb-owned page context
  menu opens by pointer or `Shift+F10`; the Omnibar exposes Open page context menu, while actions
  such as copy link, save image, and Inspect element remain inside the menu because they require its
  target.
- Tabs reorder by pointer and keyboard. Pinned tabs stay within the Pinned section and ordinary tabs
  within the ordinary section. Duplicate tab opens the current address in a new ordinary tab without
  copying navigation history, form state, or a live page. The ordinary-tab context menu can close
  the other ordinary tabs or the ordinary tabs below it and never includes Pinned tabs.
- Themes reload at runtime from versioned JSON. A theme defines type as well as colour: font
  families, sizes and label spacing, and the tinting of tab tiles. Semantic opacity values control
  transparent surfaces, which fall back to an opaque color where accessibility settings require it.
  Blurring the desktop behind them is the window system's and is not required for them to read. A
  Transparent surface carries a still dither of one output level, added after its own opacity, so a
  blurred wallpaper's gradient seen through it does not band. The dither averages to the theme's
  colour, and an opaque surface or the empty window background has none.
- The sidebar's opacity is the theme's until the reader sets it. Settings' interface section has a
  Sidebar opacity slider from 50% to 100% in steps of 5%, with a reset that returns to the theme's
  value, and it can be moved with the arrow keys. It is the reader's override of the theme's
  `opacity.sidebar`, as the interface font size is of the theme's font: it stands until reset, and a
  theme switch changes the sidebar only while no override stands. The sidebar follows the slider as
  it moves, in an ordinary and a Private window. The value is kept as `sidebar-opacity` in
  `settings.json`, is local to the machine and is not synced. It does not change the sheet, overlay
  or window opacities, which stay the theme's, and the page viewport stays opaque.
- Quiet text is content, not decoration: a tab's title, the footer's controls. Whatever a theme
  names for it, Omaweb holds it to WCAG AA against every ordinary and Private surface it is drawn
  on, and therefore clear of the disabled rendering of ordinary text. A reader must never have to
  guess whether something is merely quiet or actually unavailable. Borders clear the WCAG AA
  non-text threshold against every surface they separate. Contrast repair keeps a colour's
  theme-supplied hue. Where a palette's own surfaces put a threshold out of reach, the colour that
  reads best on the worst of them is used and the rest of the theme is kept: the browser always
  follows the desktop's colours.
- The Agent accent keeps its theme-supplied hue and clears 4.5:1 against the window and the sidebar,
  since the page frame's label is written on it. It is kept apart from the accent.
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
  apart from the first reload after a Content blocking rule change. Reload bypassing cache does not,
  and Stop loading ends the current load without clearing page state.
- Browser fullscreen and site-requested fullscreen are separate. Site-requested fullscreen begins
  with a visible origin notice and always exits with `Escape`.
- Printing uses the native print dialog and includes the operating system's PDF destination. Where
  an engine provides a sandboxed PDF viewer, Omaweb opens PDFs inline with search, zoom, print, and
  download; another engine downloads the document and reports the missing capability.
- Screenshot page saves the page area as the engine drew it, at the display's device pixel ratio and
  with none of Omaweb's chrome, as a PNG named for the page's title and the moment into the
  downloads location, and lists it in the downloads list as a finished download. Copy screenshot
  puts the same image on the clipboard. Screenshot full page and Copy full-page screenshot take the
  whole document from top to bottom instead, a screenful at a time, with what the page fixes to the
  viewport drawn once at the top and what it makes sticky drawn once where it sits in the page; the
  reader's scroll position, selection, and fixed and sticky elements are as they were afterwards. A
  page taller than 32,767 device pixels is refused, naming that limit, and leaves no file behind. In
  a split the active tab is captured; a Space at rest and the Start page have no page to capture and
  say so. A Private window's screenshot lands in the same downloads location, because the reader
  asked for a file.
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
- Picture-in-picture is unavailable on the Qt engine. A video's controls offer no picture-in-picture
  button, `document.pictureInPictureEnabled` is false, and a page's `requestPictureInPicture()` is
  rejected with a `NotSupportedError` the page can catch. The floating window waits for an engine
  that surfaces the request.
- Form history remembers what the reader typed into an ordinary text field when its form is
  submitted, under the field's name or else its id, within the Space of the page. Only a value the
  reader typed or picked from the list counts, never one the page wrote or sent prefilled, and an
  Agent tab's forms are not kept. The field is a text, search, email, telephone or URL input in the
  page's main frame. It never remembers a password field, a field or form the page marks
  `autocomplete=off`, a field marked as a card, password or one-time code, a field whose name reads
  as a card number or security code, or a value of thirteen to nineteen digits that passes the Luhn
  check, whatever the field is called. Submitting a value again makes it the most recently used. A
  Private window neither offers nor remembers anything, and form history stays out of Sync.
- Focusing a field with history opens a suggestion list Omaweb draws on the component kit's popup
  surface, at least as wide as the field, under it, or above it when the page has no room below, and
  none while the field is scrolled out of the page. It lists the field's values most recently used
  first, at most six, each with the typed prefix in regular weight and the rest in bold, and leaves
  out a value equal to what the field holds. Typing narrows it to the values that start with what
  was typed. The field keeps the keyboard: the page gives up `Down`, `Up` and `Escape` only while
  the list is shown, and `Enter` and `Shift+Delete` only while the keyboard has highlighted a row; a
  pointer over a row only shows it. `Enter` or a press on a row fills the field and closes the list,
  `Shift+Delete` forgets the highlighted value, and `Escape` closes the list until the field is
  focused again; a Glance closes on the next `Escape`. No value reaches the page until the reader
  accepts it.
- Settings keeps the reader's addresses, each a name, street, postal code, city, country, phone and
  email, and lists, adds, edits and removes them. An address needs a name. Addresses belong to the
  browser rather than a Space, are offered in every Space and never in a Private window, and stay
  out of Sync.
- A text field or text area whose autocomplete token is `name`, `given-name`, `family-name`,
  `street-address`, `address-line1`, `postal-code`, `address-level2`, `country-name`, `tel` or
  `email`, after any section, `shipping`, `billing` or contact word, offers the saved addresses in
  the suggestion list whether or not form history keeps the field, once the reader has pressed the
  field or typed into it. A field the page focused itself offers none until then. The addresses come
  first, each its name with the street and city muted under it, then the field's form history below
  a divider, six rows in all. Typing narrows them to those whose value for the field starts with
  what was typed.
- Accepting an address fills the focused field and every empty field with one of those tokens in its
  form, or outside any form when the field has none. A field that already holds a value keeps it.
  The fill is only for the focus the list was drawn for: a page that moved the keyboard since gets
  nothing. A `given-name` takes the name but its last word, a `family-name` its last word, and a
  select marked `country` or `country-name` the option whose value or text matches the country; a
  text field marked `country` asks for a code an address does not hold and is left alone. It fills
  only a field the reader could type into and see: one that is drawn, not transparent, at least four
  pixels each way and not before the page's start. A field clipped away or covered by another
  element is not caught. It leaves a field the address has no value for as it was, and form history
  does not keep what it filled. `Shift+Delete` forgets no address; Settings removes one.
- Payment cards are kept in the desktop's Secret Service, one item each, whose attributes hold only
  Omaweb's schema name and a random identifier and whose label is the same for every card, and
  nowhere else: not in Omaweb's data directory, not in Sync, not the last four digits. The keyring
  is read the first time a card is needed, so a locked keyring asks to be unlocked then; one the
  reader left locked is asked again only from Settings, never by a page or a field. A machine with
  no Secret Service has no payment cards, and Settings says the desktop offers no secret store.
  Cards belong to the reader rather than a Space, are offered in every Space, and a Private window
  neither offers nor saves one.
- Settings lists, adds, edits and removes cards, each by its nickname, or its brand, and its last
  four digits, with the name on the card and the expiry under them. Once saved, a card's number is
  shown only as its last four digits, and an edit that leaves it empty keeps it. A card needs 12 to
  19 digits that pass the Luhn check. The security code is never stored.
- A text field whose autocomplete token is `cc-number`, `cc-name`, `cc-given-name`,
  `cc-family-name`, `cc-exp`, `cc-exp-month` or `cc-exp-year`, after any section, `shipping` or
  `billing` word, offers the saved cards in the suggestion list while it is empty, once the reader
  has pressed it or typed into it, in the main frame or any frame below it. A page whose certificate
  the engine did not accept without an exception, a waived check included, and a plain HTTP page are
  offered none, and the list says the cards are offered only on secure pages. What is typed into a
  card field is never reported to the shell.
- Picking a card is the confirmation: there is no second dialog, and nothing is filled without it.
  It fills the focused field and every empty card field of its form, in that field's frame and no
  other: never the page around a payment frame or a frame beside it. The number, the name split as
  an address's is, and the expiry as the field asks for it (MM/YY, MM/YYYY, a month or a year, a
  select by its option's value or text) are filled; the security code is left for the reader. Site
  information lists each fill for the rest of the page's life, by the card's last four digits and
  the origin of the frame it went into when that is not the page's own. Nothing about a fill is
  written to history or to disk.
- When the reader submits a form, in a secure context and in the tab on show, whose `cc-number`
  field still holds what they typed, a bar on the permission bar's surface offers "Save card ••••
  4242 to the keyring?", naming the site and the Space, with Save and Not now. A value the page
  wrote, a field the page had put text in when the reader started typing, a submit the page made up,
  and one with no input of the reader's behind it, as `requestSubmit()` makes one, offer nothing.
  The card is taken from the page by a call of the shell's own, never over the page's console, and
  held in memory until the offer is answered or another tab is on show. Not now forgets it, and
  nothing is remembered per site. No offer is made in a Private window, without a Secret Service, or
  for a card already saved. The security code is never read.
- While an Agent's step runs in a tab, or the tab is an Agent's, what its page reports typed is not
  the reader's: form history keeps none of it, no address or card list is offered, and no card is
  offered for saving. A step is held for a second after it answers, since a submit it made is heard
  after, and its page forgets which fields were typed into.
- Clear browsing data offers Payment cards as a category of its own, off each time the dialog opens,
  which deletes every saved card from the keyring whatever the time range.
- Video decodes on the GPU where the host has a working VA-API driver, and in software where it has
  none. A missing driver is not a refusal to start, and the driver packages are `optdepends` rather
  than dependencies because which one a host needs depends on its GPU.
- Omaweb supplies no spellchecker, translation, Reader mode, View source command, print preview, or
  installed-web-application model. The desktop print dialog owns preview and PDF output.

## Browser-owned pages

- History is a browser-owned full-page sheet using no web engine. Over a live page it blurs that
  page; in a Space at rest it uses the native window backdrop. It searches only the active Space and
  deletes one visit, one origin, a time range, or the entire Space history. Deleting history also
  deletes the stored favicons of the pages it names, except one a tab in that Space's sidebar still
  shows and one whose page is still in History. Private windows record none.
- Settings clears selected cookies, storage, cache, permissions, history, and form history for one
  Space and time range by default. Clearing every Space is a separate explicit choice. Deleting a
  Space removes all of its browser-managed data after confirmation.
- A configurable local search-engine list stores a name, query URL, optional suggest URL, and
  optional keyword. Omaweb ships DuckDuckGo (`d`), Google (`g`), Bing (`b`), Brave Search (`br`),
  Kagi (`k`), Ecosia (`e`) and Startpage (`sp`) configured, with DuckDuckGo as the default. A list
  saved before they shipped is given the ones it lacks once, and an engine deleted afterwards stays
  deleted. Each ships with a suggest URL that answers in OpenSearch suggestions JSON, except Kagi,
  whose endpoint needs a subscriber's session token. A list saved before suggest URLs existed gives
  a shipped engine its suggest URL once, and only while its query URL is still the shipped one. The
  add-engine form takes a suggest URL with `{query}` between the query URL and the keyword. Left
  empty, the engine offers no Engine suggestions.
- A keyword is matched case-insensitively and stored lowercased, so two keywords that differ only in
  case cannot both be saved. The Omnibar names the engine a typed keyword selects while it is typed:
  the space after the keyword moves the engine into a chip ahead of the terms, Backspace on empty
  terms puts the keyword back as text, and the field describes itself to a screen reader as "Search
  <engine> for <terms>", or the default engine for text with no keyword. No row repeats it. The
  keyword alone opens the engine's front page. While the text could still become a keyword, the
  Omnibar offers each matching engine other than the default as a row beneath the history rows.
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
- `Primary+O` and `Primary+I` to jump back and forward through the Tab jump list. Opening a file has
  no default key and stays in the command scope.
- `gs` for the next Space and `Primary+1`–`Primary+9` for a specific one.
- `Primary+B` to hide the sidebar, `Primary+E` to focus it, `Primary+,` for settings, and
  `Primary+K` or `:` for the Omnibar's command scope.
- `Alt+H`, `Alt+J`, `Alt+K` and `Alt+L` to move the keyboard between the regions on screen.
- `Primary+Shift+I` for Developer tools, `Primary+Alt+C` to inspect an element, `Primary+Shift+C` to
  copy the address of the page on show, and `Primary+Shift+L` for Site information.
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
- A machine that connects Sync to a repository already holding Spaces takes the repository's Spaces
  and their tabs. Its own Spaces are replaced, whether or not they were used, and the repository's
  first Space becomes active.
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
- A site that asks for a security key or a passkey is answered through Omaweb's own prompt, a bar in
  the place the Site-permission question takes, naming the site the key would sign in to and the
  Space, or the Private window. The engine moves one request through its steps and the prompt
  changes in place: touch the key, enter or choose its PIN with the attempts left where the engine
  reports them, choose one of the accounts the key holds, or a failure that names its cause in one
  line. `Escape`, Cancel and Close decline in every step, which the site sees as the reader
  declining, and so does leaving the tab or closing the page. Only the page in front of the reader
  may ask; any other is declined without a prompt. An Auxiliary window asks in the window that
  opened it.
- Security keys reach the Qt engine over USB HID only. The Arch package's QtWebEngine is built
  without Bluetooth, so a phone cannot answer over hybrid transport, and Chromium has no platform
  authenticator on Linux, so a passkey stored on the computer cannot either. The touch prompt and
  every failure say which of these the engine cannot reach. QtWebEngine raises a request only once
  it has something to ask, so a key that needs no PIN and holds one account for the site is touched
  without a prompt, and a request no key answers shows nothing until it times out. The engine names
  an account only by the name the site stored on the key, so that is what the account chooser shows.
- Third-party cookies are blocked by default. Authentication and payment flows may receive a
  temporary origin-specific allowance listed in Site information's third-party detail and revocable
  from it.
- Global Privacy Control is on by default and browser-wide. While it is on, every request from every
  Engine profile carries `Sec-GPC: 1`, subresources and the engine's own requests on a page's behalf
  included, and `navigator.globalPrivacyControl` reads `true` in every frame. Spaces and Private
  windows send it alike. The privacy section of Settings shows the setting and turns it off, which
  turns off both the header and the property, and the choice survives a restart. Omaweb sends no Do
  Not Track header and offers no per-site exception.
- Every Engine profile, in a Space, a Private window or an Agent Space, sends the engine's default
  user agent without its `QtWebEngine/<version>` token and the space before it, on every site. Sites
  such as WhatsApp Web answer that token with an "update your browser" page. The rest of the string
  is the engine's own, so an engine update reports its own Chrome version, and `navigator.userAgent`
  matches the header. Client hints stay as the engine sets them. There is no per-site override and
  no setting.
- HTTPS-only mode is on by default and browser-wide. Every top-level `http:` navigation, typed,
  followed from a link, redirected to or opened from another application, is sent as `https:` before
  it leaves the browser; subresources stay with the engine's mixed-content policy, and the addresses
  the Omnibar sends over plain HTTP, a Local-development site's and a name without a dot given a
  port, are left alone. Where the upgraded address cannot be loaded, where the site sends the load
  back to plain HTTP, or where a form would be sent to a plain address, Omaweb draws its own page
  over the engine's error naming the host and why, and offers going back, loading the plain address
  once, or using plain HTTP for that site in the Space for good. A Private window offers only the
  first two and remembers nothing. The remembered choice is kept with the site's permissions, so
  resetting them takes it back. A certificate the upgraded address cannot prove is the certificate
  interstitial's, as for any `https:` page. Site information says when a page arrived through an
  upgrade. The privacy section of Settings turns the mode off, and the choice survives a restart.
- Secure DNS is off by default and browser-wide. Off, names are looked up by the system's resolver.
  On, every name is looked up over DNS-over-HTTPS by the resolver the reader chose in the privacy
  section of Settings: Quad9, Cloudflare, Mullvad, or an `https:` address the reader types, which is
  refused before it is saved if it is anything else. It is secure mode only, so a resolver that
  cannot be reached fails the lookup rather than falling back to the system in the clear, and Site
  information names the resolver that could not find a page. A change applies at once, and the
  choice survives a restart. Spaces and Private windows follow it alike. Omaweb checks a typed
  address before saving it, and the engine checks it again; should the engine still refuse one,
  names go back to the system's resolver and Settings says so in the urgent colour rather than
  quietly.
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
  facts the adapter can prove.
- Site information is a card about 400 px wide that floats from the address over the page edge, on
  an opaque ground, with every corner on Omarchy's corner radius and 12 px inside every edge. A band
  in the verdict's colour leads it: a badge and "Connection is secure", "Not secure", the
  certificate error, or "Omaweb's own page" for the Start page, with a second line that qualifies it
  (encrypted, upgraded by HTTPS-only mode, waived for this session, readable on the way, nothing
  loaded, or the Secure DNS resolver that could not find the site). The badge centres on the verdict
  and its second line together. The site's host follows, then four tiles, each a value, a label and
  `›`: Certificate with its issuer (over TLS only), Blocked with the request count, Cookies and site
  data with the cookie count the engine holds for the site, and Third parties with the count
  allowed. Then a row for each permission the site asked for or the reader decided in this Space,
  with an Allow, Ask or Block dropdown that is stored for the Space at once and makes the engine
  forget its own record, and a site that never asked has none. Clear site data and Reset permissions
  close it, side by side, each confirmed in the centred dialog first.
- A tile opens its detail inside the card, which keeps its width and place, with "‹ <host>" to go
  back. Enter opens the tile the arrows are on, Escape steps back to the top, and Escape again
  closes the card. The lock opens it at its top, the shield at the blocked requests, and the "Site
  information" command, `Primary+Shift+L` by default, at its top. A collapsed sidebar peeks while
  the card is open.
- The certificate detail shows the chain from the site's own certificate to the trust anchor, one
  selectable entry per certificate, with subject, issuer, validity period, SHA-256 fingerprint, and
  subject alternative names, each copyable. The certificate interstitial opens the same detail for
  the certificate it refused, so a reader can judge a Local-development site's certificate before
  letting it through, and Escape hands the keyboard back to the question. A Private window shows it
  like any other window. The chain a page arrived over comes from the engine patch that reports it
  ([ADR 0054](../adr/0054-read-a-pages-certificate-from-the-engine.md)); an engine without it shows
  only the chain a failure was raised for.
- Site information states facts about the site and never what this build's engine cannot do. The
  privacy section of Settings states each shortfall, and only on an engine that has it: that it
  cannot report a certificate failure, cannot show the certificate a page arrived over, cannot
  refuse a third party, keeps no site data on disk, or is not blocking insecure content.
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
- Site data is cleared at whichever scope the engine can reach, and the dialog names which that is.
  Site information's Clear site data empties one origin's storage, databases, caches and service
  workers from inside its own page. Cookies and cache go for a whole Space, from Settings' Browsing
  data only, and the cookies detail names their size as the Space's.
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
- Omaweb offers no custom user agent, globally or per site. A changed user agent makes a reader
  easier to tell apart, and mostly works around sites that turn away browsers other than Chrome.

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

On the Qt engine, a procedural cosmetic rule written against a site hides, restyles or removes what
it matches, in the main frame and in every subframe, and keeps doing so as the page changes
([ADR 0052](../adr/0052-apply-procedural-cosmetic-filters.md)). A rule change reaches an open page
in place, except that an element a `:remove()` rule deleted returns only when the page reloads. A
generic procedural rule, and one using an operator the pinned parser lacks, is reported in a
category of its own. A `$specifichide` exception takes a site's own cosmetic rules away, procedural
ones included, and an `$elemhide` exception its generic ones as well; scriptlets are not cosmetic
rules and still run.

Omaweb does not claim full uBlock Origin compatibility. Response rewriting, content security
policies, HTML filtering and dynamic rules are outside the first contract. A subscribed list keeps
the rules this contract does parse; Settings reports what each list contributed and what it skipped.

On the Qt engine Omaweb ships, a subresource request the lists let through is checked again under
each name in its host's CNAME chain, so a tracker served from a site's own subdomain is refused as
the tracker it is. With Secure DNS on the engine sees the whole chain; through the system resolver
it sees the chain's last name only. The engine resolves the host through the Space's own resolver,
and makes no lookup for a main-frame navigation, an IP literal, or a request behind a proxy. A name
on the request's own site is not checked. A `$cname` rule that turns this off for a host is reported
unsupported, because the pinned parser does not read it. An Omaweb built against another engine does
not uncloak, and Settings says so ([ADR 0050](../adr/0050-uncloak-cname-trackers-in-the-engine.md)).

Site information lists the requests Content blocking refused on the page beside the Refusal tally,
and names the name from the CNAME chain an uncloaked one matched.

A rule change applies to the requests a page makes after it: a list update, a user rule, or a site
whose blocking was switched on or off. The first reload of a tab after a rule change reads the page
from the network rather than the cache, so an image or script the page already held is asked for
again under the new rules, and the reload after that keeps the cache again. A page that asks again,
without a reload, for a resource it already holds is given the copy it holds, and the change reaches
that resource at the page's next load.

## Daily-driver non-goals

- Bookmarks
- Integrated terminal
- Web extensions other than a Known extension
- Browser Account system
- Password management by Omaweb itself
- Browser data import
- Installed web applications
- Additional ordinary browser windows or detached tabs
- Translation, Reader mode, View source, or spellchecking
- A custom user agent, globally or per site: it makes a reader easier to fingerprint and mostly
  works around sites that turn away browsers other than Chrome
- Print preview: the desktop print dialog owns preview and PDF output
- DRM streaming and proprietary media codecs before distribution review
- Picture-in-picture on the Qt engine: QtWebEngine turns it off in every page and offers no way to
  draw a video outside its page ([research](../research/picture-in-picture.md))
- USB, Bluetooth, serial, or MIDI permissions
- macOS distribution
- AppImage or Flatpak packaging
