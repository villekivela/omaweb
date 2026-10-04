# Local checks

CI builds on Arch Linux, in `archlinux:base-devel`. A check run locally means what CI means only
when it runs on the same tools. On an up-to-date Arch machine most of them do. Elsewhere, run the
ones below in CI's image.

## Tests

In a fresh worktree, run `scripts/bootstrap_content_blocker.sh` first (about 20 seconds), then
`cmake --preset ui` and `ctest --preset ui`. Run one QML test as
`build/ui/omaweb-ui-tests BrowserChrome::test_name` with `QT_QPA_PLATFORM=offscreen`. A QtTest
function filter for a C++ test is the bare name, not `Class::name`.

- `omaweb-window-chrome` needs a real platform plugin. It crashes under `QT_QPA_PLATFORM=offscreen`.
- `omaweb-sync` and its tests build only on Linux, because they need libsecret.
- Several builds at once on one machine starve each other. Set `CMAKE_BUILD_PARALLEL_LEVEL` and
  `ctest -j` to a share of the cores.

## Formatting

`scripts/format.sh` must run with CI's `clang-format` and `qmlformat`. Other versions reflow code a
change never touched. On Arch with current packages, run it natively. Anywhere else, run it in
`archlinux:base-devel` (`--platform linux/amd64` on an arm64 host, with `DisableSandbox` under
`[options]` in `pacman.conf`, because pacman's sandbox fails under emulation). Mount the repository
root at its own path so a worktree's `.git` resolves.

qmlformat 6.11 reads a bare object literal after `?` (`cond ? { ... } : ({})`) as a block and
re-indents the rest of the file. Build the object in a function instead.

## Compilers

CI builds with both GCC and clang, each under `-Werror`, and they disagree: a `QVariantList({x})`
once passed GCC and failed clang. Before pushing C++, compile each changed file with the other
compiler too, using `build/ci/compile_commands.json`.

## Screenshots

What the UI lab can capture depends on the renderer:

| Backend                           | Draws                                        | Misses                                                                                                       |
| --------------------------------- | -------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| `offscreen`, software             | layout, text, colour                         | `MultiEffect` blur (PageBackdrop, the Omnibar's glass) and every `ShaderEffect` (the Start page's CRT glass) |
| the desktop's own Wayland session | all of it, on the real GPU                   | nothing                                                                                                      |
| headless cage                     | all of it, through OpenGL on Mesa's llvmpipe | nothing; output is 1280×720                                                                                  |

For cage, install `cage qt6-wayland mesa`, run as a non-root user with
`WLR_BACKENDS=headless WLR_RENDERER=pixman`, `QT_QPA_PLATFORM=wayland` and `QT_SCALE_FACTOR=1`, and
capture with the lab's `--capture`. Offscreen shots use `QT_SCALE_FACTOR=2`. A blur missing from an
offscreen shot is the renderer, not a regression. The build needs `qt6-shadertools`.

## Sound

A headless browser plays Web Audio through the machine's speakers. Mute every automated audio run:
`--mute-audio` for Chrome, and `media.volume_scale` set to `"0.0"` in a Firefox profile's `user.js`.

## On macOS

macOS is a development platform, not a distribution target. There:

- Homebrew Qt runs the `ui` preset natively, QML tests included. Run `omaweb-window-chrome` with
  `QT_QPA_PLATFORM=cocoa`.
- Xcode's `clang-format` and Homebrew's `qmlformat` differ from CI's, so format in the container.
- Sync tests, a GCC build and Linux screenshots need an arm64 `lopsided/archlinux:devel` container,
  configured into a build directory inside the container, not under the mounted worktree. Install
  `qt6-base qt6-declarative qt6-wayland libsecret libsodium rust cmake ninja gcc ccache`, plus
  `cage` for screenshots.
- `QT_QPA_PLATFORM=cocoa` draws blur and shaders through Metal, for a quick look only.
- Homebrew's `qtshadertools` provides the shader tools.
