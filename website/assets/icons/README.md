# Icons

`omaweb.svg` is the wordmark as the editor that made it saved it, drawn black. It is the source the
inline wordmark in every page was copied from, not a file the page loads: the page's copy carries
the same two paths and is drawn in `currentColor`, so it follows whatever palette the page is in. A
change to the drawing is a change here first, then to each page's copy.

The favicon lives at `website/favicon.svg`: the application icon in black and white, edited by hand.
It is the same file in every theme: the page could redraw it only as a `data:` URL, and Omaweb's
engine shows no icon for one of those.
