# Scale pages by the desktop's text scaling, beneath a tab's zoom

A reader who enlarges type across the desktop instead of scaling the output, as Omarchy's
`omarchy-display-text-size` does, expects pages and Omaweb's own type to come up larger with the
rest of their applications (#709). Omaweb therefore draws every page at the Page scale times its
tab's zoom. The Page scale is the Desktop text scaling until the reader sets one in Settings, kept
as the `page-scale` key in `settings.json` and absent while it follows the desktop. The interface
font size follows the same factor while the reader has not set it: the theme's size times the
factor, rounded to a whole pixel.

The Page scale sits beneath zoom rather than being the zoom new tabs start at. Zoom keeps its ladder
and its meaning: 100 percent is the page at the Page scale, zoom-reset returns there, tabs stored
before the Page scale existed need no migration, and a change of scale reaches every open page at
once. A starting zoom was rejected because the desktop's factors fall between the ladder's rungs,
1.3636 for Omarchy's 16 px, and because tabs opened before a change would keep the old size.
Chromium on Linux reaches the same result by taking the factor as its device scale. Zoom stays the
tab's own and never a site's ([per-site zoom](../../.out-of-scope/per-site-zoom.md)).

The factor is `text-scaling-factor` in `org.gnome.desktop.interface`, read through the settings
portal and followed live, the route reduced motion already takes. Omarchy's text size command writes
it, and so does any reader who sets it directly. Reading `base-size` from Omarchy's `shell.toml` was
rejected because it reaches only Omarchy and needs a file watch for a value the portal already
carries. Where no portal answers, the factor is 1.

QtWebEngine draws a page between 25 and 500 percent. Zoom therefore stops at the last rung whose
product with the Page scale fits, and a tab whose stored zoom no longer fits after the scale changes
is drawn at the limit and keeps its zoom for when the scale comes back down.
