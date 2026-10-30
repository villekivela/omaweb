#!/bin/bash
# PROTOTYPE, throwaway: put the omaweb bar widget on the Omarchy bar and start
# the fake browser that feeds it.
#
#   run.sh          link the plugin, place it twice, open the fake browser
#   run.sh feed     open the fake browser only
#   run.sh remove   take the widget off the bar and unlink the plugin

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
id=omaweb.bar-prototype
link=~/.config/omarchy/plugins/$id
config=~/.config/omarchy/shell.json

feed() {
  setsid ghostty --class=dev.omaweb.prototype --title="Fake Omaweb" \
    -e python3 "$here/fake-omaweb.py" >/dev/null 2>&1 &
}

case "${1:-start}" in
start)
  ln -sfn "$here" "$link"
  omarchy-shell shell rescanPlugins >/dev/null
  # Tabs after the workspaces, navigation first on the right.
  jq --arg id "$id" '
    .bar.layout.left |= (map(select(.id != $id)) | (map(.id) | index("omarchy.workspaces")) as $at
      | if $at == null then . + [{id: $id, role: "tabs"}]
        else .[: $at + 1] + [{id: $id, role: "tabs"}] + .[$at + 1 :] end)
    | .bar.layout.right |= ([{id: $id, role: "navigation"}] + map(select(.id != $id)))
  ' "$config" >"$config.tmp"
  mv "$config.tmp" "$config"
  omarchy-shell shell reloadConfig >/dev/null
  feed
  ;;
feed)
  feed
  ;;
remove)
  jq --arg id "$id" '.bar.layout |= map_values(map(select(.id != $id)))' "$config" >"$config.tmp"
  mv "$config.tmp" "$config"
  omarchy-shell shell reloadConfig >/dev/null
  rm -f "$link"
  omarchy-shell shell rescanPlugins >/dev/null
  ;;
*)
  echo "usage: $0 [start|feed|remove]" >&2
  exit 1
  ;;
esac
