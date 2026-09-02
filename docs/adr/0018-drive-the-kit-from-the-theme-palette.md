# Drive the kit from the theme palette

The vendored Omarchy kit reads colour and type from the `qs.Commons` `Color` and
`Style` singletons, and those resolve an Omarchy theme from disk: `colors.toml`
and `shell.toml` under `~/.local/state/omarchy`, a machine-level
`~/.config/omarchy/shell.toml`, and `hyprctl getoption` for window rounding.
None of that is Omaweb's source of truth. Omaweb's theme palette is, so
`ThemeController` drives the foundational palette and the type both singletons
expose, and `src/ui/KitTheme.cpp` is the seam that pushes it in — the third seam
of the adaptation, after the Quickshell shim and the adapters in `src/ui`.

That is #11's second option, but only the half of it that survives contact:
the singletons are driven, and the `colors` properties on the adapters in
`src/ui` stay. A Private window's palette differs from a Main window's — a
different accent, a different window and sidebar — and both windows are
instances of `Main.qml` in one QML engine, so they share one set of singletons.
A process-wide singleton can carry one palette; it cannot carry the per-window
difference. The singletons therefore hold the ordinary palette, which is what a
kit component drawn without an adapter looks like and what `Style`'s own
convenience colours resolve against, and the adapters carry the window's own
colours on top. The prop drilling #11 hoped to remove is load-bearing, and only
type disappears from `src/ui`.

Type was the part that was unowned. #10 retired Omaweb's own `Typography`, which
left `Style.fontFamily` at the kit's default of `"monospace"` — a fontconfig
alias that does not exist on macOS, the platform `CONTEXT.md` says must stay
usable during development. Qt fell back to some mono face and logged a missing
family on every run. Populating the alias table cost about 140ms: the UI lab's
`--validate-qml` startup measured 0.58s before this change and 0.44s after, on
the macOS development machine. So the palette resolves the family before handing
it over: `theme.json` names the families it prefers, `ThemeController` picks the
first one the host actually has installed, and both `Style.fontFamily` and
`Style.resolvedFontFamily` get that concrete name. Omaweb never asks Qt for a
family it does not have. `theme.json`'s `font` block is `families` and `size` and
nothing else; the kit derives the rest of the scale from the base size, which is
the point of having one scale instead of two. `Style.spacing.*` follows the base
size as well, so the structural tokens `src/ui` already reads move with the
theme without Omaweb naming any of them.

Pushing once is not enough. Every one of the kit's own sources is asynchronous:
a watched `FileView` reload, a `shell.toml` that resets the whole of `Style` even
when it fails to load, a short-lived `hyprctl` run. Being the first writer is not
the same as being the authority, so `KitTheme` re-applies the palette whenever
one of those lands. That is a converging loop, not a fight: the seam writes only
on a difference, so once the values agree nothing writes again. Instantiating the
singletons from `main()` rather than waiting for the first QML that imports them
adds no work — the kit's lookups already ran during startup — it only moves them
a few milliseconds earlier.

What the seam does not drive stays the kit's. `Color.urgent` is one: Omaweb's
palette has no error role to map onto it, so the kit's own red stands until the
palette grows one. `Style.cornerRadius` is another — it mirrors Hyprland's
`decoration:rounding`, and a control that rounds like the rest of the desktop's
windows is the kit behaving as intended. So are the per-surface roles and
control-state tokens the kit parses out of `shell.toml`: a user's
`~/.config/omarchy/shell.toml` still tunes the fill and border alphas its
controls draw with. That is deliberate rather than overlooked — where the kit and
Omaweb disagree on appearance the kit wins (0017), and these are the kit's own
appearance rather than Omaweb's palette.

Letting the kit read the desktop's colours instead was the other candidate, and
it is rejected. `themePath()` already prefers
`~/.local/state/omarchy/current/theme/omaweb.json` on Linux, and
`integrations/omarchy/omaweb.json.tpl` already renders the desktop's own font
family into it, so a user's Omarchy theme reaches Omaweb's chrome through the
palette Omaweb owns — including the parts, like the Private-window separation and
the semantic opacities, that a kit reading `colors.toml` would skip. A second
source for the same values in the same process would buy nothing and would have
to be reconciled with the first.
