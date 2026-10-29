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
release mark no longer hidden under the extension mark.
[v0.13.0](https://github.com/villekivela/omaweb/releases/tag/v0.13.0) shipped configuration as
files: the reader's settings in one `settings.json` in the configuration directory, the app icon
chosen in Settings, six Space colours that Omaweb owns rather than the theme, and the sidebar's
opacity on a slider. [v0.13.1](https://github.com/villekivela/omaweb/releases/tag/v0.13.1) shipped
packages built against the Qt that Omarchy's stable mirror serves, so Omaweb starts on Omarchy
again. [v0.14.0](https://github.com/villekivela/omaweb/releases/tag/v0.14.0) shipped Tab windows: a
tab popped out of the main window into a window of its own, so a page of a Space can sit on another
monitor ([ADR 0062](adr/0062-pop-a-tab-out-instead-of-opening-windows.md)). It also shipped
Bitwarden's passkeys, the Hyprland opacity override no longer asked for now that Omarchy draws
windows opaque, and pinned tabs kept on their column grid.
[v0.15.0](https://github.com/villekivela/omaweb/releases/tag/v0.15.0) shipped a Glance that moves
into its tab when kept, a favicon that follows the colour scheme drawn again when the theme changes,
a page an extension opened told to the engine as a reader's tab, no dark frame when Omaweb's
workspace comes back, and `omaweb help`. Its engine is 6.11.2-14.
[v0.15.1](https://github.com/villekivela/omaweb/releases/tag/v0.15.1) shipped fixes: a page's SVG
favicon shown over a raster one beside it, so chatgpt.com's follows a dark theme, white beside a
page that leaves its sides bare, the sidebar's key labels drawn over the address row, a Private
window that answers web push as a Space does, and a page told what its passkeys can reach. Its
engine is 6.11.2-15. [v0.16.0](https://github.com/villekivela/omaweb/releases/tag/v0.16.0) shipped
the browser theme, which follows Omarchy's or is any installed Omarchy theme picked for the browser
alone ([ADR 0066](adr/0066-pick-the-browser-theme-from-installed-omarchy-themes.md)), a playing tab
kept audible through its silences, and the Omarchy kit caught up with quattro. Its engine is still
6.11.2-15. [v0.17.0](https://github.com/villekivela/omaweb/releases/tag/v0.17.0) shipped a
two-finger swipe across the sidebar that switches Space, pages drawn at a scale that follows the
desktop's text scaling, Problem reports, a click that copies without asking, Google's sign-in
answered with Chrome's own `chrome.app`, and open pages kept when Sync brings another machine's
state. Its engine is 6.11.2-16, built against Qt 6.12.0.
[v0.18.0](https://github.com/villekivela/omaweb/releases/tag/v0.18.0) shipped a quieter sidebar,
with the address as a bare domain line, borderless pinned tabs and an Agent tab marked on its row,
the inspector drawn in the theme, a CRT glass and a telemetry Scene on the Start page, touchpad
scrolling that glides on after the lift, and link hints that follow Vimium's rules. Its engine is
still 6.11.2-16. [v0.18.1](https://github.com/villekivela/omaweb/releases/tag/v0.18.1) shipped the
engine on QtWebEngine 6.11.3, with Chromium's security fixes up to 155.0.8059.85, Omarchy's private
browser key opening a Private window, and the Sync avatar drawn as a circle.
[v0.19.0](https://github.com/villekivela/omaweb/releases/tag/v0.19.0) shipped Link rules, which open
a link from another app in the Space they name or in a Private window, chosen from Site information
too, `omaweb --diagnose` for a run with named overrides, the sidebar footer as the Space row with
short-lived marks in a pill, no floating navigation strip, and a Glance head with its own back,
forward and reload.

[v0.20.0](https://github.com/villekivela/omaweb/milestone/21) is the release in progress, for polish
and performance. Sync stops reloading an open page with this machine's own state
([#861](https://github.com/villekivela/omaweb/issues/861)). The Glance comes inside its frame budget
([#858](https://github.com/villekivela/omaweb/issues/858)), after its buttons read over a light page
and its key to become a tab works ([#863](https://github.com/villekivela/omaweb/issues/863)). The
runtime budget is taken again on the 6.11.3 engine
([#859](https://github.com/villekivela/omaweb/issues/859)), and so is the comparison with Chromium
([#860](https://github.com/villekivela/omaweb/issues/860)). The x86_64 engine links at ThinLTO's
`--lto-O2`, as Chrome does, which takes Speedometer from 0.73 of Chromium 140 to 0.82
([#886](https://github.com/villekivela/omaweb/issues/886)). The address row gets quieter: the
extension mark is muted and the search button goes
([#862](https://github.com/villekivela/omaweb/issues/862)), and then the Space buttons get a quieter
hover and the keep-active dot goes ([#869](https://github.com/villekivela/omaweb/issues/869)).
Settings glides after a touchpad lift and moves between sections with the arrow keys
([#864](https://github.com/villekivela/omaweb/issues/864)), its toggles stand apart
([#866](https://github.com/villekivela/omaweb/issues/866)), downloads show progress
([#865](https://github.com/villekivela/omaweb/issues/865)), and About says Omaweb is the default
when Omarchy's browser router opens it ([#867](https://github.com/villekivela/omaweb/issues/867)).
Files are chosen through the desktop's file chooser portal
([#868](https://github.com/villekivela/omaweb/issues/868)). The drifts the new budget showed are
named. Startup's 0.2 s is mostly the runner, and a Start page closed before the first frame no
longer fades over it ([#883](https://github.com/villekivela/omaweb/issues/883)). The 21 MiB is the
chrome added since September and is kept ([#884](https://github.com/villekivela/omaweb/issues/884)).
A frozen away page that keeps growing is traced
([#885](https://github.com/villekivela/omaweb/issues/885)). The budget is measured on the release
build readers run ([#889](https://github.com/villekivela/omaweb/issues/889)), and the engine closes
its Speedometer gap to Chromium through its build flags
([#886](https://github.com/villekivela/omaweb/issues/886)).

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

- The component kit ([#9](https://github.com/villekivela/omaweb/issues/9),
  [#13](https://github.com/villekivela/omaweb/issues/13)) waits on Omarchy.
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
with the reasoning ([#323](https://github.com/villekivela/omaweb/issues/323)). Omaweb doesn't run
Omarchy's web apps with `--app`: they run well in Chromium, which `omarchy launch webapp` already
falls back to ([#655](https://github.com/villekivela/omaweb/issues/655)).
