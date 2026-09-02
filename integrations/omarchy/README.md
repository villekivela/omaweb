# Omarchy Quattro

Install the Omaweb theme template as a user-wide Omarchy template:

```sh
mkdir -p ~/.config/omarchy/themed
cp integrations/omarchy/omaweb.json.tpl ~/.config/omarchy/themed/omaweb.json.tpl
omarchy theme set "$(omarchy theme current)"
```

Omarchy renders the active file to:

```text
~/.local/state/omarchy/current/theme/omaweb.json
```

Omaweb watches the generated file and its parent directory. No `theme-set` hook is required.
