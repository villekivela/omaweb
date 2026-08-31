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

Three seams carry the adaptation instead; the third, `KitTheme`, arrived with 0018.
`tanto-quickshell-shim` registers the `Quickshell` and `Quickshell.Io` types the
kit's singletons import — an environment lookup, a watched file, and a short-lived
process run for its output. The kit's
layer-shell and Hyprland surfaces stay out of scope, and so do the components that
need them: `Panel`, `PopupCard`, `KeyboardPanel`, `MultiSelect`, `BarIconButton`,
and `SpeedTestOverlay`. QML in `src/ui` adapts each component to Tanto's call sites,
keeping Tanto's property names and the accessibility annotations the kit does not
carry, so adopting a component does not ripple through the surfaces that use it.

The shim also pins the Qt Quick Controls style to Basic. The kit's `TextField`
inherits Qt's `TextField` and replaces its `background`, which a native style refuses:
on macOS the field renders as an Aqua box and only a runtime warning says so. Basic is
what the Omarchy shell itself draws on, so pinning it is what makes the vendored
components look like themselves. The setting is process-wide, so Tanto's own Qt Quick
Controls surfaces — the settings `ScrollView` among them — are drawn by Basic too, which
is the intent rather than a side effect: a browser whose chrome is its own should not
have one scroll bar wearing the platform's clothes.

`ThemeController` remains the single source of truth for the palette. Kit components
read `qs.Commons` singletons that would otherwise resolve colours from an Omarchy
theme on disk, so Tanto's adapters pass colour per instance from the palette their
call sites already receive. Whether Tanto later drives those singletons from
`ThemeController` — dropping the per-instance overrides and the prop drilling along
with them — is a separate decision. 0018 makes it: the singletons are driven, and
the per-instance overrides stay, because a Private window's palette differs from a
Main window's inside one engine.

Type is not drilled the same way. Tanto had its own `Typography` object resolving a
family and a size scale from the palette; the kit's `Style.font` is the same idea with
a fuller scale, so `Typography` is gone and every Tanto surface reads `Style.font`
directly. Until `ThemeController` drives `Style`, that means the type scale follows the
kit's defaults rather than Tanto's palette — the visible cost of having one scale
instead of two, paid deliberately. 0018 stops paying it.

Where the kit and Tanto disagree on appearance, the kit wins: emphasis is a tinted,
accent-bordered button rather than a filled block, a checked switch does not fill its
track with the accent, and a section label is neither letter-spaced nor upper-cased.
Tanto keeps its own control only where the kit has no equivalent at all — `SettingRow`,
which carries arbitrary content beside a title and a note, and `MultilineField`, since
the kit's `TextField` is single-line. Both are built from `qs.Commons` tokens rather
than a second set of values, and neither forks a vendored file.
