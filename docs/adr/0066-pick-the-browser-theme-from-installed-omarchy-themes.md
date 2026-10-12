# Pick the browser theme from installed Omarchy themes

A reader may keep the desktop on one Omarchy theme and the browser on another, such as a light
browser on a dark desktop (#710). The browser theme is therefore either the desktop theme, which is
the default and today's behaviour, or any installed Omarchy theme the reader picks in Settings. The
choice is the `browser-theme` key in `settings.json`, absent for the desktop theme and otherwise the
theme's folder name, as `omarchy theme dir` resolves it.

Omarchy renders Omaweb's palette from `omaweb.json.tpl` when the desktop switches theme, and that
render is a whole-desktop switch: `omarchy theme set` restarts terminals and retints applications.
It cannot render a theme the desktop is not on. Omaweb therefore fills the same template itself, the
reader's copy in `~/.config/omarchy/themed`, from the colours Omarchy's read-only
`omarchy-theme-color --file <theme>/colors.toml --all` prints for the picked theme. It writes the
result to its own state directory and reads it as a palette source between the reader's `theme.json`
and the desktop's render, so the order of ADR 0016 becomes `OMAWEB_THEME_FILE`, `theme.json`, the
picked theme, the desktop's render, and the built-in palette. Omaweb watches the picked theme's
`colors.toml` and renders again when it changes. A picked theme that is not installed falls back to
the desktop theme and keeps the choice for when it returns.

Parsing `colors.toml` in Omaweb was rejected because Omarchy resolves legacy colour names and
derives the missing shades, and a copy of those rules in Omaweb would drift from Omarchy's. Writing
the render into `theme.json` was rejected because that file is the reader's own, and ADR 0016 keeps
it theirs.

The vendored kit reads its control tokens, the normal, hover, focus and selected colours of its
controls, from the desktop theme's `shell.toml` at a path it fixes, and ADR 0018 left them to the
kit. Under a light browser theme on a dark desktop those controls would draw the desktop's light
foreground on a light ground. While a browser theme is picked, `KitTheme` therefore also drives the
control colours from the palette, as Omarchy's own `shell.toml` template derives them from
foreground and accent, and leaves their alphas and widths to the kit. Pointing the kit at a rendered
`shell.toml` was rejected because the path is the kit's own and ADR 0017 forbids editing it.
Following the desktop theme drives nothing new.

The app icon stays with the desktop theme, because the launcher it appears in belongs to the
desktop.
