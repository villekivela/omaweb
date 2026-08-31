# Development

## Requirements

- macOS 13 or newer with Xcode 15 or newer, or a current Linux development environment
- Qt 6.11 at or above the repository's approved patch, including Qt Quick, Qt SQL, and Qt WebEngine
- CMake 3.30 or newer
- Ninja
- Clang with C++23 support
- ccache

The canonical macOS Qt SDK comes from Qt's official Apple Silicon distribution. Homebrew Qt is a convenience option only when it meets the approved security patch level.

## Presets

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Use the engine-free UI runner for QML work:

```sh
cmake --preset ui
cmake --build --preset ui
./build/ui/tanto-ui-lab.app/Contents/MacOS/tanto-ui-lab
```

On Linux, the executable is `./build/ui/tanto-ui-lab`. Pass `--capture <path>` to
render one frame to a PNG and exit, which works headlessly with
`QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` for reviewing chrome changes
without a desktop session. Development presets load QML, themes, and the icon font directly from the source tree. Editing those files requires an application restart but no compile or relink.

The `ladybird` preset is deliberately separate. Do not add Ladybird, Qt source builds, or Rust compilation to `dev`.

Before the first configure, build the pinned Content-blocking library into the disposable local
cache:

```sh
scripts/bootstrap_content_blocker.sh
```

The script uses `third_party/content-blocker/Cargo.lock`, records the shared library checksum, and
pins `adblock` 0.12.5 to Ladybird revision
`e5a41dfb6930fe5471c2c203d2dc32a1a782e816`. CMake verifies the cached artifact but never invokes
Cargo. Run the bootstrap again only after changing the Rust wrapper, its manifest, or its lockfile.

## Security rules

Never use Chromium's `--no-sandbox`, `--single-process`, in-process network-service flags, or `QTWEBENGINE_DISABLE_SANDBOX` in project scripts. Fix the host configuration or packaging instead.

## Theme

Tanto reads `theme.json` from the configuration directory when one is present, falling back to the desktop-managed theme (Omarchy, on Linux) and then the built-in palette. `TANTO_THEME_FILE` overrides all three.

`scripts/import_terminal_theme.py` derives that file from the terminal it runs in. Run it from the terminal whose colours you want — `TERM_PROGRAM` identifies that terminal exactly — and it writes `theme.json` into the configuration directory, which `ThemeController` is watching, so a running Tanto repaints without a restart. It reads Ghostty (via `ghostty +show-config`, which resolves a named theme into concrete colours), iTerm2, kitty, Alacritty, and a customised Terminal.app profile. `--print` shows the result instead of writing it, `--terminal` overrides detection, and `--force` replaces an existing theme. On a Linux desktop that already renders Tanto's template, the script refuses to write and says so: a theme in the configuration directory outranks the desktop-rendered one, so importing would freeze the palette at whatever the terminal looked like that day.

The derivation takes the terminal's background, foreground and sixteen ANSI colours and builds the chrome ladder by mixing in OKLab, with the step sizes measured off the default theme. The accent is the most saturated of the blue, magenta and cyan slots that clears 4.5:1 against the background; red, green and yellow are left alone because they already mean error, success and warning. The type base size, tint and the semantic opacities stay as the default theme sets them — a terminal's `background-opacity` is a window-wide setting and does not translate to Tanto's per-surface opacity.

`theme.json`'s `font` block is `families` and `size`. `families` is a preference order and the first family the host actually has installed wins, so a theme can name a font it would like without breaking a machine that lacks it; nothing is ever handed to Qt that Qt cannot find, because a missing family costs a font-alias sweep at startup and then draws in whatever face Qt substitutes. `size` is the root the whole type scale grows from — every size the interface asks for is derived from it by the Omarchy kit, which `ThemeController` drives. See [ADR 0018](adr/0018-drive-the-kit-from-the-theme-palette.md).

## Interface components

Shared controls come from the Omarchy shell's QML kit, vendored under
`third_party/omarchy-shell` and pinned by `MANIFEST.json`. Both projects are QML on
Qt 6, so the kit is used as-is rather than reimplemented.

Vendored files are byte-for-byte copies and are never edited — `ctest` fails on any
local change. Tanto meets the kit from three sides: `src/ui/quickshell-shim` registers
the `Quickshell` and `Quickshell.Io` types the kit's singletons import, the QML
in `src/ui` adapts components to Tanto's call sites, keeping Tanto's property names
and its accessibility annotations, and `src/ui/KitTheme.cpp` drives the kit's
`qs.Commons` colour and type singletons from the theme palette so they follow
`ThemeController` rather than an Omarchy theme on disk. The vendor root is on the QML
import path in source builds and lands under `qrc:/qt/qml` in resource builds.

```sh
scripts/sync_omarchy_ui.py --verify          # the local tree matches the manifest
scripts/sync_omarchy_ui.py --check-upstream  # what changed on quattro since the pin
scripts/sync_omarchy_ui.py --sync --ref <sha>
```

Upstream's branch moves, so a sync is deliberate: move the pin, read the diff, and
run `ctest`. `tanto-ui` instantiates the adapted components and asserts the kit's
tokens resolve, so an upstream API change fails there.

Nothing moves the pin on its own, so the `Omarchy kit drift` workflow does the
looking: every Monday it runs `--check-upstream --report drift.json` and hands the
report to `scripts/report_omarchy_drift.py`, which keeps one `omarchy-drift` issue
listing the added, removed, and changed files with a compare link, and closes it once
the pin catches up. It never syncs — an upstream API change lands on Tanto's adapters,
so the diff wants a reader. `ctest -R tanto-omarchy-drift` covers both halves with
GitHub stubbed out.

## Keyboard navigation configuration

Tanto copies `assets/keybindings/default.json` to `keybindings.json` in the configuration directory on first launch — `$XDG_CONFIG_HOME/tanto`, or `~/.config/tanto` when that is unset. A file left by an earlier version under the application data directory is moved there. Set `TANTO_CONFIG_ROOT` to relocate the whole directory, or `TANTO_KEYBINDINGS_FILE` to load one specific file during development. The version 1 format maps key sequences to the six supported commands and may give a site selected keys or the whole page:

```json
{
  "version": 1,
  "enabled": false,
  "bindings": {
    "j": "scroll-down",
    "k": "scroll-up",
    "gg": "scroll-top",
    "G": "scroll-bottom",
    "f": "open-link",
    "Shift+F": "open-link-background"
  },
  "passthrough": {
    "youtube.com": { "keys": ["k"] },
    "editor.example": { "all": true }
  }
}
```

Site rules match the named host and its subdomains. Tanto rejects unknown schema versions and command names instead of loading part of the file.

## Performance

Build and startup budgets are recorded in [ADR 0007](adr/0007-keep-dependencies-out-of-the-fast-build.md). Run `scripts/benchmark_build.sh dev` to measure configure, clean build, incremental C++ rebuild, and a no-op build with the same preset. Record launch time separately when updating the baseline.

Baseline measured on the initial macOS development machine on 2026-08-29:

- Fresh configure: 1.613 seconds
- Clean development build using the project-local ccache, including the browser, UI lab, and tests: 3.340 seconds
- Incremental C++ rebuild and relink after changing `ThemeController.cpp`: 0.252 seconds
- No-op build check: 0.020 seconds, with no compile or link work

These timings exclude application launch and prebuilt Qt installation. Re-run `scripts/benchmark_build.sh` after changing target boundaries or build settings.
