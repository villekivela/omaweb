# Tanto

Tanto is a keyboard-driven browser being built for developers. Its Qt build is
becoming a macOS daily driver first, then the same browser contract moves to
Linux with first-class Wayland support.

The window has no title bar. Tabs run down a sidebar, addresses and commands
open in a centered Omnibar, and every action is reachable from the keyboard.
Browsing is split into Spaces, each with its own logins, cookies, history,
permissions, and tabs. Content blocking is built in rather than an extension.
There is no telemetry, no account, and no sync.

The developer feature is deliberately small: Tanto docks the inspector supplied
by the current engine beside the tab it inspects. Qt provides Chromium DevTools;
an engine without an inspector reports the command unavailable. Local addresses,
one-time local certificate exceptions, cache-bypassing reload, and every browser
action remain reachable from the keyboard. Tanto does not build an agent,
diagnostics protocol, terminal, or source editor into the browser.

QtWebEngine is the engine Tanto is built against today. Ladybird is the engine
it is built *for*, and it stays in a separate pinned build until its embedding
and security contracts hold up for daily use.

## Status

Pre-alpha, and not a browser to keep your banking session in yet.

The vertical slice runs: frameless window, Spaces with vertical and Pinned tabs,
the Omnibar and command panel, keyboard navigation with link hints, content
blocking, session persistence, live themes, an engine-free UI lab, and Chromium
DevTools docked beside the tab they inspect, drawn in the browser's own theme.
The next milestone fills the ordinary browser gaps such as find, zoom, printing,
context menus, History, security controls, and release updates. [docs/roadmap.md](docs/roadmap.md) tracks the work.

## Build

Prerequisites are Qt 6.11, CMake 3.30, Ninja, Clang with C++23, and ccache.
[docs/development.md](docs/development.md) has the versions and the macOS Qt
notes.

Build the pinned content blocker once before the first configure. CMake checks
its checksum but never runs Cargo:

```sh
scripts/bootstrap_content_blocker.sh
```

Then:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The binary is `./build/dev/tanto.app/Contents/MacOS/tanto` on macOS and
`./build/dev/tanto` on Linux.

QML, themes, and the icon font load from the source tree in development
builds, so editing them costs a restart and no compile. For interface work
without QtWebEngine, use the `ui` preset and run `tanto-ui-lab`, which draws
the same QML against fake browser state. `--capture <path>` renders one frame
to a PNG, which works headlessly. The other presets are `asan`, `release`,
`ci`, and `ladybird`; the Ladybird one is deliberately outside the default
build graph.

## Configuration

Files live in `$XDG_CONFIG_HOME/tanto`, or `~/.config/tanto` when that is
unset:

- `keybindings.json` holds both maps, `bindings` for page commands and
  `browser` for commands Tanto runs itself. Tanto writes the defaults there on
  first launch. Rebinding is editing that file; sharing a keymap is copying it.
- `theme.json` is optional. Without one, Tanto follows the desktop theme
  (Omarchy on Linux) and then its built-in palette. `TANTO_THEME_FILE`
  overrides both. Its `syntax` section names the colours code is read in, and
  Developer tools are drawn in them.

`scripts/import_terminal_theme.py` writes `theme.json` from the colours of the
terminal you run it in. It reads Ghostty, iTerm2, kitty, Alacritty, and
Terminal.app. A running Tanto is watching the file, so the window repaints
without a restart.

## Repository

- `src/core` owns Spaces, tabs, sessions, commands, and persistence.
- `src/ui` is the shared QML chrome; `src/ui-lab` runs it without an engine.
- `src/engine` holds the engine contract and the Qt adapter behind it.
- `src/content-blocking` wraps the Rust matcher; `src/platform` holds the
  macOS and Linux window integrations.
- `third_party/omarchy-shell` is a vendored, pinned copy of the Omarchy
  shell's QML kit. Those files are never edited locally; `ctest` fails if they
  are.
- `docs/adr` records the decisions behind all of this. Start with
  [architecture.md](docs/architecture.md) and
  [product/requirements.md](docs/product/requirements.md).

## Privacy

Every network request Tanto makes on its own is listed in
[docs/network-requests.md](docs/network-requests.md). Engine sandboxes stay
enabled in every build, and no script may pass a flag that disables them.

## License

Tanto's own code is under MPL 2.0. Third-party engines and data keep their own
licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
