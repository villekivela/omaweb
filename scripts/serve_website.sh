#!/usr/bin/env bash
# Serve website/ on http://localhost:8000 for local review.
#
#   scripts/serve_website.sh            # the site
#   scripts/serve_website.sh 8080       # on another port
#
# Rebuild the per-theme palettes, screenshots, favicons and wordmarks after a
# chrome change or an upstream theme change:
# scripts/build_website_themes.py
set -euo pipefail

port="${1:-8000}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/website"

printf 'website  http://localhost:%s/\n' "$port"

# `python3 -m http.server` types a file from the system mime table, and an Arch
# install has no entry for WebP: the screenshots then arrive as
# application/octet-stream and the browser declines to draw them, which looks
# exactly like a broken page. The types are named here so a local review shows
# what hosting would.
exec python3 - "$port" "$root" <<'PYTHON'
import functools
import http.server
import sys

handler = http.server.SimpleHTTPRequestHandler
handler.extensions_map.update({".webp": "image/webp", ".svg": "image/svg+xml"})
# Threaded, because the one-request-at-a-time server stalls the whole site on
# a connection a browser is holding open: the page stops loading half way
# through its own images and nothing says why.
http.server.test(
    HandlerClass=functools.partial(handler, directory=sys.argv[2]),
    ServerClass=http.server.ThreadingHTTPServer,
    port=int(sys.argv[1]),
    bind="127.0.0.1",
)
PYTHON
