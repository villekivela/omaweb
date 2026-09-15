#!/usr/bin/env bash
#
# Checks the path a release takes to reach a reader's `pacman -Syu`: the binary
# PKGBUILD is derived, a package is signed, `repo-add` writes a signed database,
# and a pacman client that trusts only the signing key installs from it.
#
# The first real run of that path is a real release, and the only part of it a
# tag is needed for is the upload. Everything before the upload is run here, on
# every change, against a throwaway key and a scratch directory.
#
# The package this publishes is a stand-in that carries one file. What is under
# test is the publishing, and the browser is neither faster to build here nor
# different to `repo-add` than any other package; `scripts/check_package.sh`
# is what checks the real one.
#
#     scripts/check_repo_publish.sh
#
# Run as an ordinary user it derives, signs, publishes and verifies, and says
# that the client pass was skipped. Run as root it goes on to install from the
# repository with pacman. Nothing it does reaches the host: the keyring, the
# package database and the installation root are all inside its own scratch
# directory, so the root it needs is for pacman's own refusal to sync as a user
# rather than for anything it changes outside.

set -euo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=${OMAWEB_REPO_WORK:-$(mktemp -d)}
mkdir -p "$work"

# Its own short directory rather than one under the scratch tree. gpg-agent
# opens a Unix socket in GNUPGHOME, and a Unix socket path is 108 characters,
# so a keyring nested a few directories deep fails to start an agent at all.
# gpg hides that on a desktop by using /run/user/$UID instead, which is why the
# short path only turns out to matter under `sudo`, where that directory is not
# the one the session has.
GNUPGHOME=$(mktemp -d /tmp/omaweb-repo-key.XXXXXX)
export GNUPGHOME
chmod 700 "$GNUPGHOME"
# The keyring the pacman client is given, for the same reason: `pacman-key`
# starts an agent of its own in there.
client_gpg=$(mktemp -d /tmp/omaweb-repo-client.XXXXXX)
chmod 700 "$client_gpg"
trap 'for home in "$GNUPGHOME" "$client_gpg"; do
          gpgconf --homedir "$home" --kill gpg-agent >/dev/null 2>&1 || true
          rm -rf "$home"
      done' EXIT

arch=$(uname -m)

# makepkg refuses to run as root, and that covers reading a PKGBUILD as well as
# building one, so every call to it goes through somebody else. Under `sudo`
# that is the person who ran the script, and nothing about this check then
# reaches the host: the keyring, the package database and the installation root
# are all inside the scratch directory. A container has no such person, so root
# there makes one.
build_user="${SUDO_USER:-}"
if [ "$(id -u)" -eq 0 ] && [ -z "$build_user" ]; then
    id -u builder >/dev/null 2>&1 || useradd --create-home builder
    build_user=builder
fi

makepkg_in() {
    local dir="$1"
    shift
    if [ "$(id -u)" -eq 0 ]; then
        chown -R "$build_user" "$dir"
        # `su -` rather than `su`: makepkg writes into HOME, and without the
        # dash that is still root's.
        su - "$build_user" -c "cd '$dir' && makepkg $*"
    else
        ( cd "$dir" && eval "makepkg $*" )
    fi
}

echo "==> Deriving the binary PKGBUILD"
# Cheap, and it is the half of the split that goes stale silently: a dependency
# added to packaging/PKGBUILD in a form the derivation does not expect ships a
# package that does not run.
"$repo_root/scripts/make_release_pkgbuild.sh" --version 0.0.0 --output "$work/pkgbuild"
srcinfo=$(makepkg_in "$work/pkgbuild" --printsrcinfo)
# Matched whole, tabs included: `makepkg` indents everything under `pkgbase`,
# and a substring match for the name alone would be satisfied by omaweb-git.
required_lines=$(printf '%s\n' \
    "pkgbase = omaweb" \
    "	pkgver = 0.0.0" \
    "	conflicts = omaweb-git" \
    "	install = omaweb.install")
while IFS= read -r required; do
    if ! printf '%s\n' "$srcinfo" | grep -qxF "$required"; then
        echo "The derived PKGBUILD does not say '$required'" >&2
        printf '%s\n' "$srcinfo" >&2
        exit 1
    fi
done <<< "$required_lines"

echo "==> Making a throwaway signing key"
# `never` rather than an expiry, because a key that expires mid-run would fail
# the check for a reason that has nothing to do with the change being checked.
#
# No passphrase, and the loopback that lets gpg accept an empty one without
# asking. A key that lives for the length of one script in a directory the
# script made has nothing to protect, and gpg's default is to open a pinentry
# window and put the question to whoever ran the check.
gpg --batch --quiet --pinentry-mode loopback --passphrase "" \
    --quick-generate-key \
    "Omaweb repository check <check@omaweb.invalid>" default default never
fingerprint=$(gpg --list-secret-keys --with-colons | awk -F: '/^fpr:/ { print $10; exit }')
[ -n "$fingerprint" ] || { echo "No key was generated" >&2; exit 1; }
echo "==> Key $fingerprint"

echo "==> Building the stand-in packages"
# Two versions, because one release is not what the repository has to get right.
# A second release replaces the database entry and has to take the file the old
# entry named with it, or the directory grows a version nothing lists.
build_standin() {
    local version="$1"
    # Declared apart, because `local a=1 b=$a` does not see `a` under `set -u`.
    local dir="$work/build/$version"
    mkdir -p "$dir"
    cat > "$dir/PKGBUILD" <<PKGBUILD
# A stand-in for the browser package. Only its name, its version and the fact
# that it installs a file matter to what this script checks.
pkgname=omaweb
pkgver=$version
pkgrel=1
pkgdesc="A keyboard-driven web browser"
arch=('$arch')
url="https://github.com/villekivela/omaweb"
license=('MPL-2.0')

package() {
    install -Dm644 /dev/null "\$pkgdir/usr/share/omaweb/published-by-the-check"
}
PKGBUILD

    makepkg_in "$dir" -f --noconfirm >/dev/null

    local built
    built=$(find "$dir" -maxdepth 1 -name '*.pkg.tar.*' ! -name '*.sig' \
        ! -name '*-debug-*' -print | sort)
    if [ -z "$built" ] || [ "$(printf '%s\n' "$built" | wc -l)" -ne 1 ]; then
        echo "The stand-in build produced no single package:" >&2
        printf '%s\n' "$built" >&2
        exit 1
    fi
    printf '%s\n' "$built"
}

first=$(build_standin 0.0.0)
second=$(build_standin 0.0.1)

echo "==> Publishing"
"$repo_root/scripts/publish_repo.sh" --package "$first" \
    --repo-dir "$work/repo" --key "$fingerprint"

echo "==> Publishing the next version, as a second release would"
"$repo_root/scripts/publish_repo.sh" --package "$second" \
    --repo-dir "$work/repo" --key "$fingerprint"

held=$(find "$work/repo/$arch" -maxdepth 1 -name '*.pkg.tar.*' ! -name '*.sig' -print)
if [ "$(printf '%s\n' "$held" | wc -l)" -ne 1 ] \
    || [ "$(basename -- "$held")" != "$(basename -- "$second")" ]; then
    echo "The repository should hold the second version alone, and holds:" >&2
    find "$work/repo/$arch" -maxdepth 1 >&2
    exit 1
fi
if bsdtar -tf "$work/repo/$arch/omaweb.db" | grep -q '^omaweb-0\.0\.0-1/'; then
    echo "The database still lists the version that was replaced" >&2
    exit 1
fi

# pacman asks for these names and a static host serves files rather than
# following links, so each one has to be a file that is there.
for served in omaweb.db omaweb.db.sig omaweb.files omaweb.files.sig; do
    if [ ! -f "$work/repo/$arch/$served" ] || [ -L "$work/repo/$arch/$served" ]; then
        echo "The repository does not serve $served as a file" >&2
        exit 1
    fi
done

if [ "$(id -u)" -ne 0 ]; then
    echo "==> Not root, so the pacman client pass is skipped"
    echo "==> Signed, published and verified in $work"
    exit 0
fi

echo "==> Installing from the repository, as a reader would"
root="$work/client"
mkdir -p "$root/root" "$root/db" "$root/cache"
gpg --export --armor "$fingerprint" > "$work/signing-key.asc"
# The two commands the README will give a reader, run against this script's own
# keyring rather than the host's.
pacman-key --gpgdir "$client_gpg" --init >/dev/null
pacman-key --gpgdir "$client_gpg" --add "$work/signing-key.asc" >/dev/null
pacman-key --gpgdir "$client_gpg" --lsign-key "$fingerprint" >/dev/null

cat > "$root/pacman.conf" <<CONF
[options]
Architecture = $arch
# What the README will tell a reader to write. An unsigned package, or one
# signed by a key the reader has not signed, is refused rather than installed.
SigLevel = Required DatabaseRequired

[omaweb]
Server = file://$work/repo/\$arch
CONF

pacman --config "$root/pacman.conf" --root "$root/root" --dbpath "$root/db" \
    --cachedir "$root/cache" --gpgdir "$client_gpg" --logfile "$root/pacman.log" \
    --noconfirm -Sy omaweb
if [ ! -f "$root/root/usr/share/omaweb/published-by-the-check" ]; then
    echo "pacman reported success and installed nothing" >&2
    exit 1
fi

echo "==> A client that trusts only the signing key installed from the repository"
