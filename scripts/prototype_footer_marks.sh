#!/bin/sh
# PROTOTYPE (#492): the footer's Space marks, four ways, in the UI lab against
# the real sidebar. Builds the lab if needed and opens it on variant A, or the
# variant named: scripts/prototype_footer_marks.sh [A|B|C|D] [lab flags...].
# Flip with the pink bar at the bottom or the arrow keys.
set -eu
cd "$(dirname "$0")/.."
variant="${1:-A}"
[ $# -gt 0 ] && shift
if [ ! -f build/ui/build.ninja ]; then
    blocker=.cache/content-blocker/release
    [ -d "$blocker" ] || blocker="$(git rev-parse --path-format=absolute --git-common-dir)/../.cache/content-blocker/release"
    cmake --preset ui -DOMAWEB_CONTENT_BLOCKER_CACHE="$blocker"
fi
cmake --build build/ui --target omaweb-ui-lab
lab=build/ui/omaweb-ui-lab
[ -x "$lab" ] || lab=build/ui/omaweb-ui-lab.app/Contents/MacOS/omaweb-ui-lab
exec "$lab" --tabs --spaces --agents-away --prototype-spaces --variant "$variant" "$@"
