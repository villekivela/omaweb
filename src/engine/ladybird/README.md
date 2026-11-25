# Ladybird adapter

The Ladybird adapter belongs to a separately configured build and never enters the default Qt
dependency graph. Its first implementation may use bitmap painting only for functional checks;
daily-driver qualification requires a Qt Quick texture path and preservation of Ladybird's upstream
helper-process sandbox.

The engine contract requires a page the shell has stopped for want of a reader to keep its document
and everything it holds, so clearing the decision continues the page rather than loading it again.
An adapter that cannot stop a page keeps running it, and answers for nothing else.

The engine contract has a `PageDarkening` capability for a page drawn dark by the engine when the
shell sets `darkenPage`. The Qt engine reports it. Ladybird has no such setting, so its capabilities
lack it, `darkenPage` is ignored, Settings says "This engine cannot darken a page" beside the switch
and Site information offers no row for a site.

To gain it, LibWeb needs a hook at paint time, where the Qt engine darkens, rather than a style
rewrite, so a page's computed styles and `prefers-color-scheme` keep reading what the page wrote.
The hook has to:

- Hand each colour to one function with the role it is painted in: background, text, border, SVG
  fill or stroke, selection, or gradient stop. The Qt engine maps a colour differently for each
  role, toward a ground colour and a text colour the shell names, so a white ground and white text
  land in different places.
- Give text the colour behind it, so text can be held at 4.5:1 against it.
- Leave images alone, or classify them, and leave a canvas as drawn.
- Turn on and off per page, and take a change of the two colours on a page already showing, without
  a reload, as `darkenPage` and the Browser theme change.
- Paint from the first frame of a document, so a darkened page shows no white while it loads.
