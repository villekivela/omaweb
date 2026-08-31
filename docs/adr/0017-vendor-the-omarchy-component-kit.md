# Vendor the Omarchy component kit

Tanto's shared controls come from the Omarchy shell's QML kit rather than from a
second set of Tanto-owned equivalents. Both projects are QML on Qt 6, the kit
already covers what browser chrome needs — buttons, text inputs, toggles,
dropdowns, sliders, confirm dialogs, border and panel primitives — and Tanto
targets the Omarchy desktop, so matching that desktop's controls is a feature
rather than a coincidence. Omarchy is MIT licensed, which its `THIRD_PARTY_NOTICES.md`
entry records.

The kit is vendored, not depended on. `shell/Ui` and `shell/Commons` are copied
verbatim into `third_party/omarchy-shell/qs`, pinned to a commit by `MANIFEST.json`,
and refreshed by `scripts/sync_omarchy_ui.py`. Upstream is a directory inside a
whole-desktop repository on an active branch, so a submodule would pin an operating
system to obtain thirty-nine QML files, and a released package does not exist. The
`qs` directory level is not upstream's layout: the kit's module URIs are `qs.Commons`
and `qs.Ui`, so the copies live at the path those URIs resolve to.

No vendored file is ever edited. That is the property the whole arrangement buys: a
sync is a review of upstream's diff instead of a merge with local patches. `ctest`
fails on a local edit, an untracked file under the vendored tree, or a hash that
does not match the manifest, and the sync script refuses to overwrite an edited copy.

Two seams carry the adaptation instead. `tanto-quickshell-shim` registers the
`Quickshell` and `Quickshell.Io` types the kit's singletons import — an environment
lookup, a watched file, and a short-lived process run for its output. The kit's
layer-shell and Hyprland surfaces stay out of scope, and so do the components that
need them: `Panel`, `PopupCard`, `KeyboardPanel`, `MultiSelect`, `BarIconButton`,
and `SpeedTestOverlay`. QML in `src/ui` adapts each component to Tanto's call sites,
keeping Tanto's property names and the accessibility annotations the kit does not
carry, so adopting a component does not ripple through the surfaces that use it.

`ThemeController` remains the single source of truth for the palette. Kit components
read `qs.Commons` singletons that would otherwise resolve colours from an Omarchy
theme on disk, so Tanto's adapters pass colour and typography per instance from the
palette their call sites already receive. Whether Tanto later drives those singletons
from `ThemeController` — dropping the per-instance overrides and the prop drilling
along with them — is a separate decision, taken once more than one component is
adapted.
