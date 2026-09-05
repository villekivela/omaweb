#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

action=write
scope=changed
base=origin/main
while [ "$#" -gt 0 ]; do
    case "$1" in
        --check)
            action=check
            shift
            ;;
        --all)
            scope=all
            shift
            ;;
        --changed)
            scope=changed
            base=${2:?--changed requires a Git revision}
            shift 2
            ;;
        *)
            echo "usage: $0 [--check] [--all | --changed REVISION]" >&2
            exit 2
            ;;
    esac
done

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
all_files="$work/all"
cpp_files="$work/cpp"
qml_files="$work/qml"
prettier_files="$work/prettier"
: > "$cpp_files"
: > "$qml_files"
: > "$prettier_files"

if [ "$scope" = all ]; then
    git ls-files > "$all_files"
else
    if ! git rev-parse --verify "$base^{commit}" >/dev/null 2>&1; then
        echo "unknown base revision: $base" >&2
        exit 2
    fi
    {
        git diff --name-only --diff-filter=ACMR "$base"...HEAD
        git diff --name-only --diff-filter=ACMR
        git diff --cached --name-only --diff-filter=ACMR
        git ls-files --others --exclude-standard
    } | sort -u > "$all_files"
fi

while IFS= read -r file; do
    [ -f "$file" ] || continue
    case "$file" in
        third_party/*|docs/archive/*)
            continue
            ;;
        *.cpp|*.h|*.mm)
            printf '%s\n' "$file" >> "$cpp_files"
            ;;
        *.qml)
            printf '%s\n' "$file" >> "$qml_files"
            ;;
        *.md|*.json|*.yml|*.yaml|*.html|*.css|*.js|*.mjs)
            printf '%s\n' "$file" >> "$prettier_files"
            ;;
    esac
done < "$all_files"

clang_format=clang-format
if ! command -v "$clang_format" >/dev/null 2>&1; then
    if command -v xcrun >/dev/null 2>&1 && xcrun -f clang-format >/dev/null 2>&1; then
        clang_format=$(xcrun -f clang-format)
    else
        echo "clang-format is required" >&2
        exit 1
    fi
fi

qml_format=qmlformat
if ! command -v "$qml_format" >/dev/null 2>&1; then
    if [ -x /usr/lib/qt6/bin/qmlformat ]; then
        qml_format=/usr/lib/qt6/bin/qmlformat
    else
        echo "qmlformat is required" >&2
        exit 1
    fi
fi

failed=0
if [ "$action" = write ]; then
    while IFS= read -r file; do
        "$clang_format" -i "$file"
    done < "$cpp_files"
    while IFS= read -r file; do
        "$qml_format" -i "$file"
    done < "$qml_files"
    if [ -s "$prettier_files" ]; then
        xargs npm exec -- prettier --write < "$prettier_files"
    fi
else
    while IFS= read -r file; do
        "$clang_format" --dry-run --Werror "$file" || failed=1
    done < "$cpp_files"

    formatted="$work/formatted.qml"
    while IFS= read -r file; do
        if ! "$qml_format" "$file" > "$formatted" || ! cmp -s "$file" "$formatted"; then
            echo "$file: run scripts/format.sh --changed $base" >&2
            failed=1
        fi
    done < "$qml_files"

    if [ -s "$prettier_files" ]; then
        xargs npm exec -- prettier --check < "$prettier_files" || failed=1
    fi
fi

exit "$failed"
