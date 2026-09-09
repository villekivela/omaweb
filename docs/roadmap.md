# Roadmap

The Qt browser contract is complete. What remains before Omaweb is a daily driver is Linux: the
platform integration the Wayland port carries, and a package to install it with.

[#50](https://github.com/villekivela/omaweb/issues/50),
[#8](https://github.com/villekivela/omaweb/issues/8), and every issue under them are closed.
Validation on real Linux hardware ([#103](https://github.com/villekivela/omaweb/issues/103)) has
run, and [v0.2.0](https://github.com/villekivela/omaweb/releases/tag/v0.2.0) carries the package
that proves it. Omaweb is alpha.

The frameless window is Qt window flags rather than platform code, so it already works on Wayland,
and blur behind the browser's transparent surfaces is the compositor's own. Notifications and
printing now go through the desktop's own session-bus services, so `omaweb-platform` has nothing
left that is macOS-only. The package ships, and what remains for Linux is what the release turns up.

Blur is not a gate. On Linux the shader pass belongs to the compositor and Omaweb's part is asking
for a transparent surface, which it does. There is nothing here for a validation run to judge.

## Alpha, and what answered it

Alpha is the daily-driver contract holding on Linux, with a release that carries the package proving
it. `CONTEXT.md` states the contract, and
[v0.2.0](https://github.com/villekivela/omaweb/releases/tag/v0.2.0) is the release.
[ADR 0028](adr/0028-derive-the-version-from-the-release-tag.md) ships every `v0.*` tag as a
prerelease while the major version is 0, which records what a build is compatible with rather than
how finished it is.

1. Run the Wayland validation sweep on real hardware, covering input, clipboard, IME, accessibility,
   and window movement ([#103](https://github.com/villekivela/omaweb/issues/103)). Answered by
   `scripts/check_wayland_session.py`, which is a script rather than the checklist this started as:
   Hyprland's own dispatchers supply the keystrokes and the cursor, and `ydotool` the one thing they
   do not, a pointer button. Becoming the default browser is the one bullet left, because it changes
   the machine that runs it; `scripts/check_default_browser.py` drives it where that is allowed.
2. Confirm a theme change repaints without a restart, driven by `omarchy theme set` rather than by
   inspecting the rendered files. Answered by `scripts/check_theme_repaint.py`, which reads the
   sidebar's pixels against the colour the theme declares and confirms the process did not restart.
3. Confirm the Arch package installs, upgrades, and removes without touching unrelated user files,
   and that default-browser registration still requires an explicit action. Answered by
   `scripts/check_package.sh`, which Linux CI runs on every change in a container of its own.
4. Qualify the build against the approved engine baseline, which means building on the packaged
   QtWebEngine and running `ctest --preset ci` in full. `SECURITY.md` states the process.
5. Publish a release carrying the `.pkg.tar.zst`, the licenses, `THIRD_PARTY_NOTICES.md`, and the
   generated SBOM. The `v0.1.x` releases carry notes alone.

The three checks that drive a live desktop are not one command, and deliberately. The sweep changes
nothing a reader would notice and reads no pixels. The other two change the machine they run on, the
theme in force or the default browser, so each is its own script, each refuses to run without being
told to, and each puts back what it changed.

Nothing else gates alpha. The Omarchy window rule
([#75](https://github.com/villekivela/omaweb/issues/75)) and the component kit
([#9](https://github.com/villekivela/omaweb/issues/9),
[#13](https://github.com/villekivela/omaweb/issues/13)) wait on upstream. The Ladybird adapter
([#7](https://github.com/villekivela/omaweb/issues/7)) stays experimental, and Account and Sync are
deferred.

## Remaining

### Linux and Wayland

Tracking issue: [#103](https://github.com/villekivela/omaweb/issues/103)

- Drive the default browser on a machine whose default browser may change, which is the one check in
  [#103](https://github.com/villekivela/omaweb/issues/103) still to be run
- Compose through an input method. With `fcitx5-qt` installed Qt loads the plugin and reaches fcitx5
  over D-Bus, never binding `zwp_text_input_v3`, which is the plugin's design rather than a fault.
  Composing itself needs an input method that composes, and a plain keyboard layout is not one. The
  renderer sandbox and accessibility are already verified under Hyprland.

Linux is the only platform CI builds. macOS remains a development and test platform, and Omaweb does
not distribute its bundles ([ADR 0029](adr/0029-distribute-only-for-linux.md)).

### Ladybird adapter

Tracking issue: [#7](https://github.com/villekivela/omaweb/issues/7)

- Build against a pinned Ladybird revision outside the default graph
- Implement navigation, input, rendering, lifecycle, accessibility, and content-blocking contracts
- Report unavailable daily-browser capabilities rather than imitating Qt behavior
- Validate sandboxing and keep the build visibly experimental until it satisfies the daily-driver
  contract

### Deferred Feature modules

- Account
- Sync with replaceable providers

## Settled without work

URL reputation is out of the daily-driver contract
([#59](https://github.com/villekivela/omaweb/issues/59),
[ADR 0032](adr/0032-ship-without-url-reputation.md)). Omaweb ships no phishing, malware, or
download-reputation provider, documents the missing protection, and never presents Content blocking
as equivalent.

## Delivered

### Foundation

Tracking issues [#1](https://github.com/villekivela/omaweb/issues/1) through
[#6](https://github.com/villekivela/omaweb/issues/6) established the Qt vertical slice and
daily-driver foundation:

- Frameless macOS application shell and engine-free UI lab
- Spaces with isolated identities, suspension, vertical tabs, and Pinned tabs
- Omnibar, command panel, Keyboard navigation, and link hints
- Transactional session persistence and renderer recovery
- History suggestions, Site permissions, downloads, Private windows, and Auxiliary windows
- Portable Content blocking and runtime themes

### Docked Developer tools

Tracking issue: [#51](https://github.com/villekivela/omaweb/issues/51)

- The engine capability, an Open developer tools command, and Inspect element
- Qt's bundled Chromium DevTools docked beside the inspected tab at a remembered width
- One inspector on one tab across navigation and Space switches
- The inspector drawn in the active theme, syntax colours included
- Remote debugging behind an explicit loopback-only development launch option
- An Omaweb-drawn page context menu, which is where Inspect element belongs

### Everyday page commands

Tracking issue: [#52](https://github.com/villekivela/omaweb/issues/52)

- Find belonging to one tab, retained while hidden and cleared by navigation
- Per-tab zoom on one ladder, kept in the session and restored with the tab
- Reload, Reload bypassing cache and Stop loading as three separate operations
- Browser fullscreen and site-requested fullscreen kept apart, the latter announced by origin and
  left with `Escape`
- Printing through the platform's own print dialog, including its PDF destination, on macOS
- PDFs drawn in the engine's sandboxed viewer, downloaded and reported where there is none
- Every operation reporting an engine or desktop that cannot carry it out

### The rest of the Qt browser contract

Tracking issues: [#50](https://github.com/villekivela/omaweb/issues/50), and
[#53](https://github.com/villekivela/omaweb/issues/53) to
[#56](https://github.com/villekivela/omaweb/issues/56)

- Omaweb-owned page context menus and browser prompts, with native file dialogs
- History sheet, configurable search engines, local-file opening, and browsing-data controls
- Tab reordering, duplication, bulk closing, retained closed-tab stacks, and Keep active for
  selected Pinned tabs
- Site information, certificate handling, and third-party-cookie policy
- External-protocol confirmation, and native notifications on macOS

### Downloads and runtime security

Tracking issue: [#57](https://github.com/villekivela/omaweb/issues/57)

- A pure seam naming what a file is, executable, script, installer, disk image or archive, from the
  name that will land on disk
- One rule in the core turning that, the reader's dealings with the origin, and the directory into a
  disposition the adapters read by name
- A question that writes nothing: the request is cancelled before a byte lands and the page is asked
  for the same file again once the reader answers
- A Site permission for a page that downloads without being touched, or starts a second download
  while its first runs
- Finished downloads marked with where they came from and left without their execute bits, never
  opened
- The download directory as the reader's configuration rather than one Space's browsing data
- Downloads listed with their progress and whatever is still open to them, and the ones still
  running marked in the outline footer
- Sandbox-disabling switches refused through `QTWEBENGINE_CHROMIUM_FLAGS` as well as `argv`, and a
  Linux host that cannot isolate a renderer named and stopped
- An approved engine baseline the build reports against and CI reads weekly
