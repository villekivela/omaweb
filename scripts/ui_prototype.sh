#!/usr/bin/env bash
# THROWAWAY: runs the chrome-design prototype. See src/ui-lab/prototype/README.md.
# Usage: scripts/ui_prototype.sh [A|B|C]
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

cmake --preset ui -DTANTO_BUILD_UI_PROTOTYPE=ON >/dev/null
cmake --build --preset ui --target tanto-ui-prototype

export TANTO_UI_VARIANT="${1:-${TANTO_UI_VARIANT:-A}}"

binary="build/ui/tanto-ui-prototype.app/Contents/MacOS/tanto-ui-prototype"
if [[ ! -x "$binary" ]]; then
    binary="build/ui/tanto-ui-prototype"
fi

exec "$binary"
