#!/usr/bin/env bash
#
# Names the packages a release puts in the pacman repository, one path a line,
# in the order they are to be published, and refuses a release that holds
# anything else.
#
#     scripts/release_packages.sh --tag <tag> --dir <directory>
#
# The Release workflow asks it before it publishes a release, and so does the
# engine workflow that publishes one whose own publishing failed. Both publish
# to the repository readers install from, and the second takes whatever someone
# attached to a release, so what may be served is named here rather than
# inferred from what is there.
#
# The tag says what is expected. `engine-*` is the engine alone. `v*` is the
# browser and the small client, exactly one of each for every architecture the
# release holds, the client first: each package is served as soon as it is
# published, and a browser served before the client it depends on is an upgrade
# no reader's `pacman -Syu` can complete until the client follows.
#
# A package's architecture is read from its name, which is how
# `publish_repo.sh` files it, and held against the build inside it, so a
# package misnamed for another architecture is not served to that one.

set -euo pipefail

tag=""
dir=""

usage() {
    echo "usage: $0 --tag <tag> --dir <directory>" >&2
    exit 2
}

refuse() {
    echo "release_packages: $1" >&2
    exit 1
}

while (( $# > 0 )); do
    case "$1" in
        --tag) tag="${2:-}"; shift 2 ;;
        --dir) dir="${2:-}"; shift 2 ;;
        *) usage ;;
    esac
done
[[ -n "$tag" && -n "$dir" ]] || usage

# In the order they are published.
case "$tag" in
    engine-*) expected="omaweb-qtwebengine" ;;
    v[0-9]*) expected="omaweb-cli omaweb" ;;
    *) refuse "$tag is neither an engine-* nor a v* release" ;;
esac

# One line a package: its architecture, its name and its path.
held=""
for package in "$dir"/*.pkg.tar.*; do
    [[ -e "$package" ]] || continue
    case "$package" in *.sig) continue ;; esac
    file=$(basename -- "$package")
    info=$(bsdtar -xOf "$package" .PKGINFO)
    pkgname=$(sed -n 's/^pkgname = //p' <<< "$info" | head -n 1)
    built=$(sed -n 's/^arch = //p' <<< "$info" | head -n 1)
    named=${file##*-}
    named=${named%%.pkg.tar.*}
    case " $expected " in
        *" $pkgname "*) ;;
        *) refuse "$file is $pkgname, which $tag does not carry" ;;
    esac
    [[ "$named" == "$built" ]] || refuse "$file is named $named and was built for $built"
    if grep -q "^$built $pkgname " <<< "$held"; then
        refuse "$tag holds more than one $pkgname for $built"
    fi
    held+="$built $pkgname $package"$'\n'
done
[[ -n "$held" ]] || refuse "$tag carries no package"

for arch in $(cut -d' ' -f1 <<< "$held" | sed '/^$/d' | sort -u); do
    for name in $expected; do
        grep -q "^$arch $name " <<< "$held" || refuse "$tag has no $name for $arch"
    done
done
for arch in $(cut -d' ' -f1 <<< "$held" | sed '/^$/d' | sort -u); do
    for name in $expected; do
        grep "^$arch $name " <<< "$held" | cut -d' ' -f3-
    done
done
