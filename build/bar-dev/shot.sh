#!/usr/bin/env bash
# Grabs the headless harness's two strips into shot.png.
set -e
run=/tmp/claude-1000/bd
display=$(cd "$run" && ls -t wayland-? | head -1)
rm -f "$(dirname "$0")/shot.png"
WAYLAND_DISPLAY=$display XDG_RUNTIME_DIR=$run grim -g "${1:-0,30 1200x75}" "$(dirname "$0")/shot.png"
