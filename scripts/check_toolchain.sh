#!/usr/bin/env bash
set -euo pipefail

required_commands=(cmake ninja clang++ ccache)
missing=0

for command_name in "${required_commands[@]}"; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "missing: $command_name" >&2
        missing=1
    fi
done

if ! command -v qtpaths6 >/dev/null 2>&1 && ! command -v qtpaths >/dev/null 2>&1; then
    echo "missing: qtpaths or qtpaths6" >&2
    missing=1
fi

if [[ "$missing" -ne 0 ]]; then
    exit 1
fi

cmake --version | head -n 1
ninja --version
clang++ --version | head -n 1
ccache --version | head -n 1

if command -v qtpaths6 >/dev/null 2>&1; then
    qtpaths6 --qt-version
else
    qtpaths --qt-version
fi
