#!/bin/sh
# Adds the [omaweb] pacman repository to this machine and trusts its signing
# key, so that `pacman -S omaweb-qtwebengine` installs Omaweb's own engine.
#
#   scripts/trust_omaweb_repository.sh
#
# For the disposable containers that build against that engine: the release
# workflow, which has to compile against the engine it ships on, and CI's
# `arch-linux` job, which tests against it (ADR 0049). Both run it as root.
#
# The public half of the key is `security/repo-signing-key.asc` in the checkout
# rather than fetched, because a keyserver that does not answer would fail a
# build for a reason that has nothing to do with the build.
set -eu

# What the README and the website tell a reader to check. Held against the key
# file so that a replaced file is a failure rather than a silently different
# key. `scripts/check_repository_instructions.py` holds all of them together.
PUBLISHED=FDA535B2185755EA718BEA585DBF15FE484EFA64

cd "$(dirname "$0")/.."

fingerprint=$(gpg --show-keys --with-colons security/repo-signing-key.asc \
    | awk -F: '/^fpr:/ { print $10; exit }')
if [ "$fingerprint" != "$PUBLISHED" ]; then
    echo "::error::security/repo-signing-key.asc is $fingerprint, not $PUBLISHED" >&2
    exit 1
fi

# The image's keyring has Arch's keys but no local master key, and --lsign-key
# signs with that key. scripts/check_repo_publish.sh initialises its client
# keyring the same way.
pacman-key --init
pacman-key --add security/repo-signing-key.asc
pacman-key --lsign-key "$fingerprint"

# `$arch` stays literal: pacman expands it to the architecture the client asks
# for, which is how one Server line serves both.
# shellcheck disable=SC2016
{
    echo
    echo "[omaweb]"
    echo "SigLevel = Required DatabaseRequired"
    echo 'Server = https://github.com/villekivela/omaweb/releases/download/repo-$arch'
} >> /etc/pacman.conf
