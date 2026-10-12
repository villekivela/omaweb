# Draw favicons afresh in each run

Chromium keeps a favicon database, `Favicons`, in each Engine profile: the drawing of each icon it
has shown, and the address of every page that showed one. A later visit finds the page's icon there
and shows that drawing without asking the site again. Only a reload marks it out of date.

An SVG favicon that follows `prefers-color-scheme` is drawn under the colour scheme of the visit
that drew it. chatgpt.com's is black under a light scheme and white under a dark one, so a Space
that met it while pages were told the scheme was light kept the black mark under a dark theme, run
after run. That happened before Omaweb told the engine its own scheme (#346), and under any light
theme since.

Omaweb now removes `Favicons` and its journal from every Space's Engine profile at startup, before
any profile is built, so no engine has the files open. A launch that could not hand its address to
the browser already running leaves them alone. The first visit to a site in a run asks for its icon
again, from the HTTP cache where the site allows it, and the engine draws it under that run's
scheme. Until then a tab shows the copy its Space keeps
([ADR 0055](0055-keep-favicons-in-the-space-that-loaded-them.md)). The database also held the
address of every page whose icon it drew, out of reach of deleting History, and now goes at the next
start.

Removing it only when the scheme differs from the last run's would keep the engine's drawings for
longer. That would need a record of the scheme each run started under. Every run starting without
the database is simpler, and it also takes the page addresses with it.

## When the theme changes while Omaweb runs

Omaweb tells the engine its theme's colour scheme, and tells it again when the theme changes (#346).
The engine draws an icon once per address while it runs, so before #700 a page on show kept the icon
drawn under the earlier scheme until the next start.

A patch in the engine series now follows the change. The engine tells every page the new scheme and
has each page on show draw its icon again under it. The page reports no icon and then the same
address, and Omaweb's tab draws the new drawing rather than a cached one, and stores it
([ADR 0055](0055-keep-favicons-in-the-space-that-loaded-them.md)).

The engine also marks every drawing the profile keeps out of date, as a reload does for one page,
rather than deleting them: Chromium's favicon service marks a range of drawings out of date in one
call and has none that deletes them all. A page not on show then asks for its icon again on its next
visit and draws it under the new scheme, showing the earlier drawing for that moment. Until that
visit, its tab and an Omnibar row draw the copy the Space keeps. An engine without the patch keeps
the behaviour before #700.

## When a page offers a raster icon beside an SVG one

Drawing an SVG under the current scheme helps only if the engine shows the SVG. chatgpt.com declares
a black ICO for any size ahead of its SVG. Chromium ranks an icon declared for any size, or for
none, as ideal before fetching it. After fetching, it prefers a raster with exact 16 and 32 pixel
frames over the single drawing it makes of an SVG. The ICO won, and the tab showed the black mark
under a dark theme, on a fresh load as well as after a change (#700). Chromium's own ranking is the
same on its main branch.

A second patch in the engine series ranks a page's SVG icon above its raster ones and takes the SVG
once it is drawn. A candidate carries no type from its `<link>`, so the engine knows an SVG by its
address: a path ending in `.svg`, or a `data:` URL of `image/svg+xml`. An SVG that cannot be fetched
leaves the page its raster icon. The page sees nothing of this. A script in the page could have
hidden the raster icons from the engine instead, but only by changing the page's own links.

See #698.
