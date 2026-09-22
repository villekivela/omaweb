#!/usr/bin/env bash
#
# Puts a built package into the pacman repository directory Omaweb serves, and
# leaves that directory ready to upload: the package, its detached signature,
# and the signed databases `pacman -Sy` reads.
#
# The repository holds one version of each package per architecture. The GitHub
# releases are this project's archive, and serving history would make the
# published site a second one that nobody maintains. `repo-add` replaces the
# database entry and this removes the package file that entry used to name, in
# the same run, so the directory never carries a version the database does not
# list.
#
# More than one package is served: the browser and the engine it runs on, which
# are released on different schedules (ADR 0049). So superseded means an older
# version of the same package, read from the package's own metadata, and never
# the other package that happens to be in the same directory.
#
#     scripts/publish_repo.sh --package <file.pkg.tar.zst> --repo-dir <dir> \
#         --key <signing key>
#
# The directory is served whole, so its layout is the URL a reader writes into
# `pacman.conf`: the architecture directory is the `$arch` their `Server` line
# resolves. One package at a time, so a release with a package per architecture
# calls this once per package and nothing here names an architecture it serves.

set -euo pipefail

package=""
repo_dir=""
key="${OMAWEB_REPO_KEY:-}"
repo_name="omaweb"

usage() {
    echo "usage: $0 --package <file> --repo-dir <dir> --key <signing key>" >&2
    exit 2
}

while (( $# > 0 )); do
    case "$1" in
        --package) package="${2:-}"; shift 2 ;;
        --repo-dir) repo_dir="${2:-}"; shift 2 ;;
        --key) key="${2:-}"; shift 2 ;;
        *) usage ;;
    esac
done

[[ -n "$package" && -n "$repo_dir" && -n "$key" ]] || usage
[[ -f "$package" ]] || { echo "publish_repo: no package at $package" >&2; exit 1; }

for tool in repo-add gpg bsdtar; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "publish_repo: $tool is not installed" >&2
        exit 1
    }
done

package=$(CDPATH= cd -- "$(dirname -- "$package")" && pwd)/$(basename -- "$package")
name=$(basename -- "$package")

# The architecture is in the package's own name, which is the only place it is
# recorded once the build is over. Reading it from there keeps a package from
# being filed under an architecture it was not built for.
arch="${name##*-}"
arch="${arch%%.pkg.tar.*}"
case "$arch" in
    x86_64 | aarch64 | any) ;;
    *)
        echo "publish_repo: $name names no architecture this repository serves" >&2
        exit 1
        ;;
esac

target="$repo_dir/$arch"
mkdir -p "$target"

echo "==> Signing $name"
# `--yes` so a re-run replaces its own previous signature rather than stopping
# to ask, which is what makes this script safe to run twice.
gpg --detach-sign --no-armor --yes --local-user "$key" \
    --output "$target/$name.sig" "$package"
cp -f "$package" "$target/$name"
gpg --verify "$target/$name.sig" "$target/$name"

echo "==> Adding it to the database"
# `--sign --key` signs the database as well. `--remove` is not passed: the
# entry is replaced by name, and the file the old entry named is removed below.
( cd "$target" && repo-add --quiet --sign --key "$key" \
    "$repo_name.db.tar.gz" "$name" )

echo "==> Removing superseded packages"
# Older versions of the package just added, and nothing else. A package built
# for a different architecture cannot be here: it is filed under its own
# directory above. A different package can be, and removing it would leave the
# database naming a file that is gone.
#
# The name comes out of `.PKGINFO` rather than off the front of the filename,
# because a filename is `pkgname-pkgver-pkgrel-arch` and a `pkgname` may carry
# dashes of its own, which is exactly the case here: `omaweb-qtwebengine`.
pkgname_of() {
    bsdtar -xOf "$1" .PKGINFO 2> /dev/null \
        | sed -n 's/^pkgname = //p' | head -n 1
}

published_name="$(pkgname_of "$package")"
if [[ -z "$published_name" ]]; then
    echo "publish_repo: $name carries no pkgname in its .PKGINFO" >&2
    exit 1
fi

removed=0
for stale in "$target"/*.pkg.tar.*; do
    [[ -e "$stale" ]] || continue
    case "$(basename -- "$stale")" in
        "$name" | "$name.sig") continue ;;
        *.sig) continue ;;
    esac
    [[ "$(pkgname_of "$stale")" == "$published_name" ]] || continue
    rm -f "$stale" "$stale.sig"
    removed=$(( removed + 1 ))
done
echo "publish_repo: removed $removed superseded version(s) of $published_name"

echo "==> Writing the names pacman asks for"
# `repo-add` writes `omaweb.db.tar.gz` and links `omaweb.db` to it. pacman
# requests the short name, and a static host serves files rather than resolving
# links, so each short name is written as a copy of the file it stood for.
# Without this the repository answers 404 for the only name a client asks for.
for db in "db" "files"; do
    for suffix in "" ".sig"; do
        real="$target/$repo_name.$db.tar.gz$suffix"
        [[ -f "$real" ]] || continue
        rm -f "$target/$repo_name.$db$suffix"
        cp -f "$real" "$target/$repo_name.$db$suffix"
    done
done

echo "==> Verifying what will be served"
gpg --verify "$target/$repo_name.db.sig" "$target/$repo_name.db"
gpg --verify "$target/$repo_name.files.sig" "$target/$repo_name.files"
# The database is read rather than trusted: a signed database listing nothing is
# a repository that verifies and installs no browser.
listed=$(bsdtar -tf "$target/$repo_name.db" | sed -n 's|^\([^/]*\)/$|\1|p')
if [[ -z "$listed" ]]; then
    echo "publish_repo: the database lists no package" >&2
    exit 1
fi

# The database has to name the file that is here. `repo-add` declines to replace
# an entry with an older version, and the removal above goes by the file it was
# given rather than by what the database settled on, so the two are checked
# against each other rather than assumed to agree. They disagree as a repository
# advertising a package it then answers 404 for.
while IFS= read -r entry; do
    # Matched by glob rather than by extension: the compression is
    # `makepkg.conf`'s to choose and a list of the ones seen so far would fail
    # a build that chose another.
    if ! find "$target" -maxdepth 1 -name "$entry-$arch.pkg.tar.*" \
        ! -name '*.sig' -print -quit | grep -q .; then
        echo "publish_repo: the database names $entry and no such package is here" >&2
        ls -1 "$target" >&2
        exit 1
    fi
done <<< "$listed"

echo "publish_repo: $target serves $listed"
