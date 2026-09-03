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
-- ~/.local/share/omarchy/default/hypr/windows.lua
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
rather than leaving Qt to derive one — so the exemption is one rule in
`~/.config/hypr/hyprland.conf`:

```conf
windowrule = opacity 1.0 1.0, class:^(omaweb)$
```

Then `hyprctl reload`. Confirm the class first if the page stays washed:

```sh
hyprctl clients -j | jq -r '.[] | select(.title|test("Omaweb";"i")) | .class'
```

This is a rule the reader adds, not one Omaweb installs. Omaweb writes into
`~/.config/omarchy/themed/` because that is a template directory a program is
meant to render from; `~/.config/hypr/` is the reader's own window management,
and a browser that quietly edited it would be doing something else entirely.
Upstreaming a `default/hypr/apps/omaweb.lua` into Omarchy is the fix that would
reach every install, and it depends on an external pull request.

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
