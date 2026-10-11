#!/bin/bash
# The loop in a throwaway archlinux:base-devel container shaped like CI's
# budget job: Omaweb's engine from its repository, dnsmasq as root on the
# container's resolver, and cage on the headless backend.
#
#   tests/debug/renderer-sigtrap/container/run.sh <work-dir> [content-blocker-dir]
#
# Without a content-blocker directory, the container installs rust and
# bootstraps one. CPUS (0-3) is the container's cpuset, WORKERS (3) its cages,
# and LOOP_EXTRA more variables for the browser, such as
# QT_QUICK_BACKEND=software. `touch <work-dir>/stop` ends the loop, and
# `docker rm -f omaweb-sigtrap-loop` the container.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
source=$(cd "$here/../../../.." && pwd)
work=$(mkdir -p "$1" && cd "$1" && pwd)
git -C "$source" archive HEAD > "$work/src.tar"
patch --quiet -o "$work/bench.py" "$source/scripts/benchmark_runtime.py" \
    < "$here/../pageload.patch"
cp "$here/setup.sh" "$here/loop.sh" "$here/inner.sh" "$here/../trapreport.c" "$work/"
if [ -n "${2:-}" ]; then
    cp -r "$2" "$work/content-blocker"
fi
chmod -R a+rX "$work"
rm -f "$work/stop"
docker run -d --name omaweb-sigtrap-loop --shm-size=2g --cpuset-cpus="${CPUS:-0-3}" \
    -v "$work:/work" archlinux:base-devel sleep infinity
docker exec omaweb-sigtrap-loop /work/setup.sh > "$work/setup.log" 2>&1
docker exec -d -e WORKERS="${WORKERS:-3}" -e LOOP_EXTRA="${LOOP_EXTRA:-}" omaweb-sigtrap-loop \
    bash -c '/work/loop.sh > /work/loop.log 2>&1'
