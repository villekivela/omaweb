#!/usr/bin/env bash
# Build website/ into website/dist/ and serve that on http://localhost:8000
# for local review. The release pages are generated, so the build is the only
# way to see the site whole; it runs without the network, with the releases
# page that points at GitHub in place of the generated ones.
#
#   scripts/serve_website.sh            # the site
#   scripts/serve_website.sh 8080       # on another port
#
# A change wants the build run again: stop this and start it again, or run
# `node build/site.mjs --local` from website/ while it serves, or
# `node build/site.mjs` for the real release pages. Regenerate the interface
# captures after a chrome change or an upstream theme change:
# scripts/build_website_themes.py
set -euo pipefail

port="${1:-8000}"
website="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/website"
root="$website/dist"

(cd "$website" && node build/site.mjs --local)
printf 'website  http://localhost:%s/\n' "$port"

# `python3 -m http.server` types a file from the system mime table, and an Arch
# install has no entry for WebP or WOFF2: the images and fonts then arrive as
# application/octet-stream and the browser declines to draw them, which looks
# exactly like a broken page. The types are named here so a local review shows
# what hosting would.
exec python3 - "$port" "$root" <<'PYTHON'
import functools
import http.server
import sys

handler = http.server.SimpleHTTPRequestHandler
handler.extensions_map.update(
    {".webp": "image/webp", ".svg": "image/svg+xml", ".woff2": "font/woff2"}
)
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
