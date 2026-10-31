#!/usr/bin/env bash
# Opens the worktree's Omarchy bar widget in a scratch Quickshell window that
# reloads on every save. `run.sh` reads the installed Omaweb's bar socket (Tabs:
# Omarchy bar must be on); `run.sh fake` reads a canned state instead.
here="$(cd "$(dirname "$0")" && pwd)"
if [[ $1 == fake ]]; then
  export BAR_SOCKET="$XDG_RUNTIME_DIR/omaweb-bar-dev/bar.sock"
  python3 "$here/fake-feed.py" "$BAR_SOCKET" &
  trap 'kill $!' EXIT
fi
qs -p "$here/shell/shell.qml"
