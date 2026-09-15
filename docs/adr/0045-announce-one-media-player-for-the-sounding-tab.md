# Announce one media player for the sounding tab

Omaweb exports `org.mpris.MediaPlayer2.omaweb` on the session bus while a tab is making sound, and
withdraws it when none is. The player follows the Sounding tab rather than standing for a tab of its
own.

One player, not one per tab. A player per tab is legal MPRIS and unreadable on a desktop: the bar
lists a row for every tab that has ever played, and a media key has a choice to make rather than a
target. The Sounding tab is the last tab playing, so the keys go somewhere a reader can predict, and
the tab the player is following is named in `xesam:title` so the entry says which tab it is. A
paused tab yields to one that is playing, whichever started first: a reader who pauses a video and
leaves music running in another tab means the music.

Metadata comes from the page rather than from the engine. Qt reports whether a page is audible and
whether it is muted, and reports nothing about `navigator.mediaSession`. First-party script in the
page reads what the page declares and reports it the way the other page facts already travel, and
the same script invokes the page's own action handlers for the commands the desktop sends. A page
that registered no handler is answered by the element that is sounding. This is Omaweb's own script
rather than a third-party one, so [ADR 0025](0025-run-only-vendored-scriptlets.md) is untouched. An
engine that cannot report a page's media session reports that it knows none, and Omaweb announces
the tab's title rather than inventing one.

Artwork is passed through as the address the page named. Omaweb neither fetches it nor caches it, so
an address a site chose stays off Omaweb's network path and whoever draws the picture asks for it
themselves.

A Private window exports playback and the controls and nothing that says what is playing.
`PlaybackStatus`, `CanPlay` and the rest are there, so the media key still works; `xesam:title`,
`xesam:artist` and `mpris:artUrl` are absent. [ADR 0012](0012-isolate-private-browsing-by-window.md)
keeps a Private window out of everything the Spaces can see, and the session bus is read by every
application on the desktop, which is the same kind of exposure.

The export is a platform concern, beside the notification and print paths under `src/platform/`.
Core owns which tab sounds and what it declares, in `SoundingTabs`, which every window reports into
and which has one answer for the process. The platform layer puts that answer on the bus. Neither
half names a web engine.

A paused page keeps the player. Withdrawing it on a pause would take away the player the next key
press needs, and the reader would have to find the tab to start it again. What withdraws the player
is a page that declares its media session over, a tab that goes quiet having declared nothing, a tab
whose page is Frozen, and a tab that closes.

The interface is answered whole, including the parts Omaweb has no answer for. MPRIS names `Stop`,
`Volume`, `Position` and `Seeked` as part of the player, and a consumer that reads a player's
properties in one call reads all of them, so they are exported rather than omitted: `Volume` is a
constant, `Position` is zero, `CanSeek` is false, and `Seeked` is never emitted. Exporting them is
not implementing them. Per-tab volume stays out, because the desktop's own mixer already has one
slider per process and a second answer is worse than none.

Two Omawebs on one bus do not fight over the name. The second takes
`org.mpris.MediaPlayer2.omaweb.instance<pid>`, which MPRIS provides for exactly this, and goes back
to the plain name once it is free rather than keeping the instance name for the rest of the process.

What was rejected:

- **A player per tab.** Above.
- **Following the active tab rather than the tab that started last.** A reader watching a video and
  then reading something else in another tab would find the media key pointed at a page playing
  nothing.
- **`Raise` and `Quit`.** A media widget is not where a reader quits a browser, and the window a
  Sounding tab belongs to is not always a window there is one of. Both are exported as refusals:
  `CanQuit` and `CanRaise` are false.
- **`TrackList` and `Playlists`.** Neither has a meaning for a browser tab.

See [#271](https://github.com/villekivela/omaweb/issues/271).
