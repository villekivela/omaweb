#!/usr/bin/env python3
"""The one-line install, run against a stand-in root.

`scripts/install.sh` is what `curl -fsSL https://omaweb.app/install | sh` runs on a reader's
machine, as root through `sudo`, editing `/etc/pacman.conf` and the pacman keyring. Here it runs
against a temporary root instead: `OMAWEB_INSTALL_ROOT` moves `etc/pacman.conf` and `dev/tty` into
it, and `pacman`, `pacman-key`, `sudo` and `curl` are stubs that log what they were asked. The
`PATH` holds only the stubs and the few tools the script uses, so a machine that has pacman still
tests the machine that has not.

`gpg` is the real one, because checking the key's fingerprint is the part of the script that has to
be right, and a stub would check nothing.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "install.sh"
KEY = ROOT / "security" / "repo-signing-key.asc"
sys.path.insert(0, str(ROOT / "scripts"))

import check_repository_instructions as instructions  # noqa: E402

# What README.md, the website and security/repo-signing-key.asc agree on. Written out rather than
# read from the script, so the test can disagree with it.
PUBLISHED = "FDA535B2185755EA718BEA585DBF15FE484EFA64"

# What the script may call besides the stubs. Linked into the stand-in's own bin directory rather
# than reached through the host's PATH, which on Arch also holds the real pacman.
# The shells a reader's `sh` may be: bash on Arch, dash on Debian and Ubuntu, which have no pacman
# to install with but are where a reader is told so. The tests that differ between them run in each
# one present.
SHELLS = tuple(shell for shell in ("sh", "dash", "bash") if shutil.which(shell))
TOOLS = (*SHELLS, "cat", "cp", "date", "grep", "mkdir", "mktemp", "sed", "rm", "tee", "gpg", "awk", "tr")

KEY_ADDRESS = (
    "https://raw.githubusercontent.com/villekivela/omaweb/main/security/repo-signing-key.asc"
)

# `$arch` is literal, as in the README: pacman expands it.
BLOCK = """
[omaweb]
SigLevel = Required DatabaseRequired
Server = https://github.com/villekivela/omaweb/releases/download/repo-$arch
"""

PACMAN_CONF = """\
[options]
HoldPkg     = pacman glibc
Architecture = auto

[core]
Include = /etc/pacman.d/mirrorlist

[extra]
Include = /etc/pacman.d/mirrorlist
"""

# Each stub appends its name and arguments to the log. `pacman-key` keeps the keyring as a file of
# fingerprints, one per line, and the local signatures as another, which is as much of a keyring as
# the script can observe. `--list-sigs` prints a local signature the way gpg does, flagged `L`.
STUBS = {
    # A reader, not root, so every change goes through sudo.
    "id": 'if [ "$1" = -u ]; then echo 1000; else exit 1; fi\n',
    "sudo": 'echo "sudo $*" >> "$STAND_IN/log"\nexec "$@"\n',
    "pacman": 'echo "pacman $*" >> "$STAND_IN/log"\n',
    "curl": (
        'echo "curl $*" >> "$STAND_IN/log"\n'
        'while [ $# -gt 1 ]; do\n'
        '    if [ "$1" = -o ]; then cp "$STAND_IN/served-key.asc" "$2"; exit 0; fi\n'
        "    shift\n"
        "done\n"
        'cat "$STAND_IN/served-key.asc"\n'
    ),
    "pacman-key": (
        'echo "pacman-key $*" >> "$STAND_IN/log"\n'
        'keys="$STAND_IN/keyring"; signed="$STAND_IN/signed"\n'
        'touch "$keys" "$signed"\n'
        'case "$1" in\n'
        "    --init) ;;\n"
        '    --add) gpg --batch --homedir "$STAND_IN/gnupg" --show-keys --with-colons "$2" '
        "| awk -F: '/^fpr:/ { print $10; exit }' >> \"$keys\" ;;\n"
        '    --lsign-key) grep -qx "$2" "$keys" && echo "$2" >> "$signed" ;;\n'
        '    --list-keys) grep -qx "$2" "$keys" ;;\n'
        '    --list-sigs) grep -qx "$2" "$keys" || exit 1\n'
        '        if grep -qx "$2" "$signed"; then\n'
        '            echo "sig   L        ${2#????????????????????????} 2026-10-02  '
        'Pacman Keyring Master Key <pacman@localhost>"\n'
        "        fi ;;\n"
        "esac\n"
    ),
}


def stub(directory: Path, name: str, body: str) -> None:
    path = directory / name
    path.write_text("#!/bin/sh\n" + body, encoding="utf-8")
    path.chmod(0o755)


class StandIn:
    """A temporary root with a pacman.conf, a terminal and the stubs."""

    def __init__(self, directory: Path, *, pacman: bool = True, key: Path = KEY):
        self.directory = directory
        self.bin = directory / "bin"
        self.bin.mkdir()
        (directory / "etc").mkdir()
        (directory / "dev").mkdir()
        (directory / "gnupg").mkdir(mode=0o700)
        self.conf.write_text(PACMAN_CONF, encoding="utf-8")
        shutil.copy(key, directory / "served-key.asc")
        for tool in TOOLS:
            found = shutil.which(tool)
            if found:
                (self.bin / tool).symlink_to(found)
        for name, body in STUBS.items():
            if name == "pacman" and not pacman:
                continue
            stub(self.bin, name, body)

    @property
    def conf(self) -> Path:
        return self.directory / "etc" / "pacman.conf"

    @property
    def log(self) -> list[str]:
        path = self.directory / "log"
        return path.read_text(encoding="utf-8").splitlines() if path.exists() else []

    def backups(self) -> list[Path]:
        return sorted(p for p in (self.directory / "etc").iterdir() if p.name != "pacman.conf")

    def run(
        self, *arguments: str, answer: str | None = "y\n", shell: str = "sh"
    ) -> subprocess.CompletedProcess:
        """Runs the script the way `curl ... | sh` does: the script on stdin, the reader's answer
        on the terminal. `answer=None` is a run with no terminal at all."""
        tty = self.directory / "dev" / "tty"
        tty.unlink(missing_ok=True)
        if answer is not None:
            tty.write_text(answer, encoding="utf-8")
        environment = {
            "PATH": str(self.bin),
            "HOME": str(self.directory),
            "STAND_IN": str(self.directory),
            "OMAWEB_INSTALL_ROOT": str(self.directory),
        }
        return subprocess.run(
            (str(self.bin / shell), "-s", "--", *arguments),
            input=SCRIPT.read_text(encoding="utf-8"),
            env=environment,
            capture_output=True,
            text=True,
            timeout=60,
        )


@unittest.skipUnless(shutil.which("gpg"), "the fingerprint check needs gpg")
class Install(unittest.TestCase):
    def stand_in(self, **options) -> StandIn:
        return StandIn(Path(self.enterContext(tempfile.TemporaryDirectory())), **options)

    def test_a_system_without_pacman_is_refused_by_name(self):
        for shell in SHELLS:
            with self.subTest(shell):
                machine = self.stand_in(pacman=False)
                result = machine.run(shell=shell)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("pacman", result.stderr)
                self.assertIn("Arch", result.stderr)
                self.assertEqual(machine.conf.read_text(encoding="utf-8"), PACMAN_CONF)
                self.assertEqual(machine.log, [])

    def test_a_first_install_adds_the_repository_trusts_the_key_and_installs(self):
        for shell in SHELLS:
            with self.subTest(shell):
                machine = self.stand_in()
                result = machine.run(shell=shell)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

                self.assertEqual(machine.conf.read_text(encoding="utf-8"), PACMAN_CONF + BLOCK)
                [backup] = machine.backups()
                self.assertEqual(backup.read_text(encoding="utf-8"), PACMAN_CONF)

                [download] = [line for line in machine.log if line.startswith("curl ")]
                self.assertIn("-fsSL", download.split())
                self.assertEqual(download.split()[-1], KEY_ADDRESS)
                sudo = [line for line in machine.log if line.startswith("sudo ")]
                self.assertIn(f"sudo pacman-key --lsign-key {PUBLISHED}", sudo)
                self.assertTrue(any(line.startswith("sudo pacman-key --add ") for line in sudo))
                self.assertEqual(sudo[-1], "sudo pacman -Syu --needed --noconfirm omaweb")

    def test_it_says_what_it_will_do_and_asks_once_before_doing_it(self):
        machine = self.stand_in()
        result = machine.run()
        for line in BLOCK.strip().splitlines():
            self.assertIn(line, result.stdout)
        self.assertIn(PUBLISHED, result.stdout)
        self.assertIn("pacman -Syu omaweb", result.stdout)
        self.assertEqual(result.stdout.count("[y/N]"), 1)

    def test_an_answer_other_than_yes_changes_nothing(self):
        machine = self.stand_in()
        result = machine.run(answer="n\n")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(machine.conf.read_text(encoding="utf-8"), PACMAN_CONF)
        self.assertEqual(machine.backups(), [])
        self.assertEqual([line for line in machine.log if line.startswith("sudo ")], [])

    def test_a_second_run_changes_nothing_the_first_did(self):
        machine = self.stand_in()
        self.assertEqual(machine.run().returncode, 0)
        conf = machine.conf.read_bytes()
        backups = machine.backups()
        first = len(machine.log)

        result = machine.run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(machine.conf.read_bytes(), conf)
        self.assertEqual(machine.backups(), backups)
        again = machine.log[first:]
        changes = [
            line
            for line in again
            if line.startswith(("sudo cp", "sudo tee", "sudo pacman-key --add", "sudo pacman-key --lsign"))
        ]
        self.assertEqual(changes, [])
        # --needed: pacman upgrades what is out of date and leaves an up-to-date omaweb alone.
        self.assertEqual(again[-1], "pacman -Syu --needed --noconfirm omaweb")

    def test_a_block_the_reader_added_by_hand_is_left_as_it_is(self):
        machine = self.stand_in()
        by_hand = PACMAN_CONF + "\n[omaweb]\nServer = https://example.org/$arch\n"
        machine.conf.write_text(by_hand, encoding="utf-8")
        self.assertEqual(machine.run().returncode, 0)
        self.assertEqual(machine.conf.read_text(encoding="utf-8"), by_hand)
        self.assertEqual(machine.backups(), [])

    def test_a_key_added_but_never_signed_is_signed(self):
        """A first run stopped between --add and --lsign-key: the key is there, untrusted."""
        machine = self.stand_in()
        (machine.directory / "keyring").write_text(PUBLISHED + "\n", encoding="utf-8")
        self.assertEqual(machine.run().returncode, 0)
        self.assertIn(f"sudo pacman-key --lsign-key {PUBLISHED}", machine.log)
        self.assertFalse(any(line.startswith("sudo pacman-key --add") for line in machine.log))

    def test_a_key_with_another_fingerprint_stops_it_before_any_change(self):
        machine = self.stand_in(key=self.another_key())
        result = machine.run()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn(PUBLISHED, result.stderr)
        self.assertNotIn("[y/N]", result.stdout)
        self.assertEqual(machine.conf.read_text(encoding="utf-8"), PACMAN_CONF)
        self.assertEqual(machine.backups(), [])
        self.assertEqual([line for line in machine.log if not line.startswith("curl ")], [])

    def test_yes_installs_without_asking(self):
        machine = self.stand_in()
        result = machine.run("--yes", answer=None)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[y/N]", result.stdout)
        self.assertEqual(machine.conf.read_text(encoding="utf-8"), PACMAN_CONF + BLOCK)
        self.assertEqual(machine.log[-1], "pacman -Syu --needed --noconfirm omaweb")

    def test_without_a_terminal_or_yes_it_asks_for_yes_and_changes_nothing(self):
        for shell in SHELLS:
            with self.subTest(shell):
                machine = self.stand_in()
                result = machine.run(answer=None, shell=shell)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("--yes", result.stderr)
                self.assertEqual(machine.conf.read_text(encoding="utf-8"), PACMAN_CONF)
                self.assertEqual([line for line in machine.log if line.startswith("sudo ")], [])

    def another_key(self) -> Path:
        """A real public key that is not Omaweb's, made for this test."""
        home = Path(self.enterContext(tempfile.TemporaryDirectory()))
        home.chmod(0o700)
        gpg = ("gpg", "--batch", "--homedir", str(home), "--passphrase", "")
        subprocess.run(
            (*gpg, "--quick-gen-key", "Not Omaweb <not@omaweb.app>", "ed25519", "sign", "never"),
            check=True,
            capture_output=True,
        )
        exported = home / "another.asc"
        exported.write_bytes(
            subprocess.run(
                (*gpg, "--armor", "--export"), check=True, capture_output=True
            ).stdout
        )
        return exported


class PinnedFingerprint(unittest.TestCase):
    def test_the_script_pins_the_key_in_security(self):
        """The one fingerprint the script holds a download to is the shipped key's."""
        pinned = [
            line.split("=", 1)[1]
            for line in SCRIPT.read_text(encoding="utf-8").splitlines()
            if line.startswith("FINGERPRINT=")
        ]
        self.assertEqual(pinned, [instructions.key_fingerprint(instructions.KEY)])
        self.assertEqual(pinned, [PUBLISHED])


if __name__ == "__main__":
    unittest.main()
