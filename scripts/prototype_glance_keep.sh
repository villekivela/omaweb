#!/usr/bin/env bash
# PROTOTYPE #704, throwaway: how keeping a Glance as a tab should move.
#
#   scripts/prototype_glance_keep.sh [A|B|C|D]
#       Opens the UI lab with a Glance over a page. The bar at the bottom
#       switches variant and replays the keep; the Glance's own open-as-tab
#       button keeps it too.
#   scripts/prototype_glance_keep.sh --capture <dir>
#       Renders each variant a frame at a time, held on the motion's clock,
#       and puts them together as GIFs and MP4s, at speed and at a quarter.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
lab="$root/build/ui/omaweb-ui-lab"
CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-6} \
    cmake --build --preset ui --target omaweb-ui-lab >/dev/null

if [[ "${1:-}" != "--capture" ]]; then
    exec "$lab" --tabs --browse --glance --glance-keep "${1:-A}"
fi

out=${2:?--capture takes a directory}
mkdir -p "$out"
export QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1
step=20
declare -A total=([A]=730 [B]=330 [C]=320 [D]=730)
for variant in A B C D; do
    frames="$out/$variant-frames"
    rm -rf "$frames"
    mkdir -p "$frames"
    # A still Glance first, then the motion, then its end held for a beat.
    index=0
    "$lab" --tabs --browse --glance --glance-keep "$variant" \
        --capture "$frames/$(printf %03d $index).png" \
        --capture-delay 1200 >/dev/null 2>&1
    for ((at = 0; at <= total[$variant] + step; at += step)); do
        index=$((index + 1))
        "$lab" --tabs --browse --glance --glance-keep "$variant" --glance-keep-at "$at" \
            --capture "$frames/$(printf %03d $index).png" --capture-delay 1200 >/dev/null 2>&1 &
        if (($(jobs -r | wc -l) >= 4)); then wait -n; fi
    done
    wait
    # The still and the end, held: 400 ms each.
    for speed in 1 4; do
        rate=$((1000 / step / speed))
        ffmpeg -loglevel error -y -framerate "$rate" -i "$frames/%03d.png" \
            -vf "tpad=start_mode=clone:start_duration=0.4:stop_mode=clone:stop_duration=0.4" \
            -pix_fmt yuv420p "$out/$variant-x$speed.mp4"
        ffmpeg -loglevel error -y -i "$out/$variant-x$speed.mp4" \
            -vf "scale=900:-1:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse" \
            "$out/$variant-x$speed.gif"
    done
    echo "$variant: $index frames, $out/$variant-x1.gif, $out/$variant-x4.gif"
done
