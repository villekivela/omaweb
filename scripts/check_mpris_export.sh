#!/usr/bin/env bash
#
# Whether the Sounding tab reaches the desktop, asked of a running browser.
#
# The unit tests stand the export up against a private session bus, which
# answers the shape of the protocol but says nothing about the path a page's
# declaration takes to get there: the script in the page, the console channel
# the adapter reads it off, the core that picks the tab, and the window that
# hands a media key back to the page. This drives all of it, by starting the
# browser on a page that declares a media session and then talking to it the way
# a bar's media widget does.
#
# It needs a graphical session and puts a browser window on screen, so it is a
# check to run by hand rather than a CI gate. It runs the browser on a session
# bus of its own, for the same reason `check_wayland_session.py` does: on the
# reader's bus the argument would be handed to the browser already open, and the
# player would land in the reader's own bar. The code path is the session bus
# either way.
#
#     scripts/check_mpris_export.sh [path to the built omaweb binary]
#
# Exits 0 when the player appears, carries what the page declared, answers a
# command, and goes when the browser does. Exits 1 on the first step that does
# not.

set -euo pipefail

# A bus of its own, entered once. `dbus-run-session` is part of the D-Bus
# package that Qt already requires, so it is wherever this can run at all.
if [ -z "${OMAWEB_MPRIS_CHECK_BUS:-}" ]; then
    export OMAWEB_MPRIS_CHECK_BUS=1
    exec dbus-run-session -- "$0" "$@"
fi

browser="${1:-build/ci/omaweb}"
service="org.mpris.MediaPlayer2.omaweb"
object="/org/mpris/MediaPlayer2"
player="org.mpris.MediaPlayer2.Player"

if [ ! -x "$browser" ]; then
    echo "No browser to run at $browser. Build it, or name one." >&2
    exit 1
fi
if [ -z "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ]; then
    echo "No graphical session. This check runs a browser window." >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'if [ -n "${browser_pid:-}" ]; then kill "$browser_pid" 2>/dev/null || true; fi; rm -rf "$work"' EXIT

# A page that declares a media session and answers the actions itself, which is
# what a music site does. It plays nothing: the declaration is the part of the
# path this checks, and a page that needed a gesture to make sound could not be
# driven from here.
cat > "$work/page.html" <<'PAGE'
<!doctype html>
<title>Sounding tab check</title>
<script>
  const session = navigator.mediaSession;
  session.metadata = new MediaMetadata({
    title: "A declared track",
    artist: "A declared artist",
    album: "A declared album",
    artwork: [{ src: "https://example.test/cover.png", sizes: "512x512", type: "image/png" }],
  });
  session.setActionHandler("nexttrack", () => {});
  session.setActionHandler("previoustrack", () => {});
  session.setActionHandler("play", () => {
    session.playbackState = "playing";
  });
  session.setActionHandler("pause", () => {
    session.playbackState = "paused";
  });
  session.playbackState = "playing";
</script>
PAGE

ask() {
    gdbus call --session --dest "$service" --object-path "$object" "$@"
}

# Asked of the bus itself rather than of a listing tool, so the answer is about
# the bus this check is on and no other.
owned() {
    gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.NameHasOwner "$service" 2>/dev/null | grep -q true
}

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

# A session of its own, so the check starts with one tab on the check page and
# leaves the reader's Spaces, history and open tabs alone.
OMAWEB_DATA_ROOT="$work/data" OMAWEB_CONFIG_ROOT="$work/config" \
    "$browser" "file://$work/page.html" > "$work/browser.log" 2>&1 &
browser_pid=$!

for _ in $(seq 1 30); do
    sleep 1
    if owned; then
        break
    fi
done
owned || fail "no player on the bus 30 seconds after the browser started"
echo "OK: the player is on the bus as $service"

metadata="$(ask --method org.freedesktop.DBus.Properties.Get "$player" Metadata)"
echo "$metadata" | grep -q "A declared track" \
    || fail "the title the page declared did not reach the bus: $metadata"
echo "$metadata" | grep -q "example.test/cover.png" \
    || fail "the artwork the page named did not reach the bus: $metadata"
echo "OK: the metadata is what the page declared"

ask --method "$player.PlayPause" > /dev/null
sleep 2
status="$(ask --method org.freedesktop.DBus.Properties.Get "$player" PlaybackStatus)"
echo "$status" | grep -q Paused \
    || fail "PlayPause did not reach the page's own handler: $status"
ask --method "$player.PlayPause" > /dev/null
sleep 2
status="$(ask --method org.freedesktop.DBus.Properties.Get "$player" PlaybackStatus)"
echo "$status" | grep -q Playing \
    || fail "the second PlayPause did not reach the page's own handler: $status"
echo "OK: a media key reaches the page and what it does comes back"

kill "$browser_pid" 2>/dev/null || true
wait "$browser_pid" 2>/dev/null || true
browser_pid=""
sleep 2
if owned; then
    fail "the player is still on the bus after the browser went"
fi
echo "OK: the player went with the browser"
# The bus this ran on is torn down after this, and its daemon says so on the way
# out. Anything printed below this line is that, not a result.
echo "The Sounding tab reaches the desktop."
