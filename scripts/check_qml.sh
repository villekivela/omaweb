#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

scope=${1:---changed}
base=${2:-origin/main}
files=$(mktemp)
trap 'rm -f "$files"' EXIT HUP INT TERM

case "$scope" in
    --all)
        git ls-files '*.qml' ':!third_party/**' > "$files"
        ;;
    --changed)
        if ! git rev-parse --verify "$base^{commit}" >/dev/null 2>&1; then
            echo "unknown base revision: $base" >&2
            exit 2
        fi
        {
            git diff --name-only --diff-filter=ACMR "$base"...HEAD
            git diff --name-only --diff-filter=ACMR
            git diff --cached --name-only --diff-filter=ACMR
            git ls-files --others --exclude-standard
        } | sort -u | while IFS= read -r file; do
            case "$file" in
                *.qml)
                    [ -f "$file" ] && printf '%s\n' "$file"
                    ;;
            esac
        done > "$files"
        ;;
    *)
        echo "usage: $0 [--all | --changed REVISION]" >&2
        exit 2
        ;;
esac

failed=0
qml_lint=qmllint
if ! command -v "$qml_lint" >/dev/null 2>&1; then
    if [ -x /usr/lib/qt6/bin/qmllint ]; then
        qml_lint=/usr/lib/qt6/bin/qmllint
    else
        echo "qmllint is required" >&2
        exit 1
    fi
fi

while IFS= read -r file; do
    if ! "$qml_lint" -I src/ui -I src/ui-lab -I src/engine/qt -I third_party/omarchy-shell \
        "$file" >/dev/null 2>&1; then
        "$qml_lint" -I src/ui -I src/ui-lab -I src/engine/qt -I third_party/omarchy-shell "$file"
        failed=1
    fi
done < "$files"

exit "$failed"
