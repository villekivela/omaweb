#!/usr/bin/env bash
#
# What a page's call learns about this machine's addresses, asked of a running
# browser.
#
# The contract test reads the WebRTC address policy back off a profile's
# settings, which is what the engine promises and all a runner with one
# interface can check. This asks the engine itself: it starts the browser on a
# page that gathers ICE candidates and prints what the page saw, once with the
# policy on and once with it off. On, the page sees the server-reflexive
# address of the default route and no host candidate; off, it sees a host
# candidate per interface as well. Chromium hides a host candidate's address
# behind an mDNS name from a page holding no media permission, so the
# difference reads as candidates present or absent rather than as addresses.
#
# It needs a graphical session, puts a browser window on screen twice, and
# sends one STUN request per run to the server named below, so it is a check
# to run by hand rather than a CI gate. The browser runs on throwaway data and
# configuration roots: the policy is written to the scratch `privacy.json`,
# and nothing of the reader's is read or changed.
#
#     scripts/check_webrtc_candidates.sh [path to the built omaweb binary]
#
# Exits 0 when the policy on leaves no host candidate and the policy off
# gathers at least one. Exits 1 on the first run that does not.

set -euo pipefail

browser="${1:-build/ci/omaweb}"
port=8765
debugging_port=9333
stun="stun:stun.l.google.com:19302"

if [ ! -x "$browser" ]; then
    echo "No browser to run at $browser. Build it, or name one." >&2
    exit 1
fi
if [ -z "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ]; then
    echo "No graphical session. This check runs a browser window." >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'if [ -n "${browser_pid:-}" ]; then kill "$browser_pid" 2>/dev/null || true; fi
      if [ -n "${server_pid:-}" ]; then kill "$server_pid" 2>/dev/null || true; fi
      rm -rf "$work"' EXIT

# The page reports every candidate in its title, which the debugging listener
# hands out without a DevTools session. `typ` is the eighth field of a
# candidate line: host for an interface's own address, srflx for the one a
# STUN server saw.
mkdir -p "$work/page"
cat > "$work/page/index.html" <<PAGE
<!doctype html>
<title>gathering</title>
<script>
  const seen = [];
  const connection = new RTCPeerConnection({ iceServers: [{ urls: "$stun" }] });
  connection.createDataChannel("check");
  connection.onicecandidate = (event) => {
    if (!event.candidate) {
      document.title = "done " + seen.join(" ");
      return;
    }
    const fields = event.candidate.candidate.split(" ");
    seen.push(fields[7] + ":" + fields[4]);
    document.title = "gathering " + seen.join(" ");
  };
  connection.createOffer().then((offer) => connection.setLocalDescription(offer));
</script>
PAGE
(cd "$work/page" && python3 -m http.server "$port" --bind 127.0.0.1 >/dev/null 2>&1) &
server_pid=$!
sleep 1
if ! curl -sf "http://127.0.0.1:$port/" | grep -q RTCPeerConnection; then
    echo "Port $port is not serving this check's page. Something else holds it." >&2
    exit 1
fi

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

title() {
    curl -s "http://127.0.0.1:$debugging_port/json" 2>/dev/null | python3 -c '
import json, sys
pages = [page["title"] for page in json.load(sys.stdin) if page.get("type") == "page"]
print(pages[0] if pages else "")' 2>/dev/null || true
}

# One run per policy. Off is written the way Settings writes it, so the read
# path is the browser's own; on is the default and needs no file.
gather() {
    rm -rf "$work/data" "$work/config"
    mkdir -p "$work/data" "$work/config" "$work/home"
    if [ "$1" = off ]; then
        echo '{"webrtc-public-interfaces-only": false}' > "$work/config/privacy.json"
    fi
    HOME="$work/home" XDG_DATA_HOME="$work/data" OMAWEB_CONFIG_ROOT="$work/config" \
        "$browser" --remote-debugging="$debugging_port" "http://127.0.0.1:$port/" \
        >"$work/browser-$1.log" 2>&1 &
    browser_pid=$!
    # Gathering ends only after the engine has given up on every route it
    # tried, which on a host without a reachable IPv6 route is a long wait.
    # A candidate set that has stopped growing is the answer.
    result=""
    stable=0
    for _ in $(seq 1 60); do
        sleep 1
        now="$(title)"
        case "$now" in
        done*) result="$now"; break ;;
        "gathering "*)
            if [ "$now" = "$result" ]; then
                stable=$((stable + 1))
            else
                result="$now"
                stable=0
            fi
            if [ "$stable" -ge 5 ]; then
                break
            fi
            ;;
        esac
    done
    kill "$browser_pid" 2>/dev/null || true
    wait "$browser_pid" 2>/dev/null || true
    browser_pid=""
    echo "policy $1: ${result:-no answer from the page}" >&2
    case "$result" in
    done*|"gathering "*:*) ;;
    *) fail "the page gathered nothing with the policy $1" ;;
    esac
    printf '%s\n' "$result"
}

on="$(gather on | tail -n 1)"
case "$on" in
*host:*) fail "a host candidate reached the page with the policy on" ;;
esac

off="$(gather off | tail -n 1)"
case "$off" in
*host:*) ;;
*) fail "no host candidate reached the page with the policy off" ;;
esac

echo "The policy keeps a call to the default route and lets it go when the reader says so."
