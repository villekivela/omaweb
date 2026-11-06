# Ladybird adapter

The Ladybird adapter belongs to a separately configured build and never enters the default Qt
dependency graph. Its first implementation may use bitmap painting only for functional checks;
daily-driver qualification requires a Qt Quick texture path and preservation of Ladybird's upstream
helper-process sandbox.

The engine contract requires a page the shell has stopped for want of a reader to keep its document
and everything it holds, so clearing the decision continues the page rather than loading it again.
An adapter that cannot stop a page keeps running it, and answers for nothing else.

The engine contract has a `PageDarkening` capability for a page drawn dark by the engine when the
shell sets `darkenPage`. The Qt engine reports it, through Chromium's forced dark mode. Ladybird has
no such setting, so its capabilities lack it, `darkenPage` is ignored, Settings says "This engine
cannot darken a page" beside the switch and Site information offers no row for a site.
