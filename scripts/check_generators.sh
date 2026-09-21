#!/bin/sh
# Checks that the build-graph test reads the graph of whichever generator the
# tree was configured with.
#
# The test asks whether the build compiles QML ahead of time, and it answers by
# reading the generated build files: one `build.ninja` under Ninja, a
# `build.make` per target under Make. Reading only one generator's would leave
# the other unchecked and say nothing about it, so this configures both ways and
# runs the test in each. Configuring is all it takes: the graph is written
# before anything is built.
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=${OMAWEB_GENERATOR_WORK:-$(mktemp -d)}
mkdir -p "$work"
graph_test=omaweb-qml-build-graph
status=0

# Nothing here needs Omaweb's own engine: what is being read is the build graph
# CMake writes, which the engine has no part in. Whatever Qt the host has is
# enough. A caller that wants a particular one says so in
# OMAWEB_GENERATOR_CMAKE_ARGS.
extra_args=${OMAWEB_GENERATOR_CMAKE_ARGS:-}

# A worktree has no bootstrapped content blocker of its own, and configuring
# stops without one. OMAWEB_CONTENT_BLOCKER_CACHE names another tree's, the way
# a worktree build does; unset, CMake's own default finds this tree's.
if [ -n "${OMAWEB_CONTENT_BLOCKER_CACHE:-}" ]; then
    extra_args="$extra_args -DOMAWEB_CONTENT_BLOCKER_CACHE=$OMAWEB_CONTENT_BLOCKER_CACHE"
fi

check() {
    generator=$1
    source_qml=$2
    directory=$work/$(echo "$generator" | tr ' ' '-')-$source_qml
    echo "==> Configuring with $generator, OMAWEB_SOURCE_QML=$source_qml"
    rm -rf "$directory"
    # shellcheck disable=SC2086
    if ! cmake -S "$repo_root" -B "$directory" -G "$generator" \
        -DCMAKE_BUILD_TYPE=Release -DOMAWEB_BUILD_BROWSER=ON -DOMAWEB_BUILD_UI_LAB=ON \
        -DOMAWEB_SOURCE_QML="$source_qml" $extra_args >"$directory.log" 2>&1; then
        echo "Configuring with $generator failed; see $directory.log" >&2
        return 1
    fi
    if ! ctest --test-dir "$directory" -N 2>/dev/null | grep -q "$graph_test"; then
        echo "With $generator the $graph_test test is not registered" >&2
        return 1
    fi
    if ! ctest --test-dir "$directory" -R "$graph_test" --output-on-failure \
        >"$directory.test.log" 2>&1; then
        echo "With $generator the $graph_test test failed; see $directory.test.log" >&2
        sed -n '/FATAL\|Error/p' "$directory.test.log" >&2
        return 1
    fi
    echo "    $graph_test reads the $generator graph and passes"
}

# Both answers the test can give, so a generator whose graph goes unread cannot
# pass by finding nothing: OMAWEB_SOURCE_QML=ON expects no compile step,
# OFF expects one.
check "Ninja" ON || status=1
check "Ninja" OFF || status=1
check "Unix Makefiles" ON || status=1
check "Unix Makefiles" OFF || status=1

if [ -z "${OMAWEB_GENERATOR_WORK:-}" ]; then
    rm -rf "$work"
fi
exit $status
