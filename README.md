# `</>` Tanto

Tanto is a keyboard-driven web browser. It uses one ordinary window, vertical
tabs, and isolated browsing Spaces.

The Qt build runs on macOS today. The immediate goal is a dependable macOS
daily driver, followed by the same browser contract on Linux and Wayland.
Ladybird is the target engine, but it remains a separate experimental build
until its embedding and security contracts are ready.

## Developer tools

Developer tools belong to the tab being inspected. Tanto docks the inspector
provided by the current engine beside that tab, keeps it attached across
navigation and Space switches, and hides it when another tab is on show. The Qt
build uses Chromium DevTools and draws it in the active Tanto theme. Inspect
element is available from Tanto's page menu and from the keyboard.

Tanto does not add a diagnostics protocol, agent gateway, terminal, or source
editor. Local addresses resolve as addresses, reload can bypass cache, page
commands are keyboard-accessible, and the engine's real inspector is close at
hand.

## What works now

The current Qt build includes:

- A frameless macOS window with a resizable vertical sidebar
- Spaces with separate cookies, logins, history, permissions, sessions, and tabs
- Ordinary and Pinned tabs, Private windows, and site-requested Auxiliary windows
- A centered Omnibar for addresses, search, tabs, Spaces, and browser commands
- Keyboard navigation, configurable bindings, link hints, and a live shortcut sheet
- Docked Chromium DevTools, Inspect element, and loopback-only development debugging
- Find, per-tab zoom, normal and cache-bypassing reload, Stop loading, fullscreen,
  printing, and inline PDF viewing
- A Tanto-drawn page menu with link, image, media, selection, navigation, and
  inspection commands
- Session restoration, renderer recovery, downloads, searchable Space-local
  History, browsing-data controls, and Site permissions
- Built-in content blocking with EasyList, EasyPrivacy, cosmetic rules,
  scriptlets, and substitute resources
- Runtime themes, terminal-theme import, Private-window styling, and an
  engine-free UI lab

Tanto is still pre-alpha. Do not use it for sensitive browsing. It does not yet
have the complete certificate, download, sandbox-health, URL-reputation,
update, and signed-package work required for daily-driver status.

## Build and run

You need macOS 13 or newer with Xcode 15 or newer, or a current Linux
development environment. The build also requires Qt 6.11 at the repository's
approved patch level, CMake 3.30 or newer, Ninja, Clang with C++23 support, and
ccache. See [the development guide](docs/development.md) for the full setup.

Build the pinned content-blocking library once before the first configure:

```sh
scripts/bootstrap_content_blocker.sh
```

Then configure, build, and test Tanto:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Run the browser on macOS:

```sh
./build/dev/tanto.app/Contents/MacOS/tanto
```

On Linux, run `./build/dev/tanto`.

Development builds load QML, themes, and the icon font from the source tree.
Those edits need an application restart but no recompile. The `asan`, `release`,
`ci`, and `ladybird` presets cover their named builds. The Ladybird preset does
not enter the default Qt build graph.

### UI work without a web engine

The UI lab draws the same QML against fake browser state:

```sh
cmake --preset ui
cmake --build --preset ui
./build/ui/tanto-ui-lab.app/Contents/MacOS/tanto-ui-lab
```

Linux uses `./build/ui/tanto-ui-lab`. Pass `--private` to inspect the Private
palette or `--capture <path>` to render one frame and exit. Headless capture
works with `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`.

## Keyboard model

Every browser action is available through the command panel. `Primary` means
Command on macOS and Control on Linux. These are the main defaults:

| Keys | Action |
| --- | --- |
| `Primary+K` or `:` | Open the command panel |
| `Primary+L` or `o` | Open an address or search |
| `Primary+T` or `t` | Start a new tab |
| `Primary+W` or `x` | Close the current tab |
| `Primary+Shift+T` or `X` | Reopen the last closed tab |
| `Primary+Shift+I` | Toggle Developer tools |
| `Primary+Alt+C` | Inspect an element |
| `Primary+F` or `/` | Find in the page |
| `Primary+Y` | Open History for the active Space |
| `Primary+=`, `Primary+-`, `Primary+0` | Change or reset tab zoom |
| `Primary+Shift+R` or `R` | Reload without cache |
| `Primary+B` | Hide or show the sidebar |
| `Primary+E` | Focus the sidebar |
| `f`, `Shift+F` | Follow a link here or in a background tab |
| `j`, `k`, `gg`, `G` | Scroll the page |
| `Primary+/` or `?` | Show every current binding |

Single-key browser and page commands follow the Keyboard navigation setting.
Editable controls still receive typing, and sites can keep selected conflicting
keys. The complete default map is in
[`assets/keybindings/default.json`](assets/keybindings/default.json).

## Configuration

User-editable files live in `$XDG_CONFIG_HOME/tanto`, or `~/.config/tanto` when
that variable is unset.

- `keybindings.json` contains browser and page bindings plus per-site key
  passthrough rules. Tanto writes it on first launch.
- `theme.json` can replace the built-in palette, typography, semantic opacity,
  and Developer-tools syntax colours.
- `search-engines.json` contains the versioned local search-engine list, its
  optional keywords, and the default engine. Tanto creates it with DuckDuckGo
  as the default and keeps remote suggestions off.

`TANTO_CONFIG_ROOT` relocates the configuration directory.
`TANTO_KEYBINDINGS_FILE` and `TANTO_THEME_FILE` select individual files during
development.

Run `scripts/import_terminal_theme.py` from Ghostty, iTerm2, kitty, Alacritty,
or Terminal.app to derive `theme.json` from that terminal. A running Tanto
watches the file and repaints when it changes. Use `--print` to inspect the
result without writing it.

## Project layout

- `src/core` owns Spaces, tabs, sessions, commands, and persistence.
- `src/ui` contains the shared QML chrome.
- `src/ui-lab` runs that chrome without a web engine.
- `src/engine` defines the engine contract and contains the Qt adapter.
- `src/content-blocking` wraps the separately built Rust matcher.
- `src/platform` contains macOS and Linux window integration.
- `modules` is reserved for optional first-party Account and Sync modules.
- `docs/adr` records architectural decisions.
- `third_party` contains pinned dependencies and their manifests.

Start with [the architecture](docs/architecture.md),
[the product requirements](docs/product/requirements.md), and the
[domain glossary](CONTEXT.md) before changing a browser contract.

## Roadmap

The remaining Qt daily-driver work is tracked by
[#50](https://github.com/villekivela/tanto/issues/50). It covers browser prompts,
History and data controls, tab management, certificate and cookie policy,
download hardening, native notifications, signed updates, macOS packaging, and
phishing and malware protection.

After that, [#8](https://github.com/villekivela/tanto/issues/8) carries the
browser to Linux and Wayland. [#7](https://github.com/villekivela/tanto/issues/7)
tracks the Ladybird adapter. [The roadmap](docs/roadmap.md) keeps the ordered
breakdown.

Bookmarks, password management, third-party WebExtensions, Account, Sync,
installed web applications, Reader mode, translation, and DRM are outside the
current daily-driver contract. This is deliberate scope, not an implemented
feature list waiting for a checkbox.

## Privacy and security

Tanto has no telemetry, advertising identifier, browser account, cloud sync,
Google push service, or automatic crash upload. Every automatic request made by
the browser is listed in [the network request ledger](docs/network-requests.md).

Ordinary sessions open no debugging listener. The explicit
`--remote-debugging[=port]` development option binds to loopback, prints a
warning, and disables Private windows for that launch. Project scripts may not
disable the Chromium renderer sandbox.

The current build is still a preview. The missing daily-driver security work is
listed above and specified in
[the product requirements](docs/product/requirements.md#privacy-and-security).

## License

Tanto's own code is licensed under MPL 2.0. Engines, filter data, interface
components, and other third-party material retain their own licenses. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
