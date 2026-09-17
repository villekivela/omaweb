# Roadmap

Omaweb is alpha. The daily-driver contract in `CONTEXT.md` holds on Linux, and every release since
[v0.2.0](https://github.com/villekivela/omaweb/releases/tag/v0.2.0) carries the Arch package that
proves it. A version number records what a build is compatible with, not how finished it is
([ADR 0028](adr/0028-derive-the-version-from-the-release-tag.md)). This page states the stage and
the order of what is planned. A release's milestone lists what it contains, and its release notes
list what shipped.

## Planned

Releases are planned as [milestones](https://github.com/villekivela/omaweb/milestones) on the issue
tracker. The September 2026 feature review named what a daily driver still lacked. Three milestones
carry it, one theme each, in this order:

1. [v0.7.0](https://github.com/villekivela/omaweb/milestone/1), the page and what the reader takes
   from it: screenshots, the certificate in Site information, picture-in-picture, passkeys, an
   HTTPS-only mode, the open page bugs, and the decision on how payment cards are stored.
2. [v0.8.0](https://github.com/villekivela/omaweb/milestone/2), localization: the tooling, then
   every user-facing string wrapped, then the first locale finished and new strings gated. It
   follows v0.7.0 so the wrapping passes cover its features and nothing is in flight when the gate
   switches on.
3. [v0.9.0](https://github.com/villekivela/omaweb/milestone/3), forms and autofill: what was typed
   remembered, then addresses filled, then payment cards, on the storage decision v0.7.0 takes.

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
- Extensions wait on Qt's extension surface
  ([#175](https://github.com/villekivela/omaweb/issues/175),
  [#288](https://github.com/villekivela/omaweb/issues/288)). Whether Omaweb hosts password managers
  as Known extensions on a patched development engine is decided in
  [#344](https://github.com/villekivela/omaweb/issues/344).
- The Ladybird adapter ([#7](https://github.com/villekivela/omaweb/issues/7)) stays experimental and
  outside the default build graph until it satisfies the daily-driver contract.
- Account and Sync with replaceable providers are deferred.

## Settled without work

URL reputation is out of the daily-driver contract
([#59](https://github.com/villekivela/omaweb/issues/59),
[ADR 0032](adr/0032-ship-without-url-reputation.md)). Omaweb ships no phishing, malware, or
download-reputation provider, documents the missing protection, and never presents Content blocking
as equivalent. Custom user agents, print preview, and encrypted DNS are declined in
`docs/product/requirements.md` with the reasoning
([#323](https://github.com/villekivela/omaweb/issues/323)).
