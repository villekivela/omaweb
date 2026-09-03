#!/usr/bin/env bash
#
# Prints Markdown release notes for a commit range, grouped by Conventional
# Commit type. Merge commits are skipped: GitHub writes their subjects, and the
# commits they carry are already in the range.
#
# usage: release_notes.sh <tag> [previous-tag]
#
# With no previous tag, the range starts at the repository's first commit.

set -euo pipefail

if (( $# < 1 || $# > 2 )); then
    echo "usage: $0 <tag> [previous-tag]" >&2
    exit 2
fi

readonly tag="$1"
previous="${2-}"

if [[ -z "$previous" ]]; then
    previous="$(git describe --tags --abbrev=0 --match 'v[0-9]*' "${tag}^" 2>/dev/null || true)"
fi

if [[ -n "$previous" ]]; then
    range="${previous}..${tag}"
else
    range="$tag"
fi

emit_section() {
    local heading="$1" pattern="$2" body
    body="$(git log --no-merges --format='%s|%h' "$range" \
        | awk -F'|' -v pattern="$pattern" '
            $1 ~ pattern {
                subject = $1
                sub(/^[a-z]+(\([^)]*\))?!?: /, "", subject)
                printf "- %s (%s)\n", subject, $2
            }')"
    if [[ -n "$body" ]]; then
        printf '### %s\n\n%s\n\n' "$heading" "$body"
    fi
}

if [[ -n "$previous" ]]; then
    printf 'Changes since %s.\n\n' "$previous"
fi

# A breaking change is marked with ! before the colon, whatever its type.
emit_section 'Breaking changes' '^[a-z]+(\([^)]*\))?!:'
emit_section 'Features'         '^feat(\([^)]*\))?:'
emit_section 'Fixes'            '^fix(\([^)]*\))?:'
emit_section 'Performance'      '^perf(\([^)]*\))?:'
emit_section 'Build and CI'     '^(build|ci)(\([^)]*\))?:'
emit_section 'Documentation'    '^docs(\([^)]*\))?:'
emit_section 'Internal'         '^(refactor|test|chore|revert)(\([^)]*\))?:'

printf '**Full changelog**: '
if [[ -n "$previous" ]]; then
    printf 'https://github.com/villekivela/omaweb/compare/%s...%s\n' "$previous" "$tag"
else
    printf 'https://github.com/villekivela/omaweb/commits/%s\n' "$tag"
fi
