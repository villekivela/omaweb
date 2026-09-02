# Roadmap

## Completed foundation

Tracking issues [#1](https://github.com/villekivela/tanto/issues/1) through [#6](https://github.com/villekivela/tanto/issues/6) established the Qt vertical slice and daily-driver foundation:

- Frameless macOS application shell and engine-free UI lab
- Spaces with isolated identities, suspension, vertical tabs, and Pinned tabs
- Omnibar, command panel, keyboard navigation, and link hints
- Transactional session persistence and renderer recovery
- History suggestions, Site permissions, downloads, Private windows, and Auxiliary windows
- Portable content blocking and runtime themes

## Docked Developer tools

Tracking issue: [#51](https://github.com/villekivela/tanto/issues/51)

- The engine capability, an Open developer tools command, and Inspect element
- Qt's bundled Chromium DevTools docked beside the inspected tab at a remembered width
- One inspector on one tab across navigation and Space switches
- The inspector drawn in the active theme, syntax colours included
- Remote debugging behind an explicit loopback-only development launch option
- A Tanto-drawn page context menu, which is where Inspect element belongs

## Everyday page commands

Tracking issue: [#52](https://github.com/villekivela/tanto/issues/52)

- Find belonging to one tab, retained while hidden and cleared by navigation
- Per-tab zoom on one ladder, kept in the session and restored with the tab
- Reload, Reload bypassing cache and Stop loading as three separate operations
- Browser fullscreen and site-requested fullscreen kept apart, the latter announced by origin and left with `Escape`
- Printing through the platform's own print dialog, including its PDF destination
- PDFs drawn in the engine's sandboxed viewer, downloaded and reported where there is none
- Every operation reporting an engine or desktop that cannot carry it out

## Complete the Qt daily driver

Tracking issue: [#50](https://github.com/villekivela/tanto/issues/50)

- Browser prompts with native file dialogs
- History sheet, configurable search engines, local-file opening, and browsing-data controls
- Tab reordering, duplication, bulk closing, retained closed-tab stacks, and Keep active for selected Pinned tabs
- Site information, certificate handling, third-party-cookie policy, safer downloads, and sandbox health
- Native notifications, external-protocol confirmation, default-browser registration, signed updates, and a signed notarized macOS package
- Research and implement engine-neutral phishing and malware URL reputation before public daily-driver status

## Linux and Wayland

Tracking issue: [#8](https://github.com/villekivela/tanto/issues/8)

- Validate the completed browser contract under native Wayland on Omarchy and Hyprland
- Ship an Arch `PKGBUILD` using system Qt packages
- Add compositor-specific blur with transparent and opaque fallbacks
- Run Linux accessibility, IME, packaging, sandbox, and default-browser tests

## Ladybird adapter

Tracking issue: [#7](https://github.com/villekivela/tanto/issues/7)

- Build against a pinned Ladybird revision outside the default graph
- Implement navigation, input, rendering, lifecycle, accessibility, and content-blocking contracts
- Report unavailable daily-browser capabilities rather than imitating Qt behavior
- Validate sandboxing and keep the build visibly experimental until it satisfies the daily-driver contract

## Deferred Feature modules

- Account
- Sync with replaceable providers
