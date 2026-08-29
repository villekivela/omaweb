#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev}"
script_directory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "$script_directory/.." && pwd)"

cd "$repository_root"

start_seconds=$SECONDS
cmake --preset "$preset"
configure_seconds=$((SECONDS - start_seconds))

cmake --build --preset "$preset" --target clean
start_seconds=$SECONDS
cmake --build --preset "$preset"
clean_build_seconds=$((SECONDS - start_seconds))

cmake -E touch src/ui/ThemeController.cpp
start_seconds=$SECONDS
cmake --build --preset "$preset"
incremental_cpp_seconds=$((SECONDS - start_seconds))

start_seconds=$SECONDS
cmake --build --preset "$preset"
no_op_build_seconds=$((SECONDS - start_seconds))

echo "configure_seconds=$configure_seconds"
echo "clean_build_seconds=$clean_build_seconds"
echo "incremental_cpp_seconds=$incremental_cpp_seconds"
echo "no_op_build_seconds=$no_op_build_seconds"
