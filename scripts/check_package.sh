#!/bin/sh
# Builds the Arch package and checks that installing, upgrading and removing it
# leaves the system as it found it.
#
# Installing needs root and touches the package database, so this refuses to run
# unless it is somewhere disposable: a container, or a host that has said so
# with OMAWEB_PACKAGE_SMOKE_ALLOW_HOST=1. The intended home is Linux CI, where
# the container is the disposable thing.
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=${OMAWEB_PACKAGE_WORK:-$(mktemp -d)}
branch=$(git -C "$repo_root" rev-parse --abbrev-ref HEAD)

if [ "$(id -u)" -eq 0 ] && [ ! -f /run/.containerenv ] && [ ! -f /.dockerenv ] \
    && [ "${OMAWEB_PACKAGE_SMOKE_ALLOW_HOST:-0}" != "1" ]; then
    echo "Refusing to install packages on a host that is not disposable." >&2
    echo "Run this in a container, or set OMAWEB_PACKAGE_SMOKE_ALLOW_HOST=1." >&2
    exit 2
fi

# The engine Omaweb runs on is a package of its own, published in Omaweb's
# pacman repository rather than in Arch's (ADR 0049). Where this runs with that
# repository configured the dependency resolves and the install is a reader's.
# Where it does not, which is a CI container on an architecture whose engine has
# not been built yet, the dependency is assumed and this check covers packaging
# mechanics alone. It says which of the two it did rather than hiding it, and
# needs no flag to stop assuming: publishing the engine is what stops it.
engine=omaweb-qtwebengine
if pacman -Si "$engine" > /dev/null 2>&1; then
    makepkg_dep_flags=""
    pacman_dep_flags=""
    echo "==> $engine is available, so the dependency is resolved for real"
else
    makepkg_dep_flags="-d"
    pacman_dep_flags="--assume-installed $engine"
    echo "==> $engine is in no configured repository, so it is assumed"
    echo "==> This run checks packaging and not that the browser can start"
fi

echo "==> Building the package"
mkdir -p "$work"
cp "$repo_root/packaging/PKGBUILD" "$work/PKGBUILD"
# makepkg reads the install file from the build directory, not from the
# repository, so it has to travel with the PKGBUILD that names it.
cp "$repo_root/packaging/omaweb-git.install" "$work/omaweb-git.install"
# The published source is the repository on GitHub. A check has to build what is
# in front of it instead, including work that has not been pushed.
sed -i "s|^source=.*|source=(\"\$_pkgname::git+file://$repo_root#branch=$branch\")|" \
    "$work/PKGBUILD"

# makepkg refuses to run as root, so a container's root builds as a user it
# makes for the purpose.
if [ "$(id -u)" -eq 0 ]; then
    id -u builder >/dev/null 2>&1 || useradd --create-home builder
    chown -R builder "$work"
    su builder -c "cd '$work' && makepkg -f --noconfirm $makepkg_dep_flags"
else
    ( cd "$work" && makepkg -f --noconfirm $makepkg_dep_flags )
fi

# A host whose `makepkg.conf` enables `debug`, which Arch's own build container
# does, produces a second package carrying the detached symbols. That one is
# meant to hold nothing but `usr/lib/debug`, so checking it would be checking
# the wrong package, and picking whichever the file system listed first is how
# that happens.
package=$(find "$work" -maxdepth 1 -name '*.pkg.tar.*' ! -name '*-debug-*' -print | sort)
if [ -z "$package" ]; then
    echo "makepkg produced no package" >&2
    exit 1
fi
if [ "$(printf '%s\n' "$package" | wc -l)" -ne 1 ]; then
    echo "makepkg produced more than one package, so it is unclear which to check:" >&2
    printf '%s\n' "$package" >&2
    exit 1
fi
echo "==> Built $package"

echo "==> Checking what the package carries"
contents=$(bsdtar -tf "$package")
for required in \
    usr/bin/omaweb \
    usr/share/applications/omaweb.desktop \
    usr/share/icons/hicolor/scalable/apps/omaweb.svg \
    usr/share/licenses/omaweb/LICENSE \
    usr/share/licenses/omaweb/THIRD_PARTY_NOTICES.md \
    usr/share/omaweb/sbom.json; do
    if ! printf '%s\n' "$contents" | grep -qx "$required"; then
        echo "The package is missing $required" >&2
        exit 1
    fi
done
# The content-blocking library rides along, because it is not a distribution
# package and nothing else would supply it.
if ! printf '%s\n' "$contents" | grep -q '^usr/lib/omaweb/libomaweb_content_blocker\.so$'; then
    echo "The package is missing the content-blocking library" >&2
    exit 1
fi
# Anything outside these is a file the package has no business owning. The
# Sync module's askpass helper is in libexec because git runs it, not a person.
unexpected=$(printf '%s\n' "$contents" | grep -v '/$' \
    | grep -vE '^(usr/bin/|usr/lib/omaweb/|usr/libexec/omaweb/|usr/share/(applications|icons|licenses|omaweb)/)' \
    | grep -vE '^\.(PKGINFO|BUILDINFO|MTREE|INSTALL)$' || true)
if [ -n "$unexpected" ]; then
    echo "The package carries files it should not:" >&2
    printf '%s\n' "$unexpected" >&2
    exit 1
fi

if ! command -v pacman >/dev/null 2>&1 || [ "$(id -u)" -ne 0 ]; then
    echo "==> Not root, so the install, upgrade and remove pass is skipped"
    echo "==> Package contents are correct"
    exit 0
fi

# What the acceptance criterion asks about: whether a reader's own files come
# through an install, an upgrade and a removal untouched.
witness=$HOME/.config/omaweb-package-witness
mkdir -p "$(dirname "$witness")"
echo "written before the package existed" > "$witness"
before=$(find /etc /usr/share/applications -type f 2>/dev/null | sort | md5sum)

echo "==> Installing"
# Unquoted, because an empty value has to disappear rather than become an
# argument pacman reads as a package name.
# shellcheck disable=SC2086
install_output=$(pacman -U --noconfirm $pacman_dep_flags "$package" 2>&1) || {
    printf '%s\n' "$install_output" >&2
    exit 1
}
printf '%s\n' "$install_output"
[ -x /usr/bin/omaweb ] || { echo "omaweb is not installed" >&2; exit 1; }
# The window rule is the one thing an Omarchy reader has to do by hand, so the
# install has to say so. A silent install is the failure this catches.
if ! printf '%s\n' "$install_output" | grep -q 'tag = "-default-opacity"'; then
    echo "Installing did not print the Hyprland window rule" >&2
    exit 1
fi

echo "==> Upgrading over itself"
# shellcheck disable=SC2086
pacman -U --noconfirm $pacman_dep_flags "$package"

echo "==> Removing"
pacman -R --noconfirm omaweb-git
if [ -e /usr/bin/omaweb ]; then
    echo "Removing the package left the browser behind" >&2
    exit 1
fi

if [ "$(cat "$witness")" != "written before the package existed" ]; then
    echo "The package modified a file in the reader's configuration" >&2
    exit 1
fi
rm -f "$witness"

after=$(find /etc /usr/share/applications -type f 2>/dev/null | sort | md5sum)
if [ "$before" != "$after" ]; then
    echo "Installing and removing the package left the system changed" >&2
    exit 1
fi

echo "==> Installed, upgraded and removed cleanly"
