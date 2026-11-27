# Keep the Ladybird variant apart from the Qt browser

Refines [0001](0001-isolate-web-engines-by-build.md), which had both variants share one data root,
and [0008](0008-isolate-space-storage-on-disk.md)'s layout where one Space holds an engine profile
for each engine. The experimental Ladybird variant, `omaweb-ladybird`, keeps its own data root and
settings, so the two browsers never write the same file.

## What is decided

- **A data root and a config root of its own.** `omaweb-ladybird` reads `~/.config/omaweb-ladybird`
  and the application data directory of the application name `Omaweb-Ladybird`, beside the Qt
  browser's and never inside it. `EngineVariant` names both. Its Spaces, bookmarks, history and
  settings are its own. A Sync the reader turns on can carry bookmarks between the two.
- **Engine state is the engine's.** Cookies, logins, site storage and caches are never shared or
  translated with the Qt build ([0009](0009-separate-browser-policy-from-engine-mechanics.md)). Each
  Space has its own Ladybird profile directory, `spaces/<id>/engines/ladybird`.
- **Capabilities, not pretence.** Until Ladybird's adapter passes the engine contract for one, every
  capability but navigation and a persistent profile is reported off. Settings and Site information
  say so where the setting lives, as they do for Page darkening.
- **A build of its own.** `OMAWEB_BUILD_LADYBIRD` is off by default and only the `ladybird` preset
  turns it on. `src/engine/ladybird` is added to the build by that one `if`, so an edit to the Qt
  browser never configures or compiles Ladybird, and `omaweb-ladybird-isolated` fails if another
  preset enables it.
- **Experimental, said plainly.** Ladybird's alpha warns of crashes and no security. The sandbox
  stays intact ([0013](0013-preserve-engine-sandboxes-in-every-build.md)).

## Why not share the Qt browser's data root

A shared root makes an alpha engine one bug away from the reader's real profile, and two browsers
that run at once would write the same history and settings files. Separate roots cost the reader a
second set of Spaces, which Sync's bookmarks soften and which an experimental browser can afford.

## What ships later

The adapter, the Ladybird engine package, `omaweb-ladybird` and its launcher entry, and the install
script's `--ladybird` flag arrive with #980, so nothing installable reaches a reader before it runs
pages. The pin starts at Ladybird's alpha tag `2026.47.7`.
