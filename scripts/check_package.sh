#!/bin/sh
# Builds the Arch packages, the browser and the small client, and checks that
# installing, upgrading and removing them leaves the system as it found it, and
# that the release before this one upgrades to them with a working `omaweb`.
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
# does, produces another package carrying the detached symbols. That one is
# meant to hold nothing but `usr/lib/debug`, so checking it would be checking
# the wrong package, and picking whichever the file system listed first is how
# that happens.
built_package() {
    found=$(find "$work" -maxdepth 1 -name "$1-[0-9]*.pkg.tar.*" ! -name '*-debug-*' \
        ! -name '*.sig' -print | sort)
    if [ -z "$found" ] || [ "$(printf '%s\n' "$found" | wc -l)" -ne 1 ]; then
        echo "makepkg produced no single $1 package:" >&2
        printf '%s\n' "$found" >&2
        exit 1
    fi
    printf '%s\n' "$found"
}
package=$(built_package omaweb-git)
client_package=$(built_package omaweb-cli-git)
echo "==> Built $package"
echo "==> Built $client_package"

# Each package carries what it lists and nothing outside the directories it may
# write to. `$1` is the package, `$2` its required files, one a line, and `$3`
# and `$4` patterns its other files must match one of.
check_contents() {
    contents=$(bsdtar -tf "$1")
    # Unquoted, to take the list a line at a time: no path in it has a space.
    for required in $2; do
        if ! printf '%s\n' "$contents" | grep -qx "$required"; then
            echo "$(basename "$1") is missing $required" >&2
            exit 1
        fi
    done
    unexpected=$(printf '%s\n' "$contents" | grep -v '/$' | grep -vE "$3" | grep -vE "${4:-$3}" \
        | grep -vE '^\.(PKGINFO|BUILDINFO|MTREE|INSTALL)$' || true)
    if [ -n "$unexpected" ]; then
        echo "$(basename "$1") carries files it should not:" >&2
        printf '%s\n' "$unexpected" >&2
        exit 1
    fi
}

echo "==> Checking what the packages carry"
# The browser is off the PATH, where the client stands in for it. The
# content-blocking library rides along, because it is not a distribution
# package and nothing else would supply it. The Sync module's askpass helper is
# in libexec because git runs it, not a person.
check_contents "$package" "usr/lib/omaweb/omaweb-browser
usr/lib/omaweb/libomaweb_content_blocker.so
usr/share/applications/omaweb.desktop
usr/share/icons/hicolor/scalable/apps/omaweb.svg
usr/share/licenses/omaweb/LICENSE
usr/share/licenses/omaweb/THIRD_PARTY_NOTICES.md
usr/share/omaweb/sbom.json
usr/share/omaweb/translations/omaweb_fi.qm" \
    '^usr/((lib|libexec)/omaweb|share/(applications|icons|licenses/omaweb))/' \
    '^usr/share/omaweb/(translations/|sbom\.json$)'
# The icon is installed under the name the desktop entry asks for, whichever
# file it was made from, so the list above cannot tell the black and white icon
# from another one. The launcher draws the black and white one.
if ! bsdtar -xOf "$package" usr/share/icons/hicolor/scalable/apps/omaweb.svg \
    | cmp -s - "$repo_root/assets/icons/omaweb-mono-rounded.svg"; then
    echo "The package's omaweb.svg is not assets/icons/omaweb-mono-rounded.svg" >&2
    exit 1
fi
# The Agent skill teaches the client, so it comes with the client.
check_contents "$client_package" "usr/bin/omaweb
usr/share/omaweb/skills/omaweb/SKILL.md
usr/share/licenses/omaweb-cli/LICENSE" \
    '^(usr/bin/omaweb$|usr/share/omaweb/skills/omaweb/|usr/share/licenses/omaweb-cli/)'

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
install_output=$(pacman -U --noconfirm $pacman_dep_flags "$package" "$client_package" 2>&1) || {
    printf '%s\n' "$install_output" >&2
    exit 1
}
printf '%s\n' "$install_output"
[ -x /usr/bin/omaweb ] || { echo "omaweb is not installed" >&2; exit 1; }
[ -x /usr/lib/omaweb/omaweb-browser ] || { echo "The browser is not installed" >&2; exit 1; }
# The window rule is the one thing an Omarchy reader has to do by hand, so the
# install has to say so. A silent install is the failure this catches.
if ! printf '%s\n' "$install_output" | grep -q 'tag = "-default-opacity"'; then
    echo "Installing did not print the Hyprland window rule" >&2
    exit 1
fi

echo "==> Upgrading over itself"
# shellcheck disable=SC2086
pacman -U --noconfirm $pacman_dep_flags "$package" "$client_package"

echo "==> Removing"
pacman -R --noconfirm omaweb-git omaweb-cli-git
for left in /usr/bin/omaweb /usr/lib/omaweb/omaweb-browser; do
    if [ -e "$left" ]; then
        echo "Removing the packages left $left behind" >&2
        exit 1
    fi
done

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

fail() {
    echo "$1" >&2
    exit 1
}

# A reader upgrades from the release before this one with `pacman -Syu`. Up to
# 0.10.0 one package, `omaweb`, owned /usr/bin/omaweb, which is now the
# client's: the upgrade has to move it from one package to the other and leave a
# command that works and a desktop entry that still opens addresses.
#
# The packages a repository serves are `omaweb` and `omaweb-cli`, so the release
# PKGBUILD is derived beside the source one, under the version this build
# carries, which pacman ranks above the release it came after. `makepkg -R`
# packages what was already built rather than building it again.
previous=$(git -C "$repo_root" describe --tags --abbrev=0 --match 'v[0-9]*' "$branch^")
version=$(git -C "$repo_root" describe --long --tags --match 'v[0-9]*' "$branch" \
    | sed 's/^v//;s/\([^-]*-g\)/r\1/;s/-/./g')
echo "==> Packaging this build as the release after $previous, $version"
"$repo_root/scripts/make_release_pkgbuild.sh" --version "$version" --output "$work"
chown -R builder "$work"
su builder -c "cd '$work' && makepkg -R -f --noconfirm $makepkg_dep_flags"
release=$(built_package omaweb)
release_client=$(built_package omaweb-cli)

echo "==> Fetching $previous, as a reader has it"
arch=$(uname -m)
mkdir -p "$work/previous"
# What the release attached for this architecture: the browser, and from the
# release that split it off on, the client the browser depends on. The two
# architectures' packages were compressed differently, and a release may have
# been rebuilt, so each name is looked for rather than assumed.
fetch_previous() {
    for release in 1 2 3; do
        for extension in zst xz; do
            name="$1-${previous#v}-$release-$arch.pkg.tar.$extension"
            if curl -fsL -o "$work/previous/$name" \
                "https://github.com/villekivela/omaweb/releases/download/$previous/$name"; then
                printf '%s\n' "$work/previous/$name"
                return 0
            fi
        done
    done
    return 1
}
previous_packages=$(fetch_previous omaweb) || fail "$previous has no $arch package to upgrade from"
if previous_client=$(fetch_previous omaweb-cli); then
    previous_packages="$previous_packages $previous_client"
fi
# shellcheck disable=SC2086
pacman -U --noconfirm $pacman_dep_flags $previous_packages
previous_owner=$(pacman -Qqo /usr/bin/omaweb) || fail "$previous did not install /usr/bin/omaweb"
echo "==> /usr/bin/omaweb is $previous_owner's"

echo "==> Upgrading $previous to $version"
# shellcheck disable=SC2086
pacman -U --noconfirm $pacman_dep_flags "$release" "$release_client"
[ "$(pacman -Qqo /usr/bin/omaweb)" = omaweb-cli ] \
    || fail "After the upgrade /usr/bin/omaweb is not the client's"
[ "$(pacman -Qqo /usr/lib/omaweb/omaweb-browser)" = omaweb ] \
    || fail "After the upgrade the browser is not omaweb's"
pacman -Qi omaweb | grep -q '^Depends On .*omaweb-cli=' \
    || fail "The browser's package does not depend on the client's"

# The client runs on Qt's base, and says where it looked when no browser
# answers.
answered=$(OMAWEB_CONTROL_SOCKET="$work/nobody.sock" omaweb spaces 2>&1) && status=0 || status=$?
[ "$status" -eq 3 ] || fail "omaweb spaces answered $status: $answered"
printf '%s\n' "$answered" | grep -qF "No Omaweb is answering on $work/nobody.sock" \
    || fail "omaweb spaces said: $answered"
# Anything not a verb is the browser's. Its version names the engine, which
# the client alone cannot, so this is the installed browser answering. The
# engine is here even where the dependency is assumed: the package was built
# against one.
reported=$(omaweb --version) || fail "omaweb --version failed: $reported"
printf '%s\n' "$reported" | grep -q '^QtWebEngine ' \
    || fail "omaweb --version did not reach the browser: $reported"

# xdg-open runs the desktop entry's command with the address. The entry names
# `omaweb`, which is now the client, and the client hands the address on, which
# `omaweb-agent-client` in the test suite shows with a browser that records it.
entry=/usr/share/applications/omaweb.desktop
[ "$(pacman -Qqo "$entry")" = omaweb ] || fail "The desktop entry is not omaweb's"
grep -qx 'Exec=omaweb %u' "$entry" || fail "The desktop entry no longer runs omaweb %u"
# Arch links /usr/sbin to /usr/bin, and either can come first on the PATH.
[ "$(readlink -f "$(command -v omaweb)")" = /usr/bin/omaweb ] \
    || fail "omaweb on the PATH is $(command -v omaweb)"
# Nothing is chosen here, so the answer comes from what the installed entries
# say they open, as it does for a reader who never chose a browser.
export XDG_CONFIG_HOME="$work/config"
mkdir -p "$XDG_CONFIG_HOME"
[ "$(xdg-mime query default x-scheme-handler/https)" = omaweb.desktop ] \
    || fail "xdg-mime does not answer https with omaweb.desktop"

pacman -R --noconfirm omaweb omaweb-cli
for left in /usr/bin/omaweb /usr/lib/omaweb/omaweb-browser; do
    [ ! -e "$left" ] || fail "Removing the upgraded packages left $left behind"
done

echo "==> $previous upgraded to $version with a working omaweb"
