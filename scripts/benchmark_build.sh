#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev}"
script_directory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "$script_directory/.." && pwd)"

cd "$repository_root"

measure_seconds() {
    local timing_file
    timing_file="$(mktemp)"
    TIMEFORMAT='%3R'
    { time "$@"; } 2>"$timing_file"
    sed '$d' "$timing_file" >&2
    measured_seconds="$(tail -n 1 "$timing_file")"
    rm "$timing_file"
}

measure_seconds cmake --preset "$preset"
configure_seconds="$measured_seconds"

cmake --build --preset "$preset" --target clean
measure_seconds cmake --build --preset "$preset"
clean_build_seconds="$measured_seconds"

cmake -E touch src/ui/ThemeController.cpp
measure_seconds cmake --build --preset "$preset"
incremental_cpp_seconds="$measured_seconds"

measure_seconds cmake --build --preset "$preset"
no_op_build_seconds="$measured_seconds"

echo "configure_seconds=$configure_seconds"
echo "clean_build_seconds=$clean_build_seconds"
echo "incremental_cpp_seconds=$incremental_cpp_seconds"
echo "no_op_build_seconds=$no_op_build_seconds"
