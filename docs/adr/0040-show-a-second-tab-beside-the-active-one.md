# Show a second tab beside the active one

A split puts two tabs of one Space on show at once, side by side, as one sidebar row. It is the
answer to reading a reference beside the page being worked on and to comparing two pages, and it is
built inside the window rather than as a second Omaweb window tiled by Hyprland, so it works the
same in a floating window and keeps one sidebar for the Space.

The active tab stays the one thing the chrome answers for. The address, find bar, notices, prompt
bars, loading indicator, zoom and lock, and every command that names "the tab on show" belong to the
focused pane, exactly as before a split existed. The tab beside shows its page and nothing else;
what it wants to say waits until it is focused. This is what keeps a split from being a second
chrome: the page area learns to hold two engines, and nothing above it changes shape. Site
fullscreen from either pane takes the window, and Developer tools attach to the inspected tab in
whichever pane it sits.

[ADR 0033](0033-stop-an-away-spaces-pages-instead-of-taking-them.md) ties running to being seen, and
that rule is what makes a split cheap: both panes are visible, so both run, and no exemption is
added. A split is recorded with its Space's tabs in the session store and comes back after a
restart, because a restored Space that has lost a row is a surprise. It is not part of the Sync
projection; [ADR 0006](0006-limit-the-first-sync-contract.md) fixes what Sync carries, and the
pairing is not worth widening that contract for.

The boundaries are deliberate. Exactly two panes, side by side: a pair is a value, a layout tree is
a window manager. Both tabs belong to the Space on show, so a window never holds two browsing
identities and the sidebar never has to draw one. Pinned tabs are not paired, because every way of
allowing it has to say where the row goes and how Keep active and restore read across the pair. A
tab is in at most one split. The divider position lives in memory only. Reordering within a split
and directional focus across the sidebar, the panes and the Developer tools dock are separate
questions; the split offers one command to move focus to the tab beside, and the directional system
routes through it.

Rejected: a second window per split. Hyprland would tile and resize it for free, but a window owns a
Space's engines and the session store has no notion of a tab being in one window rather than
another, so the cheap-looking option is the one that reaches the store, and it does nothing in a
floating window.
