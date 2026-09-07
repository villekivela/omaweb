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
exec python3 -m http.server "$port" --directory "$root" --bind 127.0.0.1
