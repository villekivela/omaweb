#!/usr/bin/env bash
#
# Whether a commit range touches anything a compiler reads.
#
# The three Arch jobs in CI take around ten minutes each, and a change confined
# to prose or the website cannot break a build. This is what CI asks before
# spending them, and what anyone can run to find out why a build did or did not
# happen on a pull request.
#
# The test is inverted on purpose. Everything counts as source unless it is
# named below, so a directory nobody has thought of yet builds rather than
# quietly skipping: the cost of forgetting is ten minutes, and the cost of the
# other mistake is a break that reaches `main`. A range touching both prose and
# source is source.
#
# Exits 0 when the range has source in it and 1 when it has not. Both are
# answers rather than failures; a range it cannot read is an error, exit 2.
#
#     scripts/source_changed.sh <base> [head]

set -euo pipefail

# Read rather than compiled: `docs/` and `website/` in whole, and a Markdown
# file wherever it sits. Everything else is source, including `scripts/`,
# `packaging/`, `cmake/` and the workflows themselves.
readonly prose='^(docs|website)/|\.md$'

if (( $# < 1 || $# > 2 )); then
    echo "usage: $0 <base> [head]" >&2
    exit 2
fi

base="$1"
head="${2:-HEAD}"

for commit in "$base" "$head"; do
    if ! git rev-parse --verify "${commit}^{commit}" >/dev/null 2>&1; then
        echo "source_changed: no commit named ${commit}" >&2
        exit 2
    fi
done

changed="$(git diff --name-only "$base" "$head")"
if [[ -z "$changed" ]]; then
    echo "source_changed: the range changes no file, so there is nothing to build"
    exit 1
fi

source_files="$(printf '%s\n' "$changed" | grep -Ev "$prose" || true)"

if [[ -n "$source_files" ]]; then
    echo "source_changed: the build runs, for:"
    printf '%s\n' "$source_files" | sed 's/^/  /'
    exit 0
fi

echo "source_changed: prose and website only, so the build has nothing to compile:"
printf '%s\n' "$changed" | sed 's/^/  /'
exit 1
