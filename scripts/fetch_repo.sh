#!/usr/bin/env bash
#
# Fetches the pacman repository's current contents for one architecture, so
# `repo-add` updates what is published rather than starting from nothing.
#
#     scripts/fetch_repo.sh --tag repo-x86_64 --dir <directory>
#
# The repository is served from a GitHub release's assets rather than from a
# branch, because the engine package is larger than the 100 MB a git push
# accepts and Chromium is not going to get smaller (ADR 0043). A release's
# assets are flat under one URL, which is the directory shape pacman expects.
#
# A tag that does not exist yet is not an error: it is the first publish, and
# the directory is left empty.

set -euo pipefail

tag=""
dir=""

usage() {
    echo "usage: $0 --tag <tag> --dir <directory>" >&2
    exit 2
}

while (( $# > 0 )); do
    case "$1" in
        --tag) tag="${2:-}"; shift 2 ;;
        --dir) dir="${2:-}"; shift 2 ;;
        *) usage ;;
    esac
done
[[ -n "$tag" && -n "$dir" ]] || usage

command -v gh > /dev/null 2>&1 || { echo "fetch_repo: gh is not installed" >&2; exit 1; }

mkdir -p "$dir"

if ! gh release view "$tag" > /dev/null 2>&1; then
    echo "fetch_repo: no $tag yet, so this is the first publish"
    exit 0
fi

# Every asset, because the database names the packages beside it and publishing
# checks that each name it lists is a file that is there.
echo "fetch_repo: taking $tag into $dir"
gh release download "$tag" --dir "$dir" --clobber

echo "fetch_repo: $(find "$dir" -maxdepth 1 -type f | wc -l | tr -d ' ') file(s)"
