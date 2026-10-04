#!/usr/bin/env bash
# Runs Omaweb itself, instrumented, through workspace switches on this machine, and says whether
# the page stays painted (#517). Start it inside the Hyprland session and leave the machine alone:
#
#   cd <your omaweb checkout> && git fetch origin diag/517-instrument \
#     && git show origin/diag/517-instrument:scripts/diagnose_omaweb_switch.sh > /tmp/d517.sh \
#     && bash /tmp/d517.sh
#
# It clones the branch into a scratch directory, builds the `omaweb` target there, and runs it
# against scratch data, config and control-socket paths on a session bus of its own, so the
# browser you use is not touched and nothing is installed. Expect 15 to 40 minutes of building on
# half the cores, then about six minutes of running. The scratch directory is removed at the end
# unless --keep is given.
#
# Cases, each with five round trips to workspace 9:
#   page          a tab on a red page. The failure shows as the red going black.
#   start         a tab on the Start page, with no page view under the chrome.
#   page-no-gpu-compositing
#                 the page case with --disable-gpu-compositing. A diagnostic only: it says whether
#                 the failure follows GPU compositing, and is never the fix.
# The page case also dumps what Chromium reports about its GPU, over the remote debugging port.
# Omaweb has no setting or environment variable that turns the page blur off, so there is no
# blur-off case: the summary says so.
#
# The report is ./omaweb-switch-report-<time>/ with summary.txt (paste it into the issue), the
# per-case logs, and a screenshot after every return.
set -euo pipefail

branch=diag/517-instrument
source_url=""
keep=0
away=9
cycles=5
jobs=$(($(nproc) / 2))
[ "$jobs" -ge 2 ] || jobs=2
engine=/usr/lib/omaweb

while [ $# -gt 0 ]; do
    case $1 in
    --source) source_url=$2; shift 2 ;;
    --keep) keep=1; shift ;;
    --jobs) jobs=$2; shift 2 ;;
    --away-workspace) away=$2; shift 2 ;;
    --cycles) cycles=$2; shift 2 ;;
    --engine-prefix) engine=$2; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

for tool in git cmake ninja cargo hyprctl grim python3 dbus-run-session; do
    command -v "$tool" >/dev/null 2>&1 || { echo "needs $tool" >&2; exit 1; }
done
: "${WAYLAND_DISPLAY:?run inside the Wayland session}"
: "${XDG_RUNTIME_DIR:?run inside the Wayland session}"
[ -d "$engine/lib/cmake" ] || { echo "no engine at $engine" >&2; exit 1; }
if [ -z "${HYPRLAND_INSTANCE_SIGNATURE:-}" ]; then
    for instance in "$XDG_RUNTIME_DIR"/hypr/*/; do
        HYPRLAND_INSTANCE_SIGNATURE=$(basename "$instance")
        export HYPRLAND_INSTANCE_SIGNATURE
        break
    done
fi
if [ -z "$source_url" ]; then
    source_url=$(git remote get-url origin) || {
        echo "run from an omaweb checkout, or pass --source <url-or-path>" >&2
        exit 1
    }
fi

scratch=$HOME/.cache/omaweb-517-diagnostic
report=$PWD/omaweb-switch-report-$(date +%Y%m%d-%H%M%S)
mkdir -p "$report/screens"
log() { echo "[$(date +%H:%M:%S)] $*" | tee -a "$report/progress.log"; }

cleanup() {
    pkill -f "$scratch/src/build/dev/omaweb" 2>/dev/null || true
    if [ "$keep" -eq 0 ]; then
        rm -rf "$scratch"
    else
        echo "scratch kept at $scratch"
    fi
}
trap cleanup EXIT INT TERM

# --- the build ---------------------------------------------------------------------------------
rm -rf "$scratch"
mkdir -p "$scratch"
log "cloning $branch from $source_url"
git clone -q --depth 1 --branch "$branch" "$source_url" "$scratch/src"
cd "$scratch/src"
git log --oneline -2 | tee -a "$report/progress.log"

log "building the content blocker (cargo, in the scratch directory)"
export CARGO_HOME=$scratch/cargo
scripts/bootstrap_content_blocker.sh >"$scratch/blocker.log" 2>&1 \
    || { tail -30 "$scratch/blocker.log" >&2; exit 1; }

launcher=ccache
command -v ccache >/dev/null 2>&1 || launcher=""
log "configuring against $engine"
cmake --preset dev -DCMAKE_BUILD_TYPE=Release -DOMAWEB_BUILD_UI_LAB=OFF \
    -DCMAKE_CXX_COMPILER_LAUNCHER="$launcher" -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH="$engine" \
    >"$scratch/configure.log" 2>&1 || { tail -30 "$scratch/configure.log" >&2; exit 1; }
log "building with $jobs jobs; this is the long part"
cmake --build --preset dev --target omaweb -j "$jobs" >"$scratch/build.log" 2>&1 \
    || { tail -40 "$scratch/build.log" >&2; exit 1; }
binary=$scratch/src/build/dev/omaweb
[ -x "$binary" ] || { echo "no binary at $binary" >&2; exit 1; }
log "built; checking it runs the patched engine"
"$binary" --version 2>&1 | tee -a "$report/progress.log" || true

# --- the probes --------------------------------------------------------------------------------
cat >"$scratch/red.html" <<'EOF'
<!doctype html><title>517</title>
<body style="background:red;margin:0"><h1 id="t">0</h1>
<script>
setInterval(() => t.textContent = Date.now(), 50);
document.addEventListener("visibilitychange", () => console.log("page " + document.visibilityState));
</script>
EOF

cat >"$scratch/colours.py" <<'EOF'
import sys

# red, black and total sample counts in the middle third of the screenshot, which is the page area
# for a tab on a page and the Start page for a blank one.
def read(path):
    data = open(path, "rb").read()
    head = data.split(b"\n", 3)
    width, height = map(int, head[1].split())
    return width, height, head[3]

width, height, pixels = read(sys.argv[1])
red = black = total = 0
for y in range(height // 3, 2 * height // 3, 24):
    for x in range(width // 3, 2 * width // 3, 24):
        i = (y * width + x) * 3
        r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
        total += 1
        if r > 200 and g < 60 and b < 60:
            red += 1
        elif r + g + b < 30:
            black += 1
print("%d %d %d" % (red, black, total))
EOF

# What Chromium says about its GPU, from the browser's own DevTools endpoint: no extra program.
cat >"$scratch/gpu.py" <<'EOF'
import base64, hashlib, json, os, socket, struct, sys, urllib.request

port = int(sys.argv[1])
info = json.load(urllib.request.urlopen("http://127.0.0.1:%d/json/version" % port, timeout=5))
url = info["webSocketDebuggerUrl"]
path = url.split("%d" % port, 1)[1]
sock = socket.create_connection(("127.0.0.1", port), timeout=10)
key = base64.b64encode(os.urandom(16)).decode()
sock.sendall(("GET %s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
              "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % (path, port, key)).encode())
buffer = b""
while b"\r\n\r\n" not in buffer:
    buffer += sock.recv(4096)
buffer = buffer.split(b"\r\n\r\n", 1)[1]

def send(text):
    payload = text.encode()
    mask = os.urandom(4)
    header = bytes([0x81])
    if len(payload) < 126:
        header += bytes([0x80 | len(payload)])
    else:
        header += bytes([0x80 | 126]) + struct.pack(">H", len(payload))
    sock.sendall(header + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

def need(count):
    global buffer
    while len(buffer) < count:
        buffer += sock.recv(65536)

def receive():
    global buffer
    need(2)
    length = buffer[1] & 0x7F
    offset = 2
    if length == 126:
        need(4)
        length = struct.unpack(">H", buffer[2:4])[0]
        offset = 4
    elif length == 127:
        need(10)
        length = struct.unpack(">Q", buffer[2:10])[0]
        offset = 10
    need(offset + length)
    payload = buffer[offset:offset + length]
    buffer = buffer[offset + length:]
    return payload.decode()

send(json.dumps({"id": 1, "method": "SystemInfo.getInfo"}))
while True:
    message = json.loads(receive())
    if message.get("id") == 1:
        print(json.dumps(message, indent=1))
        break
EOF

dispatch_focus() {
    # Hyprland 0.55 and later take Lua; earlier ones take `workspace N`.
    if hyprctl dispatch "hl.dsp.focus({ workspace = \"$1\" })" 2>&1 | grep -qi error; then
        hyprctl dispatch workspace "$1" >/dev/null
    fi
}

# True once a window of this scratch build is on screen, whatever it shows: the Start page has no
# page view and so logs nothing to wait for.
window_up() {
    local pids
    pids=$(pgrep -f "$binary" | tr '\n' ' ')
    [ -n "$pids" ] || return 1
    hyprctl clients -j | python3 -c '
import json, sys
pids = set(map(int, sys.argv[1].split()))
sys.exit(0 if any(c.get("pid") in pids and c.get("mapped") for c in json.load(sys.stdin)) else 1)
' "$pids"
}

# --- a case ------------------------------------------------------------------------------------
# run_case <name> <chromium flags> [url]
run_case() {
    local name=$1 flags=$2 url=${3:-}
    local dir=$report/$name
    local data=$scratch/data-$name
    mkdir -p "$dir" "$data"
    log "case $name: starting"
    local start
    start=$(hyprctl activeworkspace -j | python3 -c 'import json,sys; print(json.load(sys.stdin)["id"])')
    (
        export OMAWEB_DATA_ROOT=$data/data OMAWEB_CONFIG_ROOT=$data/config
        export OMAWEB_CONTROL_SOCKET=$data/control.sock
        export QTWEBENGINE_CHROMIUM_FLAGS="$flags"
        [ "$name" = page ] && export QTWEBENGINE_REMOTE_DEBUGGING=127.0.0.1:9517
        # The session bus of its own is what keeps this launch from being handed to a browser
        # that is already running.
        exec dbus-run-session -- "$binary" ${url:+"$url"}
    ) >"$dir/app.log" 2>&1 &
    local waited=0
    until window_up; do
        sleep 2
        waited=$((waited + 2))
        if [ "$waited" -gt 120 ]; then
            log "case $name: no window after 120 s"
            echo "$name: NO WINDOW (see $name/app.log)" >>"$report/verdicts.txt"
            pkill -f "$binary" 2>/dev/null || true
            return 0
        fi
    done
    sleep 8
    if [ "$name" = page ]; then
        python3 "$scratch/gpu.py" 9517 >"$report/gpu.json" 2>"$report/gpu.err" \
            || log "the GPU dump failed (see gpu.err)"
    fi
    grim -t ppm "$scratch/s.ppm" && cp "$scratch/s.ppm" "$dir/screen-initial.ppm"
    read -r base_red base_black total < <(python3 "$scratch/colours.py" "$scratch/s.ppm")
    echo "initial: red=$base_red black=$base_black of $total" >"$dir/cycles.txt"
    local i=1 verdicts=""
    while [ "$i" -le "$cycles" ]; do
        echo "517 $(date +%s%3N) driver cycle=$i away" >>"$dir/driver.log"
        dispatch_focus "$away"
        sleep 2
        echo "517 $(date +%s%3N) driver cycle=$i back" >>"$dir/driver.log"
        dispatch_focus "$start"
        sleep 3
        grim -t ppm "$scratch/s.ppm" && cp "$scratch/s.ppm" "$dir/screen-$i.ppm"
        read -r red black total < <(python3 "$scratch/colours.py" "$scratch/s.ppm")
        local verdict=painted
        if [ "$name" = start ]; then
            [ $((black * 10)) -gt $((base_black * 10 + total * 3)) ] && verdict=BLACK
        else
            [ $((red * 4)) -lt "$total" ] && verdict=BLACK
        fi
        echo "cycle $i: red=$red black=$black of $total -> $verdict" >>"$dir/cycles.txt"
        verdicts="$verdicts $verdict"
        i=$((i + 1))
    done
    pkill -f "$binary" 2>/dev/null || true
    sleep 2
    cat "$dir/driver.log" >>"$dir/app.log"
    grep -a '517 ' "$dir/app.log" | sort -k2,2n >"$dir/events.log" || true
    {
        echo "$name:$verdicts"
    } >>"$report/verdicts.txt"
    log "case $name:$verdicts"
}

red_url=file://$scratch/red.html
run_case page "" "$red_url"
run_case start ""
run_case page-no-gpu-compositing "--disable-gpu-compositing" "$red_url"

# --- the summary -------------------------------------------------------------------------------
{
    echo "branch: $branch ($(git -C "$scratch/src" rev-parse --short HEAD))"
    echo "hyprland: $(hyprctl version -j 2>/dev/null | python3 -c 'import json,sys; print(json.load(sys.stdin).get("tag"))' 2>/dev/null || true)"
    echo "mesa: $(pacman -Q mesa 2>/dev/null || true)"
    echo "engine: $(pacman -Q omaweb-qtwebengine 2>/dev/null || true)"
    echo
    echo "VERDICT PER CASE (painted = the page area kept its content after the return)"
    cat "$report/verdicts.txt"
    echo
    echo "blur off: not run, Omaweb has no setting or variable that turns the page blur off"
    echo
    for name in page start page-no-gpu-compositing; do
        echo "== $name"
        cat "$report/$name/cycles.txt" 2>/dev/null || true
        echo "renderProcessPid values seen (more than one means the renderer was replaced):"
        { grep -aoE 'renderProcessPid=[0-9]+|pid=[0-9]+' "$report/$name/app.log" 2>/dev/null || true; } \
            | sed 's/.*pid=//I' | sort -u | tr '\n' ' '
        echo
        grep -a 'renderProcessTerminated' "$report/$name/app.log" 2>/dev/null | head -3 || true
        echo "state changes around the first return (view, window, lifecycle, focus, pageFrozen):"
        grep -aE '517 [0-9]+ (webView|root|window|lifecycleState|recommendedState|pageFrozen|sceneGraph|driver)' \
            "$report/$name/events.log" 2>/dev/null | head -40 || true
        echo
    done
    echo "== GPU, from SystemInfo.getInfo (page case)"
    python3 - "$report/gpu.json" <<'PY' 2>/dev/null || echo "no GPU dump (see gpu.err)"
import json, sys
info = json.load(open(sys.argv[1]))["result"]
for key, value in sorted(info.get("gpu", {}).get("featureStatus", {}).items()):
    print("  %s: %s" % (key, value))
for device in info.get("gpu", {}).get("devices", []):
    print("  device:", device.get("vendorString"), device.get("deviceString"), device.get("driverVersion"))
print("  glRenderer:", info.get("gpu", {}).get("auxAttributes", {}).get("glRenderer"))
PY
} >"$report/summary.txt"

log "done. Paste $report/summary.txt into #517 and attach the rest of $report."
