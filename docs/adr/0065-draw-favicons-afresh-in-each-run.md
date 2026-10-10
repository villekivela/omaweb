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

A theme changed while Omaweb runs still leaves the icons drawn before the change until the next
start. The engine draws an icon once per address while it runs, a reload does not draw it again for
the view on show, and the image provider answers an address from the first view that holds it.
Drawing again when the scheme changes takes a patch to the engine's favicon driver. Omaweb's own
copy of a page's icon is replaced when the page next reports one, so a tab whose page has not loaded
in this run, and an Omnibar row, draw the earlier copy until then.

See #698.
