# Interface direction: motion with an origin

This document records the interface direction settled on 2026-09-13. The chrome itself, the vertical
sidebar, the Omnibar, the Start page, the settings and history sheets, is done and stays. The
direction is about where things come from and how they move, which is where the browser's identity
lives now that its layout is settled. It is a design intent, not an implementation contract; the
tuning numbers below are the ones shipped and may move.

Two directions were prototyped and rejected before this one: a Neovim-shaped chrome with a leader
key, modes, a status line and pickers, and three looks that left the browser's shape behind. Both
are in the history of the branch that carried them and are out of the running. A modal keyboard, in
particular, is out: every command is a chord, as Omaweb's keyboard navigation already works.

## The rule

Nothing appears; everything comes from somewhere the reader can point to, and goes back there.
Motion is felt rather than watched: it says which way something went, and no more.

## Origins

- **The sidebar is the shelf.** It lives on the left edge and peeks from it. What belongs to it
  grows out of the row or control that asked for it, never out of the middle of the window.
- **Spaces are the row the footer draws.** Switching Space slides the outline along that row: the
  Space to the right arrives from the right as the one on show leaves to the left; the lit letter
  slides along the footer with it; the page arrives the same way, a little, from under the shelf.
  The leaving list is a picture taken while it was at rest, since the models belong to the next
  Space by the time the switch is heard.
- **Tabs are the list.** The page follows a tab switch the way the list reads: a tab further down
  arrives from below, one further up from above, and a pin further along its row from the side. The
  arriving page alone moves, over the page's own ground, and never over the seam.
- **The address field is where the browser is asked.** The command panel starts on the field and
  grows to its resting place. With the sidebar hidden there is no field, and the panel arrives from
  just above its place instead. Site information unfolds from the address it reports on.
- **Sheets lift.** The Start page, settings and history are one kind of thing: a sheet that lifts
  from a little below its place. The download detail rises from its mark the same way.
- **Notices belong to the edge.** A page notice comes down from the top edge as it fades in and goes
  back up as it fades out.

## Motion language

- Three durations, no others: 120 ms for anything structural (the sidebar's own ease), 160 to 180 ms
  for a page nudge, a sheet or a panel, 240 ms for a Space switch, the biggest move. Out-cubic
  arriving.
- Small distances: 10 px for a page nudge, 8 px for a panel or a notice, 24 px for a sheet, 32 px
  for the page on a Space switch. Only the sidebar's list travels its own width, because it is the
  move.
- Idle chrome draws no frames. The one thing that animates unprompted is the page loading indicator,
  while a page loads.
- One switch refuses all of it. The reader's refusal of the sidebar ease refuses every movement
  here, which is also the reduced-motion answer. The setting is still named for the sidebar and
  wants renaming once the direction holds.

## Cost

A Space switch costs one offscreen render of the sidebar's list region whenever the list settles
after a change, and nothing per frame at rest. No other movement keeps anything between uses. The
page is never drawn through a layer: it slides, and does not fade.

## Open

- Closing. Everything still closes at once. The rule says things go back where they came from, so
  the panel retreating to the field and the sheets dropping out are the same movements reversed.
- The tuning numbers above, once the direction has been lived with.
- A thawed tab's arrival. A page coming back from Frozen could arrive at 0.98 scale so that "was
  asleep" reads differently from "was live". Not tried.
