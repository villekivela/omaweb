#!/bin/sh
# Installs Omaweb from its pacman repository. The website serves this file at /install:
#
#   curl -fsSL https://omaweb.app/install | sh
#   curl -fsSL https://omaweb.app/install | sh -s -- --yes
#
# It says what it will change, asks once, and then makes the README's manual steps with sudo: the
# [omaweb] block in /etc/pacman.conf, the signing key into pacman's keyring, and the package.
#
# Everything is inside main, called on the last line, so a download cut short runs nothing.
set -eu

# The key the packages are signed with, and the only thing the downloaded key is trusted by.
# scripts/check_repository_instructions.py holds this, the README and the website to
# security/repo-signing-key.asc.
FINGERPRINT=FDA535B2185755EA718BEA585DBF15FE484EFA64
KEY_ADDRESS=https://raw.githubusercontent.com/villekivela/omaweb/main/security/repo-signing-key.asc

# A test runs the script against a stand-in root holding etc/pacman.conf and dev/tty.
ROOT=${OMAWEB_INSTALL_ROOT:-}
CONF=$ROOT/etc/pacman.conf
TTY=$ROOT/dev/tty

say() {
    printf '%s\n' "$*"
}

fail() {
    printf 'omaweb: %s\n' "$@" >&2
    exit 1
}

# `$arch` stays literal: pacman expands it to the machine's architecture.
block() {
    # shellcheck disable=SC2016
    printf '[omaweb]\nSigLevel = Required DatabaseRequired\n%s\n' \
        'Server = https://github.com/villekivela/omaweb/releases/download/repo-$arch'
}

# Any [omaweb] section counts, including one the reader wrote by hand: theirs is left alone.
has_block() {
    grep -q '^[[:space:]]*\[omaweb\][[:space:]]*$' "$CONF"
}

main() {
    yes=no
    for argument in "$@"; do
        case $argument in
        --yes) yes=yes ;;
        *) fail "unknown option $argument; the one option is --yes" ;;
        esac
    done

    if ! command -v pacman >/dev/null 2>&1; then
        fail "this installs from a pacman repository, and this system has no pacman." \
            "it needs Arch Linux or a distribution built on it, such as Omarchy."
    fi

    for tool in curl gpg; do
        command -v "$tool" >/dev/null 2>&1 || fail "it needs $tool, which pacman -S $tool installs."
    done

    if [ "$(id -u)" -eq 0 ]; then
        as_root=
    elif command -v sudo >/dev/null 2>&1; then
        as_root=sudo
    else
        fail "the changes need root, and this system has no sudo. Run this as root instead."
    fi

    work=$(mktemp -d)
    trap 'rm -rf "$work"' EXIT
    key=$work/repo-signing-key.asc
    curl -fsSL -o "$key" "$KEY_ADDRESS" || fail "could not download the key from $KEY_ADDRESS"
    mkdir -m 700 "$work/gnupg"
    # Every primary key's fingerprint, one a line. pacman-key --add imports all of them, so the file
    # has to hold Omaweb's key and nothing else.
    fetched=$(gpg --batch --homedir "$work/gnupg" --show-keys --with-colons "$key" 2>/dev/null \
        | awk -F: '/^pub:/ { primary = 1; next } /^fpr:/ && primary { print $10; primary = 0 }')
    if [ "$fetched" != "$FINGERPRINT" ]; then
        found=$(printf '%s' "${fetched:-not a key}" | tr '\n' ' ')
        fail "the key from $KEY_ADDRESS is $found, not $FINGERPRINT." \
            "nothing was changed. Report this at https://github.com/villekivela/omaweb/issues."
    fi

    say "This installs Omaweb. It will:"
    say ""
    if has_block; then
        say "1. Leave $CONF as it is: it already has an [omaweb] section."
    else
        say "1. Add the Omaweb repository to $CONF, keeping a copy of the file as it was:"
        block | sed 's/^/     /'
    fi
    say ""
    say "2. Add the key the packages are signed with to pacman's keyring and sign it locally,"
    say "   unless that is done already. It came from"
    say "     $KEY_ADDRESS"
    say "   and is the one key it has to be:"
    say "     $FINGERPRINT"
    say ""
    say "3. Upgrade the whole system and install Omaweb, taking pacman's default answer to"
    say "   anything it would ask:"
    say "     pacman -Syu --needed --noconfirm omaweb"
    say ""

    if [ "$yes" = no ]; then
        # The script itself arrives on stdin, so the answer comes from the terminal. `true`, not
        # `:`: bash as sh exits on a failed redirection of a special builtin, without a word.
        { true <"$TTY"; } 2>/dev/null || fail "there is no terminal to ask on; run it with --yes."
        printf 'Go ahead? [y/N] '
        read -r answer <"$TTY" || answer=
        case $answer in
        y | Y | yes | Yes) ;;
        *) fail "nothing was changed." ;;
        esac
    fi

    if ! has_block; then
        backup=$CONF.before-omaweb-$(date +%Y%m%d%H%M%S)
        $as_root cp -p "$CONF" "$backup"
        { echo; block; } | $as_root tee -a "$CONF" >/dev/null
        say "Added the repository to $CONF, and kept the original as $backup."
    fi

    # </dev/null: under `| sh` stdin is the pipe the script came down, not the reader.
    # A local signature is a `sig` line flagged L, whatever certification level precedes it.
    # --init makes the local master key --lsign-key signs with, and only if there is none.
    if ! $as_root pacman-key --list-sigs "$FINGERPRINT" </dev/null 2>/dev/null \
        | grep -Eq '^sig[ 0-9]*L '; then
        $as_root pacman-key --init </dev/null
        if ! $as_root pacman-key --list-keys "$FINGERPRINT" </dev/null >/dev/null 2>&1; then
            $as_root pacman-key --add "$key" </dev/null
        fi
        $as_root pacman-key --lsign-key "$FINGERPRINT" </dev/null
    fi

    # --needed leaves an up-to-date omaweb alone, which a second run would otherwise reinstall.
    # --noconfirm because the reader has been asked once already, and step 3 said so.
    $as_root pacman -Syu --needed --noconfirm omaweb </dev/null
    say "Omaweb is installed. sudo pacman -Syu keeps it current."
}

main "$@"
