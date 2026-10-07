# Roadmap

Omaweb is alpha. The daily-driver contract in `GLOSSARY.md` holds on Linux, and every release since
[v0.2.0](https://github.com/villekivela/omaweb/releases/tag/v0.2.0) carries the Arch package that
proves it. A version number records what a build is compatible with, not how finished it is
([ADR 0028](adr/0028-derive-the-version-from-the-release-tag.md)). This page states the stage and
the order of what is planned. A release's milestone lists what it contains, and its release notes
list what shipped.

## Planned

Releases are planned as [milestones](https://github.com/villekivela/omaweb/milestones) on the issue
tracker. [v0.10.0](https://github.com/villekivela/omaweb/releases/tag/v0.10.0) shipped localization:
every user-facing string wrapped, the first locale finished, and new strings gated.
[v0.11.0](https://github.com/villekivela/omaweb/releases/tag/v0.11.0) shipped forms and autofill:
what was typed remembered, addresses and payment cards filled, the cards kept in the desktop's
keyring ([ADR 0053](adr/0053-keep-payment-cards-in-the-secret-service.md)), and signing in with a
USB security key. It also shipped `omaweb dev`, the `omaweb` client that drives the browser from a
container or another host, Site information as a card from the address, and the chrome resting on
whole pixels. [v0.12.0](https://github.com/villekivela/omaweb/releases/tag/v0.12.0) shipped
performance and the chrome: the 6.11.2 engine rebuilt for speed, with V8's write barriers on and, on
x86_64, clang, LLD and ThinLTO
([ADR 0060](adr/0060-build-the-x86-64-engine-with-clang-and-thinlto.md)), the browser's missing
measurements with what came in over budget fixed, jumping between tabs with `Ctrl+O` and `Ctrl+I`,
the sidebar on the right, and five more Start page Scenes.
[v0.12.1](https://github.com/villekivela/omaweb/releases/tag/v0.12.1) shipped the black and white
icon in the launcher, the active tab marked the way the Omnibar marks its selected row, and the
release mark no longer hidden under the extension mark. v0.13.0 is the release in progress.

[v0.13.0](https://github.com/villekivela/omaweb/milestone/9) is configuration as files and the
chrome. The reader's settings move into one file in the configuration directory that they can keep
in their dotfiles ([#389](https://github.com/villekivela/omaweb/issues/389)), and three settings
build on it: the app icon chosen in Settings
([#635](https://github.com/villekivela/omaweb/issues/635)), Space colours that Omaweb owns rather
than the theme ([#631](https://github.com/villekivela/omaweb/issues/631)), and the sidebar's opacity
on a slider ([#643](https://github.com/villekivela/omaweb/issues/643)). Beside them, the sidebar's
address field is sized like a tab row ([#632](https://github.com/villekivela/omaweb/issues/632)),
the Omnibar drops its hint row at rest ([#630](https://github.com/villekivela/omaweb/issues/630)),
and the night sky Scene's planet and comets change
([#629](https://github.com/villekivela/omaweb/issues/629)).

What makes Omaweb beta is not yet decided. The milestones are the planned work, not a gate.

## Open on Linux

- Becoming the default browser is the one check in the Wayland validation sweep
  ([#103](https://github.com/villekivela/omaweb/issues/103)) still to be run, because it changes the
  machine that runs it. `scripts/check_default_browser.py` drives it where that is allowed.
- Composing through an input method needs an input method that composes, and a plain keyboard layout
  is not one. With `fcitx5-qt` installed Qt reaches fcitx5 over D-Bus rather than binding
  `zwp_text_input_v3`, which is the plugin's design rather than a fault.

Linux is the only platform CI builds. macOS remains a development and test platform, and Omaweb does
not distribute its bundles ([ADR 0029](adr/0029-distribute-only-for-linux.md)).

## Waiting on upstream or deferred

- The Omarchy window rule ([#75](https://github.com/villekivela/omaweb/issues/75)) and the component
  kit ([#9](https://github.com/villekivela/omaweb/issues/9),
  [#13](https://github.com/villekivela/omaweb/issues/13)) wait on Omarchy.
- What a reader could extend in Omaweb's own chrome waits on the command registry question
  ([#175](https://github.com/villekivela/omaweb/issues/175)).
- The Ladybird adapter ([#7](https://github.com/villekivela/omaweb/issues/7)) stays experimental and
  outside the default build graph until it satisfies the daily-driver contract.
- The engine's move to QtWebEngine 6.140.0
  ([#484](https://github.com/villekivela/omaweb/issues/484)) waits on Qt's final release, and so
  does what is built on it: an engine patch that gives an Agent tab its own debugging session, so an
  Agent reads the tab's network ([#373](https://github.com/villekivela/omaweb/issues/373)) and, in
  an Agent Space, the page's main world ([#534](https://github.com/villekivela/omaweb/issues/534)).
  They join the release that is open when 6.140.0 is out.
- Opening links from outside in the Space a rule names
  ([#388](https://github.com/villekivela/omaweb/issues/388)) is deferred to a later release.
- Account and Sync with replaceable providers are deferred.

## Known extensions

The release loop: the daily baseline check assigns an issue when Qt publishes, a daily check in the
patch repository says whether the series still applies and reports to this tracker,
`/engine-release` qualifies the series on whichever machine you are at, and a workflow started by
hand builds the engine on rented Linux machines. Packaging and signing stay local.

Omaweb ships its own QtWebEngine build so a password manager's extension runs
([ADR 0049](adr/0049-ship-omawebs-own-engine-build.md),
[#344](https://github.com/villekivela/omaweb/issues/344)). The patch series and the build process
live outside this repository. Both Known extensions now fill a login form on Linux aarch64,
Bitwarden from its own vault and 1Password through its desktop application, so the Settings surface,
the popup surface and package acquisition that keeps a store extension's identity are proved rather
than built. What remains before a reader sees this: the engine package and its Linux builders. The
five engine bug fixes go to Qt in parallel, and the day Qt carries the rest the series is deleted
([#288](https://github.com/villekivela/omaweb/issues/288)).

## Settled without work

URL reputation is out of the daily-driver contract
([#59](https://github.com/villekivela/omaweb/issues/59),
[ADR 0032](adr/0032-ship-without-url-reputation.md)). Omaweb ships no phishing, malware, or
download-reputation provider, documents the missing protection, and never presents Content blocking
as equivalent. Custom user agents and print preview are declined in `docs/product/requirements.md`
with the reasoning ([#323](https://github.com/villekivela/omaweb/issues/323)).
