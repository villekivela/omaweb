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
