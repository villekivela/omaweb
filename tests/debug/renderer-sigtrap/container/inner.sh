#!/bin/bash
# Inside a cage, as builder: the host loop's worker, on the container's paths.
export LOOP_SOURCE=/home/builder/src LOOPDIR=/work LOOP_BROWSER=/home/builder/src/build/ci/omaweb-browser
exec /home/builder/src/tests/debug/renderer-sigtrap/host/inner.sh
