# Roadmap

The Qt browser contract is complete. What remains before Omaweb is a daily driver is Linux: the
platform integration the Wayland port carries, and a package to install it with.

[#50](https://github.com/villekivela/omaweb/issues/50) and every issue under it are closed.
[#8](https://github.com/villekivela/omaweb/issues/8) is the only gate left before daily-driver
status.

Three parts of the contract are satisfied on macOS alone, and the Wayland port is what supplies them
on Linux: the frameless window chrome and its blur, printing, and native notifications. Until then
the Linux platform layer reports those capabilities off and the commands say so.

## Remaining

### Linux and Wayland

Tracking issue: [#8](https://github.com/villekivela/omaweb/issues/8)

- Validate the completed browser contract under native Wayland on Omarchy and Hyprland
- Supply the Linux window chrome, printing, and notifications the platform layer still reports
  unavailable
- Add compositor-specific blur with transparent and opaque fallbacks
- Ship an Arch `PKGBUILD` using system Qt packages
- Add Linux default-browser integration and release delivery through the native package
- Run Linux accessibility, IME, packaging, sandbox, and default-browser tests

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
