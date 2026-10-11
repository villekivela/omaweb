#!/bin/bash
# Repeats the first page load of the page-load budget on this machine, each
# worker in a headless cage of its own and pinned to two cores.
#
#   tests/debug/renderer-sigtrap/host/start.sh <omaweb-browser> <work-dir>
#
# WORKERS (4) cages start, numbered from FIRST (0); worker w takes cores
# 2*(w%4) and 2*(w%4)+1. LOOP_NO_GPU (1) covers /dev/dri in the run's private
# network, as on a runner with no GPU. Each run is a line in
# <work-dir>/runs.log, and a run that went quiet keeps its output beside it.
# `touch <work-dir>/stop` ends the loop after the runs in progress.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
source=$(cd "$here/../../../.." && pwd)
export LOOP_BROWSER=$(readlink -f "$1") LOOPDIR=$(mkdir -p "$2" && cd "$2" && pwd)
export LOOP_SOURCE=$source LOOP_NO_GPU=${LOOP_NO_GPU-1}
patch --quiet -o "$LOOPDIR/bench.py" "$source/scripts/benchmark_runtime.py" \
    < "$here/../pageload.patch"
rm -f "$LOOPDIR/stop"
# The reader's own compositor, browser and theme are none of the run's business.
unset WAYLAND_DISPLAY DISPLAY HYPRLAND_INSTANCE_SIGNATURE HYPRLAND_CMD
export OMAWEB_NO_OMARCHY_TEMPLATE=1 QT_QPA_PLATFORM=wayland
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
for w in $(seq "${FIRST:-0}" $(( ${FIRST:-0} + ${WORKERS:-4} - 1 ))); do
    WORKER=$w setsid taskset -c $((2 * (w % 4))),$((2 * (w % 4) + 1)) \
        cage -- "$here/inner.sh" > "$LOOPDIR/cage-$w.log" 2>&1 &
done
