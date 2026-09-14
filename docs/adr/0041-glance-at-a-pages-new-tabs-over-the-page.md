# Glance at a page's new tabs over the page

A link that asks for a new tab opens a Glance: the page drawn over the tab it came from, to be read
and closed, or kept as a tab with one command. It answers the reader who follows a link for a look,
which is most readers most of the time, and whose outline otherwise fills with tabs that were never
meant to stay. A tab with Omaweb's outline is a row the reader is asked to manage, and the Glance is
the page without the row.

A Glance is not a tab, and this is the decision the rest follows from. The core is not told about
it: nothing lists it, the session store does not record it, and the Sync projection
([ADR 0006](0006-limit-the-first-sync-contract.md)) is not widened for it. The window builds its
engine the way the host builds a tab's engine, on the Space's engine profile and with the same
configuration, and draws it in the Glance rather than in the host, so it is a page of the Space that
answers for no tab. The engine host keeps its invariant that every engine it holds answers for a tab
that exists, because the Glance's engine is not the host's until the reader keeps it, at which point
the host adopts the engine under a new blank tab and reports its page state once by hand. Keeping is
a reparenting rather than a reload, so history, scroll and form state come with the page.

The chrome keeps answering for the tab beneath, as it does for the tab beside in a split
([ADR 0040](0040-show-a-second-tab-beside-the-active-one.md)). The address trigger, find bar, page
notice, loading indicator, zoom and the page commands are the active tab's, and the Glance names its
own page and address in its own head. What its page asks of the browser is answered as the page on
show is answered, since it is in front: Site permissions, certificate failures, JavaScript prompts,
file pickers and the page context menu all reach the reader. Its visits are recorded in the Space's
history, because the reader read them; the tab path records visits off the tab's page report, and a
Glance with no tab records them itself. A Private window records none, as for every page in it.

A Glance stands over one tab and ends with it. A tab or Space switch, the tab closing, and settings
or history taking the page area all end it, and so do `Escape`, the close-tab command, its close
button and a click outside its panel. `Escape` is a window shortcut rather than a keymap binding,
for the reason site fullscreen's is: a reader looking at a page they did not ask for must not need
their keymap to get back. One Glance stands at a time; a second request from the page beneath
replaces the first, and a request from inside a Glance opens a tab, which ends the Glance because
the tab on show changes. A page the reader cannot see, a Keep active tab say, gets a tab as before,
since it has nothing to stand a Glance over.

The Glance comes from the link. The engine's new-window request does not say where on the page it
was asked for, so the engine adapter reports where the page was last pressed: a script on every main
frame names the link or control under a click, or the point itself, and the view keeps it in its own
coordinates (`pressOrigin`), forgotten on the next navigation. The window reads it when a page asks
for a window and the Glance grows from that rectangle to its place, and back into it when it closes.
A press in a subframe is not reported, since its coordinates are the frame's, and such a Glance
lifts as a sheet instead.

Only a page-initiated new-tab request glances. A background-tab request is the reader asking for a
tab behind, by middle click or the background link hint, and stays one; a dialog-shaped request
stays an Auxiliary window; and Content blocking's `$popup` refusal runs first, unchanged. The
setting that turns the Glance off is local and not synced: it is a way of reading rather than
configuration worth carrying between installations, and the Sync contract is not widened for it.

Rejected: a Glance as a tab the outline hides. The core would have to learn a tab kind that is
listed nowhere, saved nowhere and synced nowhere, and every rule about tabs, retention, freezing,
restore, reopen, would have to say what it means for one; the outline would still change shape when
the active tab did. Rejected too: promoting by reloading the address in a new tab. It is simpler,
and it loses what the reader did in the Glance, which is the one thing they asked to keep.
