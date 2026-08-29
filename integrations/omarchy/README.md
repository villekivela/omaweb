# Omarchy Quattro

Install the Tanto theme template as a user-wide Omarchy template:

```sh
mkdir -p ~/.config/omarchy/themed
cp integrations/omarchy/tanto.json.tpl ~/.config/omarchy/themed/tanto.json.tpl
omarchy theme set "$(omarchy theme current)"
```

Omarchy renders the active file to:

```text
~/.local/state/omarchy/current/theme/tanto.json
```

Tanto watches the generated file and its parent directory. No `theme-set` hook is required.
