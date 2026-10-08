# Omarchy shell component kit (vendored)

`qs/Ui` and `qs/Commons` are copies of [`shell/Ui`][ui] and [`shell/Commons`][commons]
from [omacom/omarchy][repo], pinned by `MANIFEST.json`. Omarchy is MIT licensed;
see `THIRD_PARTY_NOTICES.md`.

Omaweb and the Omarchy shell are both QML on Qt 6, so the kit is usable directly —
`Button`, `TextField`, `Toggle`, `Dropdown`, `ConfirmDialog`, the border and panel
primitives — instead of Omaweb growing a second set of the same controls.

## Nothing here is edited

Every file is byte-identical to upstream, which is the whole point: a sync is a
review of upstream's diff rather than a merge. `ctest` fails on any local edit
(`omaweb-vendored-omarchy-ui`), and so does the sync script.

Two things make an unedited copy work inside Omaweb:

- **`src/ui/quickshell-shim/`** supplies the `Quickshell`, `Quickshell.Hyprland`
  and `Quickshell.Io` types the kit imports — an environment lookup, a watched
  file, a short-lived process, and the `Hyprland.rawEvent` signal `Style`
  connects to, declared and never emitted. Its own qmldir for each URI goes to
  the front of the engine's import path, so the real Quickshell that Omarchy
  installs into Qt's qml directory does not claim the kit's imports instead. The
  kit's layer-shell and Hyprland surfaces are out of scope, and so are the
  components that need them (`Panel`, `PopupCard`, `KeyboardPanel`,
  `MultiSelect`, `BarIconButton`, `SpeedTestOverlay`). Upstream removed
  `BackgroundVideo`, the kit's only QtMultimedia import, so `BackgroundMedia`
  draws stills and no kit component needs a Qt module Omaweb does not link.
- **`src/ui/*.qml`** adapts components to Omaweb's call sites — keeping Omaweb's
  property names and its accessibility annotations, with a kit component
  underneath.

The `qs` directory level is not upstream's layout: the kit's QML module URIs are
`qs.Commons` and `qs.Ui`, so the copies live at the path those URIs resolve to.
The vendor root is on the QML import path in source builds and lands under
`qrc:/qt/qml` in resource builds.

## Syncing

```sh
scripts/sync_omarchy_ui.py --verify          # the local tree matches the manifest
scripts/sync_omarchy_ui.py --check-upstream  # what changed on quattro since the pin
scripts/sync_omarchy_ui.py --sync --ref <sha>
```

`quattro` is an active branch, so a sync is a deliberate act: move the pin, read
the diff, and run `ctest --preset dev` — `omaweb-qml-load` instantiates the
adapted components, so an upstream API change fails there rather than in a panel
nobody opened.

The pin is not on `quattro` yet. Qt 6.12 added a `Color` singleton to `QtQuick`,
which hides the kit's `qs.Commons` `Color` in every file that imports `QtQuick`
(#659). Upstream's fix, [omacom/omarchy#14511][fix], renames it `ShellColor`,
and the pin is that pull request's head, one commit on top of `quattro`. Until
the rename reaches `quattro`, the drift check lists the renamed files. Sync back
to `quattro` once it has the rename.

[repo]: https://github.com/omacom/omarchy
[ui]: https://github.com/omacom/omarchy/tree/quattro/shell/Ui
[commons]: https://github.com/omacom/omarchy/tree/quattro/shell/Commons
[fix]: https://github.com/omacom/omarchy/pull/14511
