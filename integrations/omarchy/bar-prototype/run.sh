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
  # A shell started outside the session, such as Claude Code's, lacks these.
  local run=${XDG_RUNTIME_DIR:-/run/user/$UID}
  if [[ -z ${WAYLAND_DISPLAY:-} ]]; then
    WAYLAND_DISPLAY=$(basename "$(ls "$run"/wayland-? | head -1)")
    export WAYLAND_DISPLAY
  fi
  if [[ -z ${HYPRLAND_INSTANCE_SIGNATURE:-} ]]; then
    HYPRLAND_INSTANCE_SIGNATURE=$(ls -t "$run/hypr" | head -1)
    export HYPRLAND_INSTANCE_SIGNATURE
  fi
  omarchy-launch-tui --app-id=dev.omaweb.prototype python3 "$here/fake-omaweb.py" \
    >"${XDG_RUNTIME_DIR:-/tmp}/omaweb/feed-launch.log" 2>&1 &
}

case "${1:-start}" in
start)
  ln -sfn "$here" "$link"
  omarchy-shell shell rescanPlugins >/dev/null
  # Tabs after the workspaces, the address beside the center.
  jq --arg id "$id" '
    .bar.layout.left |= (map(select(.id != $id)) | (map(.id) | index("omarchy.workspaces")) as $at
      | if $at == null then . + [{id: $id, role: "tabs"}]
        else .[: $at + 1] + [{id: $id, role: "tabs"}] + .[$at + 1 :] end)
    | .bar.layout.centerBegin = ((.bar.layout.centerBegin // []) | map(select(.id != $id))) + [{id: $id, role: "address"}]
    | .bar.layout.right |= map(select(.id != $id))
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
