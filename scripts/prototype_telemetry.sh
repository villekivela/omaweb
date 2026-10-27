#!/bin/sh
# PROTOTYPE (#814), throwaway: runs this worktree's build with a throwaway profile, the Start page on
# the Telemetry Scene. Never the installed omaweb, never the reader's profile.
#
#   scripts/prototype_telemetry.sh [A|B|C|D]
#
# Ctrl+Alt+. and Ctrl+Alt+, (or the yellow pill's arrows) cycle the layouts; Ctrl+Alt+D pretends a
# download runs. Build first with: cmake --preset dev && cmake --build build/dev --target
# omaweb-browser
set -eu
cd "$(dirname "$0")/.."
profile=$(mktemp -d "${TMPDIR:-/tmp}/omaweb-telemetry-prototype.XXXXXX")
trap 'rm -rf "$profile"' EXIT
mkdir -p "$profile/data" "$profile/config"
printf '{ "version": 1, "start-page-scene": "telemetry" }\n' > "$profile/config/settings.json"
OMAWEB_TELEMETRY_LAYOUT=${1:-A} \
    OMAWEB_DATA_ROOT="$profile/data" \
    OMAWEB_CONFIG_ROOT="$profile/config" \
    OMAWEB_CONTROL_SOCKET="$profile/control.sock" \
    OMAWEB_NO_OMARCHY_TEMPLATE=1 \
    dbus-run-session -- build/dev/omaweb-browser
