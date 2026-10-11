#!/bin/bash
# One worker, inside its cage: the page-load run, again and again.
cd "$LOOP_SOURCE"
out=$LOOPDIR/w$WORKER
mkdir -p "$out"
engine=$(ldd "$LOOP_BROWSER" | awk '/libQt6WebEngineCore/{print $3}')
i=0
while [ ! -e "$LOOPDIR/stop" ]; do
    i=$((i + 1))
    start=$(date +%s)
    LOOP_KEEP_LOG=$out/stderr.txt PYTHONPATH=scripts timeout 300 \
        python3 "$LOOPDIR/bench.py" pageload --browser "$LOOP_BROWSER" --require-dns \
        > "$out/run.txt" 2>&1
    status=$?
    # A run that never reached the page load is the loop's fault, not the engine's.
    grep -q "pageload:" "$out/run.txt" || break
    sign=$(grep -m1 -o 'never reported[^:]*' "$out/run.txt"
           grep -m1 -E -o 'Check failed.*|Received signal.*|FATAL.*|trapreport: signal.*' \
               "$out/stderr.txt")
    if [ -n "$sign" ]; then
        cp "$out/run.txt" "$out/keep-$i-run.txt"
        cp "$out/stderr.txt" "$out/keep-$i-stderr.txt"
    fi
    echo "$(date +%T) w$WORKER run $i exit $status $(($(date +%s) - start))s" \
        "engine=$engine nogpu=${LOOP_NO_GPU:-0} qsg=${QT_QUICK_BACKEND:-default} $sign" \
        >> "$LOOPDIR/runs.log"
done
