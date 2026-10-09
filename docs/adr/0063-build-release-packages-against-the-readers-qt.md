# Build release packages against the readers' Qt

Amends [0049](0049-ship-omawebs-own-engine-build.md), which ships Omaweb's own build of QtWebEngine,
and [0044](0044-build-the-aarch64-package-on-arch-linux-arm.md), which builds the aarch64 package on
Arch Linux ARM. Decided with the maintainer on 2026-10-08 in
[#674](https://github.com/villekivela/omaweb/issues/674).

Release packages, Omaweb's and the engine's, are built against the Qt Omaweb's readers have. On
x86_64 that is Omarchy's stable mirror, `stable-mirror.omarchy.org`, the delayed snapshot of Arch
that Omarchy machines install from. On aarch64 it is Arch Linux ARM, which the package is already
built on. No package carries a Qt version floor. CI's ordinary jobs keep building and testing
against Arch's current Qt, so what the next Qt breaks shows there before readers have it.

## Why

A binary built against an older Qt runs on a newer one, and not the other way round. A program
linked against Qt records the symbol version of each Qt symbol it uses, `Qt_6.12` for one that Qt
6.12 added, and the loader refuses to start it on a Qt without that version.

Arch's `[extra]` moved to Qt 6.12.0 on 2026-10-07, while the stable mirror stayed on 6.11.2. v0.13.0
and engine 6.11.2-6 were built against Arch's 6.12.0. The engine required `qt6-base>=6.12.0`, so
pacman held both packages back on every Omarchy machine, and v0.13.0's binaries needed `Qt_6.12`, so
they would not have started even if the engine had resolved. Omarchy readers stayed on v0.12.1 and
engine 6.11.2-5, which still crashes when an extension makes an offscreen document
([#646](https://github.com/villekivela/omaweb/issues/646)).

[#660](https://github.com/villekivela/omaweb/issues/660) had set the engine's floor at the Qt it was
built against, so that pacman would bring the two together. That holds on a machine that follows
Arch, and it holds Omarchy machines back for as long as the mirror trails Arch.

## How it is kept

- The Release workflow's x86_64 package job installs from the stable mirror, and fails the release
  when a packaged binary needs a `Qt_6.x` symbol version newer than the Qt the mirror serves
  (`scripts/check_qt_symbol_versions.sh`). The mirror's Qt is read apart from the build's own
  pacman, because the Qt that linked a binary always defines every version it needs.
- The engine's x86_64 build, local or on Hetzner, installs from the same mirror. Its PKGBUILD names
  `qt6-base` and `qt6-declarative` without a version, and its `PROCESS.md` says so.

## Consequences

- The mirror trails Arch by weeks. When the mirror reaches a new Qt minor, the engine is rebuilt
  against it as its next pkgrel, the same as it was on Arch's schedule before.
- Readers who follow Arch run binaries built against an older Qt than theirs. Qt keeps that working
  for its public API. The engine uses Qt's private API, which Qt does not promise across a minor, so
  a reader on a newer Qt minor runs an engine built for the older one until the mirror moves.
- The mirror serves x86_64 only. An aarch64 build has no mirror to follow and stays on Arch Linux
  ARM.
