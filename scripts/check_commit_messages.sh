#!/usr/bin/env bash

set -euo pipefail

readonly pattern='^(feat|fix|test|refactor|docs|chore|build|ci|perf|revert)(\([a-z0-9][a-z0-9._/-]*\))?!?: [a-z0-9]'

check_subject() {
    local subject="$1"
    local summary

    if [[ ! "$subject" =~ $pattern ]]; then
        return 1
    fi
    if (( ${#subject} > 72 )); then
        return 1
    fi

    summary="${subject#*: }"
    if [[ "$summary" =~ ^(.+)\ \(#[0-9]+\)$ ]]; then
        summary="${BASH_REMATCH[1]}"
    fi
    [[ "$summary" != *. ]]
}

is_historical_exception() {
    case "$1" in
        b2a076730c42a96dbc46052af3289878b149b4c6 | \
            48127cd012b1cf83b885b0a7d543b39b84620a02)
            return 0
            ;;
    esac
    return 1
}

if [[ "${1:-}" == "--message" ]]; then
    if (( $# != 2 )); then
        echo "usage: $0 --message <subject>" >&2
        exit 2
    fi
    if ! check_subject "$2"; then
        echo "invalid commit subject: $2" >&2
        exit 1
    fi
    exit 0
fi

readonly baseline="${1:-35e01d4}"
readonly head="${2:-HEAD}"

if ! git merge-base --is-ancestor "$baseline" "$head"; then
    echo "commit-message baseline $baseline is not an ancestor of $head" >&2
    exit 2
fi

status=0
while IFS=$'\t' read -r commit subject; do
    if is_historical_exception "$commit"; then
        continue
    fi
    if ! check_subject "$subject"; then
        echo "invalid commit subject in ${commit:0:12}: $subject" >&2
        status=1
    fi
# Merge commits carry GitHub's generated subject ("Merge pull request #N
# from ..."), which no Conventional Commits pattern accepts. The convention
# governs the commits an author writes, so merges are out of scope rather
# than exempt one hash at a time.
done < <(git log --no-merges --format='%H%x09%s' "$baseline..$head")

if (( status != 0 )); then
    echo "expected: <type>[optional scope][!]: <lowercase imperative summary>" >&2
fi
exit "$status"
