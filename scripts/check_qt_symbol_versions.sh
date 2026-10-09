#!/bin/sh
# Fails when a binary in an unpacked package needs a newer Qt than the one given.
#
#   scripts/check_qt_symbol_versions.sh <unpacked-package> <qt-version>
#
# A binary linked against Qt records the version of every Qt symbol it uses,
# `Qt_6.12` for one Qt 6.12 added, and the loader refuses to start it on a Qt
# that lacks that version. Release packages are built against the Qt readers
# have, Omarchy's stable mirror on x86_64 (#674), so a binary needing a newer
# Qt than its build's means the build was not where it was meant to be. v0.13.0
# needed Qt_6.12 and could not start on the mirror's Qt 6.11.2. `Qt_6_PRIVATE_API`
# names no version and is not read.
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <unpacked-package> <qt-version>" >&2
    exit 2
fi
tree=$1
qt=$2
case "$qt" in
    [0-9]*.[0-9]*) ;;
    *)
        echo "$qt is not a Qt version" >&2
        exit 2
        ;;
esac
major=${qt%%.*}
minor=${qt#*.}
minor=${minor%%.*}

checked=0
newer=""
# Split on newlines alone and never globbed, so a name with a space or a `*`
# is one file. A name with a newline in it is not in a package this builds.
files=$(find "$tree" -type f | sort)
set -f
IFS='
'
for file in $files; do
    readelf -h "$file" > /dev/null 2>&1 || continue
    checked=$((checked + 1))
    # Only what the binary needs, not versions a library defines for others.
    needs=$(readelf -V --wide "$file" | awk '
        /^Version needs section/ { reading = 1; next }
        /^Version (definition|symbols) section/ { reading = 0 }
        reading && match($0, /Name: Qt_[0-9]+\.[0-9]+/) {
            print substr($0, RSTART + 6, RLENGTH - 6)
        }' | sort -u)
    for need in $needs; do
        version=${need#Qt_}
        need_major=${version%%.*}
        need_minor=${version#*.}
        if [ "$need_major" -gt "$major" ] \
            || { [ "$need_major" -eq "$major" ] && [ "$need_minor" -gt "$minor" ]; }; then
            newer="$newer${file#"$tree"/} needs $need
"
        fi
    done
done

if [ "$checked" -eq 0 ]; then
    echo "::error::$tree has no binaries, so nothing was checked" >&2
    exit 1
fi
if [ -n "$newer" ]; then
    echo "::error::built against Qt $qt, and these need a newer Qt:" >&2
    printf '%s' "$newer" >&2
    exit 1
fi
echo "$checked binaries need no Qt newer than $qt"
