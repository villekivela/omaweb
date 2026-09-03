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
