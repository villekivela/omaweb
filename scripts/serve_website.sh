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
# what hosting would. The one extensionless file is /install, which vercel.json
# types as text.
exec python3 - "$port" "$root" <<'PYTHON'
import functools
import http.server
import os
import re
import sys


# Byte ranges, which the stock handler ignores: without them a browser cannot
# seek a video it has not downloaded, so the film's scrubber would snap back to
# the start here and nowhere else. One range per request, as a video asks.
class Handler(http.server.SimpleHTTPRequestHandler):
    def send_head(self):
        self.remaining = None
        asked = re.fullmatch(r"bytes=(\d*)-(\d*)", self.headers.get("Range", ""))
        path = self.translate_path(self.path)
        if not asked or not os.path.isfile(path):
            return super().send_head()
        size = os.path.getsize(path)
        first, last = asked.groups()
        if first:
            start, end = int(first), min(int(last or size - 1), size - 1)
        elif last:
            start, end = max(size - int(last), 0), size - 1
        else:
            return super().send_head()
        # A range that ends before it starts is no range, and the whole file
        # answers it; one that starts past the end cannot be met.
        if last and first and int(last) < int(first):
            return super().send_head()
        if start > end:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{size}")
            self.end_headers()
            return None
        body = open(path, "rb")
        body.seek(start)
        self.send_response(206)
        self.send_header("Content-Type", self.guess_type(path))
        self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.send_header("Content-Length", str(end - start + 1))
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()
        self.remaining = end - start + 1
        return body

    def copyfile(self, source, outputfile):
        remaining = self.remaining
        if remaining is None:
            return super().copyfile(source, outputfile)
        while remaining:
            chunk = source.read(min(remaining, 64 * 1024))
            if not chunk:
                break
            outputfile.write(chunk)
            remaining -= len(chunk)


handler = Handler
handler.extensions_map.update(
    {".webp": "image/webp", ".svg": "image/svg+xml", ".woff2": "font/woff2",
     "": "text/plain; charset=utf-8"}
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
