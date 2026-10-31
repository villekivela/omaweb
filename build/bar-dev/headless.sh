#!/usr/bin/env bash
# Runs the harness in a headless cage with the fake feed, for screenshots.
# Restarts whatever an earlier run left.
here="$(cd "$(dirname "$0")" && pwd)"
run=/tmp/claude-1000/bd
mkdir -p "$run" && chmod 700 "$run"
for p in $(pgrep -x cage); do grep -qa "$here/shell" /proc/$p/cmdline && kill $p; done
for pidfile in "$run"/feed.pid "$run"/cage.pid; do
  [[ -f $pidfile ]] && kill "$(cat "$pidfile")" 2>/dev/null
done
python3 "$here/fake-feed.py" "$run/feed/bar.sock" > "$here/feed.log" 2>&1 &
echo $! > "$run/feed.pid"
env -u WAYLAND_DISPLAY -u HYPRLAND_INSTANCE_SIGNATURE XDG_RUNTIME_DIR=$run BAR_SOCKET=$run/feed/bar.sock \
  WLR_BACKENDS=headless WLR_RENDERER=pixman dbus-run-session -- cage -- qs -p "$here/shell/shell.qml" > "$here/qs.log" 2>&1 &
echo $! > "$run/cage.pid"
wait
