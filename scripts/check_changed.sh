#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
base=${1:-origin/main}

"$repo_root/scripts/check_format.sh" --changed "$base"
"$repo_root/scripts/check_qml.sh" --changed "$base"
git -C "$repo_root" diff --check
