#!/bin/sh
# Fails unless a configured build tree found Omaweb's engine under
# /usr/lib/omaweb rather than a stock QtWebEngine.
#
#   scripts/check_omaweb_engine.sh build/ci
#
# A stock engine installed beside Omaweb's wins without a word: CMake finds its
# Qt6WebEngine* packages in /usr/lib/cmake ahead of the prefix, even with
# QT_ADDITIONAL_PACKAGES_PREFIX_PATH set, and the browser builds without CNAME
# uncloaking (docs/development.md). CMake's cache is where that shows, in two
# places: the directory each engine package was found in, and the answer of
# the probe that asks the engine's headers for the DNS alias API. Both are
# read, so neither a stock package nor a stock header gets through.
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <build-directory>" >&2
    exit 2
fi
cache="$1/CMakeCache.txt"
prefix=/usr/lib/omaweb/

packages=$(grep -E '^Qt6WebEngine[A-Za-z]*_DIR:PATH=' "$cache" || true)
if [ -z "$packages" ]; then
    echo "::error::$cache names no QtWebEngine package, so there is no engine to check" >&2
    exit 1
fi
stock=$(printf '%s\n' "$packages" | grep -v "=$prefix" || true)
if [ -n "$stock" ]; then
    echo "::error::$1 found a QtWebEngine outside $prefix; see docs/development.md" >&2
    printf '%s\n' "$stock" >&2
    exit 1
fi
if ! grep -qx 'OMAWEB_ENGINE_OFFERS_DNS_ALIASES:INTERNAL=1' "$cache"; then
    echo "::error::$1 did not find the DNS alias API, so it is not on Omaweb's engine" >&2
    exit 1
fi
printf '%s\n' "$packages"
echo "and the DNS alias API is there"
