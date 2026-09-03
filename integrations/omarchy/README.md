# Omarchy Quattro

Omaweb follows the Omarchy theme on its own. On Linux, the first start installs
the template it ships here as a user-wide Omarchy template:

```text
~/.config/omarchy/themed/omaweb.json.tpl
```

and then asks Omarchy to render the theme that is already active, so the browser
comes up in the desktop's colours without anything being run by hand. Omarchy
renders the active file to:

```text
~/.local/state/omarchy/current/theme/omaweb.json
```

Omaweb watches the generated file and its parent directory, and picks the file
up the moment it first appears. No `theme-set` hook is required, and no restart.

A template already at that path is never overwritten: a customisation of yours,
or of a theme's, outranks the one this version ships. Omaweb logs the decision
when the two differ. Delete the file to take the shipped template again.

Nothing is written where Omarchy is not installed — the state directory above is
the evidence that it is — so the macOS build and every other desktop are
unaffected.

## The desktop's window opacity

Omarchy washes every window slightly. Its defaults tag all of them and then
apply an opacity to the tag:

```lua
-- $OMARCHY_PATH/default/hypr/windows.lua
o.window(".*", { tag = "+default-opacity" })
o.window({ tag = "default-opacity" }, { opacity = "0.985 0.96" })
```

Hyprland applies that to the whole surface, opaque pixels included, so it
reaches the webpage viewport that Omaweb deliberately paints opaque. The page
comes out 1.5% into the desktop when focused and 4% when not. Omarchy already
treats this as wrong for a browser and exempts them by class in
`default/hypr/apps/browser.lua`, but Omaweb carries neither the
`chromium-based-browser` nor the `firefox-based-browser` tag, so it keeps the
generic value.

Note that `decoration:active_opacity` is a different setting. It reads `1.0` on
a stock Omarchy while this rule is still washing every window, so it is not the
thing to check.

Omaweb's window class is `omaweb` on both Wayland and X11 — `main()` states it
rather than leaving Qt to derive one — so the exemption is one line in
`~/.config/hypr/looknfeel.lua`, which is where a window's appearance belongs:

```lua
o.window("omaweb", { tag = "-default-opacity", opacity = "1 1" })
```

Then `hyprctl reload`. That is the same shape Omarchy's own
`default/hypr/apps/qemu.lua` uses to opt a window out.

`1 1` rather than the `1.0 0.985` Omarchy gives Chromium and Firefox in
`apps/browser.lua`: an unfocused browser there still washes its page by 1.5%,
and a webpage viewport that is opaque only while focused is not opaque. Omaweb's
own surfaces carry the translucency instead, where the theme can name it.

Order is what makes it work. Omarchy's defaults tag every window, let their
`apps/*.lua` opt out, and only then apply the opacity to whatever still carries
the tag. A user override is loaded after all of that — `hyprland.lua` requires
Omarchy's defaults before its own files — so the later `opacity = "1 1"` wins.
Dropping the tag as well costs nothing and is what the defaults do.

The bottom of `~/.config/hypr/hyprland.lua` works as well, under the "Add any
other personal Hyprland configuration below" comment it already carries. Either
is loaded after Omarchy's defaults, which is all the rule needs.

This is a rule the reader adds, not one Omaweb installs. Omaweb writes into
`~/.config/omarchy/themed/` because that is a template directory a program is
meant to render from; `~/.config/hypr/` is the reader's own window management,
and a browser that quietly edited it would be doing something else entirely.
Upstreaming a `default/hypr/apps/omaweb.lua` into Omarchy is the fix that would
reach every install, and it depends on an external pull request. That is
tracked in villekivela/omaweb#75, and waits on Omaweb being something an
Omarchy user can install.

## Overrides

Set `OMAWEB_NO_OMARCHY_TEMPLATE` to any value to stop Omaweb writing into
`~/.config/omarchy` at all, for a configuration directory you generate or track
yourself. The manual install is then:

```sh
mkdir -p ~/.config/omarchy/themed
cp integrations/omarchy/omaweb.json.tpl ~/.config/omarchy/themed/omaweb.json.tpl
omarchy theme set "$(omarchy theme current)"
```

`OMAWEB_THEME_FILE=<path>` points Omaweb at one palette instead, and a
`theme.json` in `~/.config/omaweb` outranks the desktop's. See ADR 0016 for the
whole lookup order.
