# Per-site zoom

Omaweb does not remember zoom per site. Zoom belongs to a tab: it survives navigation and restart
with that tab, and a new tab starts at 100 percent, wherever it goes.

## Why this is out of scope

Chrome and Firefox key zoom by host, so zooming one GitHub page zooms every GitHub tab, in every
window, and the next visit comes back zoomed. That is a preference the reader never stated, applied
to tabs they did not touch, and it has to live somewhere: an origin-keyed store per Space that
Browsing data would have to clear and Sync would have to decide about. Omaweb's requirements state
the opposite on purpose: zoom and mute "belong to a tab, survive navigation and restart, and never
become origin-wide preferences".

The usual reason to want per-site zoom is that every page comes up too small. The Page scale answers
that for every site at once
([ADR 0067](../docs/adr/0067-scale-pages-by-the-desktops-text-scaling.md)): it follows the desktop's
text scaling until the reader sets it, and a tab's zoom is relative to it. What remains, a single
site that draws its type smaller than the rest, is one keypress in the tab that shows it.

## Prior requests

- #709: "Set a default page zoom, and follow the desktop's text scaling" asked for per-site
  overrides "ideally, as in Chrome". Its Page scale was accepted and its per-site part declined.
