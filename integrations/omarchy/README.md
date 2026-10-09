# Omarchy Quattro

Omaweb follows the Omarchy theme on its own. On Linux, the first start installs the template it
ships here as a user-wide Omarchy template:

```text
~/.config/omarchy/themed/omaweb.json.tpl
```

and then asks Omarchy to render the theme that is already active, so the browser comes up in the
desktop's colours without anything being run by hand. Omarchy renders the active file to:

```text
~/.local/state/omarchy/current/theme/omaweb.json
```

Omaweb watches the generated file and its parent directory, and picks the file up the moment it
first appears. No `theme-set` hook is required, and no restart.

A template already at that path is never overwritten: a customisation of yours, or of a theme's,
outranks the one this version ships. Omaweb logs the decision when the two differ. Delete the file
to take the shipped template again.

Nothing is written where Omarchy is not installed — the state directory above is the evidence that
it is — so the macOS build and every other desktop are unaffected.

## The app icon

The launcher shows Omaweb under the black and white icon the package installs. Settings' tabs
section offers a second, Theme, which is the same mark in the Omarchy theme's accent on its
background. The choice is synced, and each machine applies it when Omaweb starts there.

Choosing Theme installs `omaweb-icon.svg.tpl` from this directory beside the palette's template,
under the same rules: a template already there stands, and the decision is logged when it differs.
Omarchy renders it on every theme switch to:

```text
~/.local/state/omarchy/current/theme/omaweb-icon.svg
```

Omaweb then writes `~/.local/share/applications/omaweb.desktop`, which outranks the installed entry.
It is the installed entry with `TryExec=omaweb` added and `Icon=` naming a copy of the rendered icon
under `~/.local/share/omaweb/app-icon/`. The copy is named by its contents because Omarchy's menu
caches an icon by its path, and the rendered icon keeps one path across theme switches. While Omaweb
runs it makes a new copy after every switch and points the entry at it. A switch made while Omaweb
is closed shows in the menu on its next start. The entry is rebuilt from the installed one on every
start, so a change the package makes to its entry still arrives.

Choosing Black and white removes that entry and the copies, and removes the template only while it
is still the one Omaweb shipped. An `omaweb.desktop` in that directory that Omaweb did not write is
never changed or removed, and the decision is logged.

Omarchy's menu lists the entry whether or not the browser is installed, because it does not read
`TryExec`. Removing the package prints a reminder to delete the entry.

## Blur behind the browser's surfaces

Omaweb's sidebar, overlays and empty window ground are drawn at the opacity its theme names, so the
desktop shows through them. The reader can set the sidebar's opacity over the theme's with the
Sidebar opacity slider in Settings' interface section, from 50% to 100%, and reset it to the
theme's. Whether what shows through is blurred or sharp is Hyprland's decision and not the
browser's: there is no client-side blur protocol to ask through, so Omaweb binds nothing and a
compositor setting is the whole mechanism.

Omarchy ships blur off in `default/hypr/looknfeel.lua`. Turn it on in
`~/.config/hypr/looknfeel.lua`, which is where a window's appearance belongs:

```lua
hl.config({
  decoration = {
    blur = {
      enabled = true,
    },
  },
})
```

Then `hyprctl reload`. This is a desktop-wide setting rather than one scoped to Omaweb, which is
what makes it the reader's to make.

The surface it changes most is the empty window ground. The shipped template gives `sidebar` 0.95
and `window` 0.0, so a Space at rest shows the desktop through the whole page area while the sidebar
transmits a twentieth of it. Moving the Sidebar opacity slider down lets the desktop show through
the sidebar, blurred when this setting is on and sharp when it is off.

Leaving blur off is a legibility choice and never a broken window. Omaweb's surfaces keep the
opacity the theme gave them and keep taking pointer input, so nothing goes click-through; what a
busy wallpaper costs is contrast behind quiet text. A reader who wants the desktop sealed out can
raise the opacity with the Sidebar opacity slider instead, or in their theme, which needs no
compositor setting at all. A theme switch renders over the theme file and leaves the slider's value
alone.

## Overrides

Set `OMAWEB_NO_OMARCHY_TEMPLATE` to any value to stop Omaweb writing into `~/.config/omarchy` at
all, for a configuration directory you generate or track yourself. That includes the app icon's
template: the Theme icon then shows only if your own setup renders `omaweb-icon.svg`, and the
launcher keeps the installed icon otherwise. The manual install is then:

```sh
mkdir -p ~/.config/omarchy/themed
cp integrations/omarchy/omaweb.json.tpl ~/.config/omarchy/themed/omaweb.json.tpl
omarchy theme set "$(omarchy theme current)"
```

`OMAWEB_THEME_FILE=<path>` points Omaweb at one palette instead, and a `theme.json` in
`~/.config/omaweb` outranks the desktop's. See ADR 0016 for the whole lookup order.
