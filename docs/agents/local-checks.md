# Local checks on a Mac

CI builds on Arch Linux. A Mac runs most of the suite natively, but its formatters, its compiler and
its Qt differ from CI's, so some checks have to run in a container to mean what CI means.

## Tests

In a fresh worktree, run `scripts/bootstrap_content_blocker.sh` first (about 20 seconds), then
`cmake --preset ui` and `ctest --preset ui`. That builds and runs the whole suite with Homebrew Qt,
QML tests included. Run one QML test as `build/ui/omaweb-ui-tests BrowserChrome::test_name` with
`QT_QPA_PLATFORM=offscreen`.

- `omaweb-window-chrome` needs the native platform. It crashes under `QT_QPA_PLATFORM=offscreen`,
  and passes alone with `QT_QPA_PLATFORM=cocoa`.
- `omaweb-sync` and its tests build only on Linux, because they need libsecret. Run them in an arm64
  `lopsided/archlinux:devel` container with
  `qt6-base qt6-declarative qt6-wayland libsecret libsodium rust cmake ninja gcc ccache`, configured
  into a build directory inside the container, not under the mounted worktree. A QtTest function
  filter there is the bare name, not `Class::name`.
- Several workers building at once on one machine starve each other. Set
  `CMAKE_BUILD_PARALLEL_LEVEL` and `ctest -j` to a share of the cores.

## Formatting

The Mac's Xcode `clang-format` and Homebrew `qmlformat` reflow code a change never touched. Run
`scripts/format.sh` in CI's own image: `archlinux:base-devel` with `--platform linux/amd64`, and
`DisableSandbox` under `[options]` in `pacman.conf`, because pacman's sandbox fails under emulation.
Mount the repository root at its own path so a worktree's `.git` resolves.

qmlformat 6.11 reads a bare object literal after `?` (`cond ? { ... } : ({})`) as a block and
re-indents the rest of the file. Build the object in a function instead.

## Compilers

The arm64 lab container builds with GCC, and CI also builds with clang under `-Werror`. Before
pushing, compile each changed C++ file with `clang++` from `build/ci/compile_commands.json`: a
`QVariantList({x})` once passed GCC and failed clang.

## Screenshots

What the UI lab can capture depends on the renderer:

| Backend                                                        | Draws                                        | Misses                                                                                                       |
| -------------------------------------------------------------- | -------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| `offscreen`, software                                          | layout, text, colour                         | `MultiEffect` blur (PageBackdrop, the Omnibar's glass) and every `ShaderEffect` (the Start page's CRT glass) |
| headless cage in an arm64 `lopsided/archlinux:devel` container | all of it, through OpenGL on Mesa's llvmpipe | nothing; output is 1280×720                                                                                  |
| `QT_QPA_PLATFORM=cocoa` on the Mac                             | all of it, through Metal                     | not Linux; a quick look only                                                                                 |

For cage, install `cage qt6-wayland mesa`, run as a non-root user with
`WLR_BACKENDS=headless WLR_RENDERER=pixman`, `QT_QPA_PLATFORM=wayland` and `QT_SCALE_FACTOR=1`, and
capture with the lab's `--capture`. Offscreen shots use `QT_SCALE_FACTOR=2`. A blur missing from an
offscreen shot is the renderer, not a regression. The build needs `qt6-shadertools` (Homebrew
`qtshadertools`).

## Sound

A headless browser plays Web Audio through the machine's speakers. Mute every automated audio run:
`--mute-audio` for Chrome, and `media.volume_scale` set to `"0.0"` in a Firefox profile's `user.js`.
