#!/usr/bin/env bash
#
# Serves a pacman repository directory from a GitHub release's assets.
#
#     scripts/serve_repo.sh --tag repo-x86_64 --dir <directory> [--title <title>]
#
# The directory is what `publish_repo.sh` left behind: the packages, their
# detached signatures, and the signed databases. Every file in it becomes an
# asset of one release per architecture, whose download URL is the `Server` a
# reader writes into `pacman.conf`.
#
# An asset the directory no longer holds is deleted, because `publish_repo.sh`
# removes the package a replaced database entry used to name and a repository
# that went on serving it would offer a version its database does not list.
#
# Why not a branch: the engine package is over the 100 MB a git push accepts,
# and it is Chromium, so it will not shrink (ADR 0043). A release's assets have
# a 2 GB limit each and sit flat under one URL, which is the shape pacman asks
# for.

set -euo pipefail

tag=""
dir=""
title=""

usage() {
    echo "usage: $0 --tag <tag> --dir <directory> [--title <title>]" >&2
    exit 2
}

while (( $# > 0 )); do
    case "$1" in
        --tag) tag="${2:-}"; shift 2 ;;
        --dir) dir="${2:-}"; shift 2 ;;
        --title) title="${2:-}"; shift 2 ;;
        *) usage ;;
    esac
done
[[ -n "$tag" && -n "$dir" ]] || usage
[[ -d "$dir" ]] || { echo "serve_repo: $dir is not a directory" >&2; exit 1; }

command -v gh > /dev/null 2>&1 || { echo "serve_repo: gh is not installed" >&2; exit 1; }

# Names only, and without `find -printf`, which is GNU's: this runs on a
# container and on whatever machine somebody publishes from by hand.
serving=()
while IFS= read -r name; do
    serving+=("$name")
done < <(cd "$dir" && find . -maxdepth 1 -type f | sed 's|^\./||' | sort)
if (( ${#serving[@]} == 0 )); then
    echo "serve_repo: $dir holds no files, so there is nothing to serve" >&2
    exit 1
fi

# The database has to be there, or what is published is a directory of packages
# no client can read.
for required in omaweb.db omaweb.db.sig; do
    [[ -f "$dir/$required" ]] || {
        echo "serve_repo: $dir has no $required" >&2
        exit 1
    }
done

if ! gh release view "$tag" > /dev/null 2>&1; then
    echo "==> Creating $tag"
    # `--latest=false` so the repository never takes the badge a reader looks
    # for when they want the browser.
    gh release create "$tag" --latest=false \
        --title "${title:-Package repository ($tag)}" \
        --notes "This release is a pacman repository rather than something to download by hand. Its assets are the packages and the signed databases \`pacman -Sy\` reads, and its download URL is the \`Server\` line in the instructions.

Nothing here is a version of Omaweb. The browser's own releases are the ones named \`v*\`."
fi

echo "==> Serving ${#serving[@]} file(s) from $tag"
# `--clobber`, because a repository is a place rather than an archive: the
# database and the packages are replaced in it every time.
gh release upload "$tag" --clobber "${serving[@]/#/$dir/}"

# Anything the release still carries that the directory no longer does. A
# superseded package left as an asset is a file the database does not name,
# which is the same repository inconsistency `publish_repo.sh` refuses locally.
published=()
while IFS= read -r name; do
    published+=("$name")
done < <(gh release view "$tag" --json assets -q '.assets[].name' | sort)
removed=0
for asset in "${published[@]}"; do
    if ! printf '%s\n' "${serving[@]}" | grep -qxF "$asset"; then
        echo "==> Removing superseded asset $asset"
        gh release delete-asset "$tag" "$asset" --yes
        removed=$(( removed + 1 ))
    fi
done
echo "serve_repo: $tag serves ${#serving[@]} file(s), removed $removed"
