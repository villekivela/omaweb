# Omarchy

Omaweb's desktop is Omarchy, and Omarchy changes quickly. Read the current tree of
`basecamp/omarchy` on its default branch (`quattro` for Omarchy 4) before designing anything that
talks to the desktop, for example with `gh api repos/basecamp/omarchy/contents/<path>?ref=quattro`.

Omarchy 4 (release v4.0.4 and on):

- **No Walker or Elephant.** `Super+Space` opens Omarchy's own Quickshell menu
  (`shell/plugins/menu/Menu.qml`).
- **The menu is extended in JSONC** at `~/.config/omarchy/extensions/omarchy-menu.jsonc`. A row's
  `provider` can only name a row source Menu.qml already defines, so an application can't add a
  dynamic one there.
- **Dynamic lists go through `omarchy-menu-select`.** It reads options from its arguments or from
  standard input, each `<glyph><TAB><label>` or `<glyph><TAB><label><TAB><subtext>`. It prints
  `<label><TAB><subtext>` for the chosen row (the label alone for a row without subtext), and exits
  1 when the reader closes the menu.
- **Terminals start through `xdg-terminal-exec`**, which `omarchy-launch-terminal` wraps in
  `uwsm-app`.
- **Raising a window** is done with `hyprctl dispatch focus address:...`, as
  `omarchy-launch-or-focus` does. A Wayland client's own activation request is honoured only with a
  fresh input serial.
