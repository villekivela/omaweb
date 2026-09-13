# Interface direction: a TUI for the web

This document records the interface direction settled on 2026-09-13. It is a design intent, not an
implementation contract, and the prototype that follows it may revise any line here. Where it
contradicts a decision under `docs/adr/`, the decision is to be revisited rather than quietly
broken; ADR 0017 and 0018 are the ones it contradicts.

## Intent

Omaweb is a browser for people who live in a tiling window manager and drive their desktop from the
keyboard. Its chrome takes its cues from the modern rich terminal interfaces those people already
use, the Neovim ecosystem above all, and from the 80s terminal aesthetic ricing culture draws on. It
blends into the Omarchy desktop and stays out of the way until summoned.

## What the desktop owns

Four things stay Omarchy's and Omaweb never overrides them:

- The palette, all of it. Omaweb invents no colour of its own.
- The font family and base size.
- Corner rounding and gap size, as Hyprland reports them.
- Blur, which the compositor draws behind translucent surfaces.

Everything else is Omaweb's: control shapes, borders, density, motion, iconography and layout. The
aesthetic is carried by shape, motion, type treatment and composition, never by colour, so it must
read on every shipped Omarchy theme, the light ones included. Anything that only works on a dark
theme is a costume, not a direction.

## Resting state

The page plus one status line. Nothing else is persistent. Idle chrome draws no frames.

## Spatial model

Three layers, each with a fixed home. Motion always means "came from its home" or "went back to it",
and nothing ever moves sideways across the page.

- **Ground**: the page. It never moves.
- **Frame**: the status line along the bottom edge and the summoned sidebar along the left edge.
  Both are tiled, so they push the page rather than cover it. The sidebar slides in from its edge;
  the status line is always present and never animates.
- **Sky**: floats. Pickers, which-key, settings and prompts rise from the status line, because that
  is where the browser talks to the reader. Toasts fall from the top-right corner and leave the same
  way. Hint labels appear in place with no motion. Nothing persistent ever floats over the page.

Transitions only, 150 ms or shorter. The one exception is a load indicator, which may animate while
a page loads and goes still the instant it finishes.

## Status line

The status line is where the browser talks to the reader and, for the mouse user, the toolbar. It
follows lualine's left, centre and right segments:

- Left: the mode, the Space name and the tab position (`3/7`). The Space's colour grounds the mode
  segment and the window's border rule, so Hyprland's border and Omaweb's agree. A Private window
  reads `PRIVATE` on the Private accent in place of the Space name.
- Centre: the title at rest, with the origin beside it. The address shows only when it is the thing
  being acted on.
- Right: the Content blocking tally, an aggregate download segment (`↓ 2 43%`), the load state and
  the pending leader keys.

Every segment is clickable and opens the picker or panel it stands for: mode opens which-key, the
Space opens the Space picker, the tab position opens the tab picker, the tally opens Site
information, downloads opens the downloads picker.

Yes/no questions are asked inline in the status line with their keys shown. Find is `/` in the
status line with the match count in the right slot. Anything with choices or text entry rises from
the status line as a float instead.

## Modes

Four modes, shown in the status line:

- `NORMAL`: the browser has the keys.
- `PAGE`: the page has the keys, for forms, games and anything else that needs them.
- `HINT`: link labels are up.
- `PICK`: a picker is open.

Browser commands hang off a leader key. Chords stay for what the page needs. `Esc` leaves `PAGE`,
but a site that consumes `Esc` gets it first, so a modifier chord is the guaranteed exit. This is
ADR 0004's conflicting-key rule with a mode name on it.

## Mapping from the Neovim ecosystem

- **Telescope** becomes the picker: a bordered float with a prompt, a results list and a preview
  pane, over sources for tabs, Spaces, history, commands, downloads and settings.
- **Harpoon** becomes Pinned tabs: a small numbered list reached with `<leader>1..9`, edited in a
  float.
- **which-key** becomes the manual: a full-width panel rising from the status line that lists the
  bindings under the pressed prefix. The Start page is which-key with no prefix pressed, so a Space
  at rest shows the same panel.
- **lualine** becomes the status line.
- **nvim-notify** becomes toasts: notices, finished downloads and anything else that needs no
  answer.
- There is no bufferline. The status line carries the tab position and the picker carries the list.
  The floating strip retires; its contents move to the status line.

### Picker scope across Spaces

Spaces containerize but do not get in the way. The tab and Pinned tab sources list every Space's
tabs, grouped by Space, and selecting one switches Space. History search reads the Space on show by
default with a picker toggle that widens it to every Space. A Private window's pickers list only its
own tabs, and no picker outside a Private window lists anything from one. The history source is
absent in a Private window rather than empty. This changes `CONTEXT.md`'s History search entry and
wants an ADR when it lands.

### Picker preview

A Frozen tab's preview is a thumbnail of its frozen state, taken once when the tab freezes and again
only when it thaws and refreezes. A tab that never freezes, because it is Keep active, making sound
or inspected, is grabbed once when it leaves the screen, and the preview states the age of the
picture. The tab on show gets a text card of its title, origin, Space, last visit, blocking tally
and retained state, since a picture of what the reader is already looking at is noise. Grabs are
taken engine-side and scaled to preview size before they cross to the interface thread.

## Sidebar

A summoned tree in the manner of nvim-tree: Spaces as folders, tabs as files, Pinned tabs marked. It
is never the resting state.

## Settings and developer tools

Settings is a picker over settings whose value is edited in the preview pane, rising from the status
line the way LazyVim's panels rise from lualine. Multiline and dropdown settings open a small float
from their row. Developer tools stay a tiled split; the engine owns them and Omaweb only frames
them.

## Mouse

Every action the keyboard can reach, the mouse can reach: the status line's segments open their
pickers, and picker rows carry their actions on right-click, including reorder and pin. Hover may
highlight but never reveals something the keyboard user cannot see. Drag to reorder is deferred; it
does not break the model and can be added if asked for.

## Look

- Everything aligns to a character cell grid derived from the mono font. Borders and rules are drawn
  hairlines on that grid, not box-drawing glyphs, so they survive fonts without the block and
  fractional scale.
- Floats carry corner brackets rather than full borders.
- A faint ruled ground at separator strength sits behind floats and nowhere else.
- Things rise; nothing fades in place.
- The load indicator is angular, not a spinner.
- Picker titles are set with wider tracking. This is tried in the prototype and dropped if it hurts
  on light themes.

Hard no: glow, gradients, bloom, scanlines, chromatic aberration, and anything the theme cannot
recolour.

## Next step

Prototype the main window in this direction with the `/prototype` skill: the status line, the picker
over tabs across Spaces, which-key, and one toast, on at least one dark and one light Omarchy theme.
