#!/bin/sh
# PROTOTYPE #541 — throwaway. Runs the UI lab with the Site information card variants.
# Usage: scripts/prototype_site_card.sh [variant 1-6: A B B2 B3 C D] [scenario secure|http|cert|start] [detail] [radius]
# Build first: cmake --preset ui && cmake --build --preset ui
set -eu
cd "$(dirname "$0")/.."
lab=build/ui/omaweb-ui-lab.app/Contents/MacOS/omaweb-ui-lab
[ -x "$lab" ] || lab=build/ui/omaweb-ui-lab
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-cocoa}"
set -- --tabs --browse --show site --site-variant "${1:-1}" --site-scenario "${2:-secure}" \
    ${3:+--site-detail "$3"} ${4:+--site-radius "$4"}
exec "$lab" "$@"
