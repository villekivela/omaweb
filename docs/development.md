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

On Linux, the executable is `./build/ui/tanto-ui-lab`. Development presets load QML, themes, and the icon font directly from the source tree. Editing those files requires an application restart but no compile or relink.

The `ladybird` preset is deliberately separate. Do not add Ladybird, Qt source builds, or Rust compilation to `dev`.

## Security rules

Never use Chromium's `--no-sandbox`, `--single-process`, in-process network-service flags, or `QTWEBENGINE_DISABLE_SANDBOX` in project scripts. Fix the host configuration or packaging instead.

## Performance

Build and startup budgets are recorded in [ADR 0007](adr/0007-keep-dependencies-out-of-the-fast-build.md). Run `scripts/benchmark_build.sh dev` to measure configure, clean build, incremental C++ rebuild, and a no-op build with the same preset. Record launch time separately when updating the baseline.

Baseline measured on the initial macOS development machine on 2026-08-29:

- Fresh configure: 1.613 seconds
- Clean development build using the project-local ccache, including the browser, UI lab, and tests: 3.340 seconds
- Incremental C++ rebuild and relink after changing `ThemeController.cpp`: 0.252 seconds
- No-op build check: 0.020 seconds, with no compile or link work

These timings exclude application launch and prebuilt Qt installation. Re-run `scripts/benchmark_build.sh` after changing target boundaries or build settings.
