# Stop an away Space's pages instead of taking them

Omaweb freezes the pages of a Space that is not on show rather than discarding their renderers. A
frozen page keeps its document, its process and everything it holds, and runs no timers, animations
or script, so selecting the Space again finds each page where it was left rather than loading it
again. This replaces the memory policy recorded in
[ADR 0003](0003-isolate-browsing-identities-by-space.md), which discarded every renderer of a Space
being put away and restored that Space from its store on the way back. ADR 0003 stands in every
other respect: a Space is still an isolated browsing identity, and nothing here crosses one.

The trade is reload churn against resident memory. Discarding spends a page load, a network round
trip and whatever the page was holding every time the reader comes back, and coming back is ordinary
work rather than an occasional act, so the cost lands on the reader repeatedly and lands hardest on
the page they were reading. Freezing spends memory instead, bounded by what the reader actually did:
one renderer per tab they opened in that Space, held until the tab or the Space is closed. It is not
one per tab in the session, because a tab of a restored Space that has never been selected has no
engine to keep and gets one only when it is first read.

Which pages run is one rule in the engine host, applied to every engine it owns, so a background tab
of the Space on show and a page of a Space that is away are answered the same way. Two pages are
exempt: the tab an inspector is attached to, which
[ADR 0027](0027-use-engine-provided-developer-tools.md) keeps active for as long as it is attached
and which the engine refuses to freeze in any case, and a tab making sound, which the reader is
still listening to. Retention is now a separate question from existence. The tabs the core names at
suspension, a Pinned tab marked Keep active and the inspected tab, are the pages Omaweb identifies,
lists with the memory they hold, and starts even in a Space that has never been selected. They are
not the only pages that survive a Space switch.

Discarding remains what happens when a tab or a Space is closed and when the core stops retaining a
tab. Nothing here decides what to do under memory pressure. Discarding a frozen page is still
available, and choosing when to spend it is a separate decision.
