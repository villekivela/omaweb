# `</>` Omaweb

Omaweb is a keyboard-driven web browser for developers. One window, tabs down the side, Spaces that
keep logins apart, and a palette that follows the desktop's own theme.

Linux with first-class Wayland support is the distribution platform, and the only one CI builds. The
Qt build also runs on macOS as a development and test platform; those bundles are never distributed.
QtWebEngine is the development engine. Ladybird is the target engine and stays a separate
experimental build until its embedding and security contracts are ready.

Omaweb is pre-alpha. Do not put anything sensitive through it.

## Status

The browser contract is complete on the Qt build: Spaces, tabs, the Omnibar, Keyboard navigation,
docked Developer tools, page commands, History and Settings, Site information, Content blocking, and
download hardening all work.

Three things are not finished:

- **Linux platform integration.** The browser runs frameless on Wayland today, but the native blur
  backdrop, printing, and native notifications are macOS-only and land with the Wayland port
  ([#8](https://github.com/villekivela/omaweb/issues/8)). On Linux those commands are listed and
  report themselves unavailable rather than pretending.
- **Linux packaging.** There is no Arch package, no default-browser registration, and no update
  delivery yet. Tracked by [#8](https://github.com/villekivela/omaweb/issues/8) as well.
- **The Ladybird adapter.** Tracked by [#7](https://github.com/villekivela/omaweb/issues/7).

[The roadmap](docs/roadmap.md) has the ordered breakdown.

## What the browser does

- Spaces with separate cookies, logins, history, permissions, sessions, and tabs
- Ordinary and Pinned tabs, Keep active for a Pinned tab, Private windows, and site-requested
  Auxiliary windows
- A centered Omnibar for addresses, search, tabs, Spaces, and browser commands
- Keyboard navigation, configurable bindings, link hints, and a Start page that reads its shortcut
  sheet from the running keymap
- Docked Chromium DevTools, Inspect element, and loopback-only remote debugging behind an explicit
  launch option
- Find, per-tab zoom, normal and cache-bypassing reload, Stop loading, fullscreen, printing, and
  inline PDF viewing
- An Omaweb-drawn page context menu, tab-modal JavaScript and HTTP-authentication prompts, and
  external-protocol confirmation
- Tab reordering, duplication, bulk closing, and a per-Space stack of the 25 most recently closed
  tabs
- Searchable Space-local History, browsing-data controls, Site permissions, and a site-information
  panel carrying connection state, certificate errors, and the third-party-cookie allowance
- Downloads that ask before writing a program down, mark what they saved with where it came from,
  strip its execute bits, and never open it, with what is still running shown in the sidebar footer
- Content blocking with EasyList, EasyPrivacy, cosmetic rules, Scriptlets, and Substitute resources
- Runtime themes, terminal-theme import, Omarchy theme following, Private-window styling, and an
  engine-free UI lab

Everything the browser can do is reachable from the command panel. An action that is not is a
defect.

## Developer tools

Developer tools belong to the tab being inspected. Omaweb docks the inspector the current engine
provides beside that tab, keeps it attached across navigation and Space switches, and hides it while
another tab is on show. The Qt build docks Chromium DevTools and draws it in the active theme,
syntax colours included. Inspect element is in the page context menu and on `Primary+Alt+C`.

Omaweb adds no diagnostics protocol, agent gateway, terminal, or source editor
([ADR 0027](docs/adr/0027-use-engine-provided-developer-tools.md)). Local addresses resolve as
addresses rather than searches, reload can bypass cache, page commands are keyboard-accessible, and
the engine's real inspector is a keystroke away.

## Build and run

You need Qt 6.11 at or above the repository's approved patch level, CMake 3.30 or newer, Ninja,
Clang with C++23 support, and ccache. Linux needs a current development environment; macOS needs 13
or newer with Xcode 15 or newer. See [the development guide](docs/development.md) for the full
setup.

Build the pinned content-blocking library once before the first configure:

```sh
scripts/bootstrap_content_blocker.sh
```

Then configure, build, and test:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Run the browser with `./build/dev/omaweb`. On macOS the binary is inside the bundle, at
`./build/dev/omaweb.app/Contents/MacOS/omaweb`.

Development builds load QML, themes, and the icon font from the source tree, so editing those needs
an application restart but no compile. The `asan`, `release`, `ci`, and `ladybird` presets cover
their named builds. The Ladybird preset stays outside the default Qt build graph.

### UI work without a web engine

The UI lab draws the same QML against fake browser state, so QML changes do not cost a web-engine
build:

```sh
cmake --preset ui
cmake --build --preset ui
./build/ui/omaweb-ui-lab
```

macOS again puts the binary in a bundle. `--private` paints the window in the Private palette,
`--show <state>` opens a state a capture cannot press a key to reach, and `--capture <path>` renders
one frame and exits. Headless capture works under
`QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`, which is the easiest way to put a chrome
change in a pull request.

## Keyboard model

`Primary` is Control on Linux and Command on macOS. The defaults:

| Keys                                  | Action                                    |
| ------------------------------------- | ----------------------------------------- |
| `Primary+K` or `:`                    | Open the command panel                    |
| `Primary+L` or `o`                    | Open an address or search                 |
| `Primary+T` or `t`                    | Start a new tab                           |
| `Primary+W` or `x`                    | Close the current tab                     |
| `Primary+Shift+T` or `X`              | Reopen the last closed tab                |
| `Primary+Shift+I`                     | Toggle Developer tools                    |
| `Primary+Alt+C`                       | Inspect an element                        |
| `Primary+F` or `/`                    | Find in the page                          |
| `Primary+Y`                           | Open History for the active Space         |
| `Primary+=`, `Primary+-`, `Primary+0` | Change or reset tab zoom                  |
| `Primary+Shift+R` or `R`              | Reload without cache                      |
| `Primary+B`                           | Hide or show the sidebar                  |
| `Primary+E`                           | Focus the sidebar                         |
| `f`, `Shift+F`                        | Follow a link here or in a background tab |
| `j`, `k`, `d`, `u`, `gg`, `G`         | Scroll the page                           |
| `Primary+/` or `?`                    | Show every current binding                |

Single-key browser and page commands follow the Keyboard navigation setting. Editable controls still
receive typing, and a site can keep selected conflicting keys. The complete default map is
[`assets/keybindings/default.json`](assets/keybindings/default.json).

## Configuration

User-editable files live in `$XDG_CONFIG_HOME/omaweb`, or `~/.config/omaweb` when that variable is
unset.

- `keybindings.json` holds browser bindings, page bindings, and per-site key passthrough rules.
  Omaweb writes it on first launch.
- `theme.json` replaces the built-in palette: colours, typography, semantic opacity, and
  Developer-tools syntax colours.
- `search-engines.json` holds the versioned local search-engine list, its optional keywords, and the
  default engine. Omaweb creates it with DuckDuckGo as the default and keeps remote suggestions off.

`OMAWEB_CONFIG_ROOT` relocates the whole directory. `OMAWEB_KEYBINDINGS_FILE` and
`OMAWEB_THEME_FILE` point at individual files during development.

On Linux, following the desktop's theme needs no setup. The first start on a machine with Omarchy
installs the template Omaweb ships to `~/.config/omarchy/themed/` and asks Omarchy to render the
active theme through it, so `omarchy theme set` repaints the browser along with the bar and the
terminal. A template already there is kept, and `OMAWEB_NO_OMARCHY_TEMPLATE` stops Omaweb writing
into that directory at all. See [`integrations/omarchy/README.md`](integrations/omarchy/README.md).

`scripts/import_terminal_theme.py` derives `theme.json` from the terminal it runs in. It reads
Ghostty, iTerm2, kitty, Alacritty, and Terminal.app. A running Omaweb watches the file and repaints
without a restart. `--print` shows the result instead of writing it.

## Project layout

- `src/core` owns Spaces, tabs, sessions, commands, and persistence.
- `src/ui` holds the shared QML chrome.
- `src/ui-lab` runs that chrome without a web engine.
- `src/engine` defines the engine contract and holds the Qt adapter.
- `src/content-blocking` wraps the separately built Rust matcher.
- `src/platform` holds macOS and Linux window-system integration.
- `modules` is reserved for the optional Account and Sync Feature modules.
- `third_party` holds pinned dependencies and their manifests. Both vendored trees are byte-for-byte
  copies and `ctest` fails on any local edit.
- `docs/adr` records architecture decisions.

Read [the architecture](docs/architecture.md),
[the product requirements](docs/product/requirements.md), and the [domain glossary](CONTEXT.md)
before changing a browser contract. The glossary is the one to read first: this project is strict
about its nouns, and a review will be too. [CONTRIBUTING.md](CONTRIBUTING.md) covers the rest of the
workflow.

## Deliberate non-goals

Bookmarks, password management, third-party WebExtensions, Account, Sync, installed web
applications, Reader mode, translation, browser data import, View source, spellchecking, DRM, and
additional ordinary browser windows are outside the daily-driver contract. So is macOS distribution.
These are settled scope decisions rather than an unimplemented feature list, and a pull request
adding one will be declined on scope however good the code is.

URL reputation is a non-goal too, and the one worth stating plainly: Omaweb ships no phishing,
malware, or download-reputation provider ([ADR 0032](docs/adr/0032-ship-without-url-reputation.md)).
Content blocking refuses some known malicious addresses. That is not the same protection, and Omaweb
does not present it as such.

## Privacy and security

Omaweb has no telemetry, advertising identifier, browser account, cloud sync, Google push service,
or automatic crash upload. Every automatic request the browser makes is listed in
[the network request ledger](docs/network-requests.md).

Every page runs in its own sandboxed renderer. Omaweb refuses to start when the sandbox is
explicitly disabled through its own command line or through `QTWEBENGINE_CHROMIUM_FLAGS`, and
refuses to start on a Linux host whose kernel cannot meet the sandbox's prerequisites. There is no
flag that turns that check off. QtWebEngine runs the network service inside the browser process
rather than in a sandboxed process of its own, and Omaweb does not describe it as isolated.

An ordinary session opens no listening socket. The `--remote-debugging[=port]` development option
binds to loopback, prints a warning, and disables Private windows for that launch.

`security/baseline.json` names the approved QtWebEngine and the Chromium release whose security
fixes it carries. The build reads it, so Settings reports whether the running engine meets the
baseline, and CI compares it against upstream weekly. A build below the baseline says it is an
unsupported preview.

[SECURITY.md](SECURITY.md) has the vulnerability reporting form and the response commitment.
[The product requirements](docs/product/requirements.md#privacy-and-security) specify the full
contract.

## License

Omaweb's own code is under MPL 2.0. Engines, filter data, interface components, and other
third-party material keep their own licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
