# Development

## Requirements

- A current Linux development environment, or macOS 13 or newer with Xcode 15 or newer
- Qt 6.11 at or above the repository's approved patch, including Qt Quick, Qt SQL, and Qt WebEngine
- CMake 3.30 or newer
- Ninja
- Clang with C++23 support
- ccache

The canonical macOS Qt SDK comes from Qt's official Apple Silicon distribution. Homebrew Qt is a
convenience option only when it meets the approved security patch level.

### Virtual machines

A guest with virtualized graphics, for example `Mesa virgl` on virtio-gpu, runs Qt Quick fine but
can leave the page transparent: the window's chrome paints and the desktop shows through where the
web view should be. Qt asks Chromium for ANGLE, ANGLE asks the driver for a context version it does
not have, and nothing is composited. The log says so under
`QT_LOGGING_RULES='qt.webenginecontext.debug=true'`:

```
EGL Driver message (Error) eglCreateContext: Requested version is not supported
```

Give Chromium a rendering path the guest can serve:

```sh
QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu ./build/dev/omaweb
```

`--use-angle=gl`, which would have kept the acceleration virgl can offer, does not work here. The
whole `--disable-gpu` hammer is the one that lands. The flag has to come through the environment:
QtWebEngine builds Chromium's command line from `QTWEBENGINE_CHROMIUM_FLAGS`, and the same flag on
Omaweb's own argv is ignored.

This is a property of the guest's graphics rather than of Omaweb, so it stays an environment
variable rather than something the build decides. Omaweb refuses `--no-sandbox`, `--single-process`,
`--in-process-gpu`, and `--in-process-network-service` (see [Security rules](#security-rules)); the
rendering flags above are not among them.

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
./build/ui/omaweb-ui-lab
```

On macOS the executable is inside the bundle, at
`./build/ui/omaweb-ui-lab.app/Contents/MacOS/omaweb-ui-lab`. Pass `--private` to paint the window in
the private palette, which is the only way to review that chrome without opening a private window.
`OMAWEB_THEME_FILE` points the lab at one theme file, as it does the browser, so a palette can be
reviewed without installing it. Pass `--show collapsed`, `--show settings`, `--show history` or
`--show shortcuts` to open the state a capture cannot press a key to reach. A state can name a
section and a dialog over it: `--show settings:<section>` opens one of `tabs`, `keyboard`,
`content-blocking`, `network`, `downloads`, `search`, `privacy` or `about`, and
`--show settings:privacy:clear` stands the clear-browsing-data dialog on Privacy. Naming the section
is how a layout change is reviewed at a font size the page was not written at: point
`OMAWEB_THEME_FILE` at a theme whose `font.size` is larger and capture each section in turn.
`--show site` opens Site information, which is a click on the address trigger. Pass
`--capture <path>` to render one frame to a PNG and exit, which works headlessly with
`QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` for reviewing chrome changes without a desktop
session. Development presets load QML, themes, and the icon font directly from the source tree.
Editing those files requires an application restart but no compile or relink.

The `ladybird` preset is deliberately separate. Do not add Ladybird, Qt source builds, or Rust
compilation to `dev`.

Before the first configure, build the pinned Content-blocking library into the disposable local
cache:

```sh
scripts/bootstrap_content_blocker.sh
```

The script uses `third_party/content-blocker/Cargo.lock`, records the shared library checksum, and
pins `adblock` 0.12.5 to Ladybird revision `e5a41dfb6930fe5471c2c203d2dc32a1a782e816`. CMake
verifies the cached artifact but never invokes Cargo. Run the bootstrap again only after changing
the Rust wrapper, its manifest, or its lockfile.

### Platform gaps on Linux

`omaweb-platform` supplies the window-system services the browser cannot supply itself. On Linux
they are session-bus services rather than window-server ones, which is the shape the whole layer
takes here: `LinuxSystemNotifier.cpp` talks to `org.freedesktop.Notifications`, and
`LinuxPagePrinter.cpp` to `org.freedesktop.portal.Print`. A desktop offering neither reports both
capabilities off, which is the same answer the macOS build gives without a window server, so the
contract the shell reads does not change between platforms.

Printing goes through the portal rather than through Qt's own print dialog, which would cost the
browser a QtWidgets dependency, and the portal is also the only route to a printer from inside a
sandbox. Sending no print token is what makes the portal show its dialog; a token stands for
settings the reader has already answered. The dialog comes up unparented, because exporting Omaweb's
surface for it to sit over needs a handle Qt does not offer through public API.

`omaweb-notification-service` and `omaweb-print-portal` cover both exchanges against stub services
on a private bus under `dbus-run-session`. They reach what a live desktop cannot be asked for in a
test: the reader answering a notification, and the rendered document surviving the spooled copy
being taken away.

Two things that look like gaps are not. The frameless window is one: `Main.qml` asks for
`Qt.FramelessWindowHint` off macOS, so a Main or Private window is already frameless under Hyprland.

The blur behind a transparent surface is the other, and `installWindowChrome` is empty on Linux
because there is nothing for it to do. Hyprland implements no client-side blur protocol, so a client
cannot ask for blur; the compositor blurs the desktop behind a surface's translucent pixels
according to its own `decoration:blur` setting. Omarchy ships that setting off, so a stock desktop
shows the wallpaper sharp through Omaweb's surfaces, which is the desktop's decision rather than a
missing implementation. `integrations/omarchy/README.md` has the rule that turns it on. macOS is the
platform where blur is the application's to install, so an `installWindowChrome` change is reviewed
there.

### Installing and opening links

`cmake --install` puts the browser at `bin/omaweb`, the bootstrapped content-blocking library under
`lib/omaweb`, and the desktop entry, icon and licences under `share`. The library carries no soname,
so the build is told as much: without that, linking it by path records that path as the dependency
itself and an installed copy would look for it in whatever directory happened to build it. The
runpath is `$ORIGIN`-relative for the same reason, because a package chooses its prefix when it
installs rather than when it configures.

The desktop entry stays `omaweb.desktop` rather than taking the application's bus name, which the
freedesktop convention would ask for. Qt reads the desktop file name as the Wayland app id and the
X11 window class, and a desktop's window rules are written against that, so renaming it would break
every rule pointing at `omaweb`. The cost is `DBusActivatable`, which needs the entry and the bus
name to match; the launcher runs `Exec` instead.

Opening a link runs the browser again, and the second process does not become a second browser.
`RunningBrowser` claims `dev.omaweb.browser` on the session bus, and a launch that finds the name
taken hands its address to the owner over `org.freedesktop.Application` and exits without building
an engine, a session store or a filter set. Omaweb is one window with its tabs down the side, so a
handed-over address arrives as a tab.

Addresses from outside the browser are read strictly and only `http`, `https` and `file` are opened.
A desktop passes on whatever it was given, so a scheme that would run in a page is refused rather
than resolved.

Being the default browser is the desktop's setting, so Settings reads it and offers to change it,
under About. Omaweb never takes it because it happened to start: the reader is the only one who
knows what they were using before. `xdg-settings` is what the offer goes through, rather than
`mimeapps.list` being edited here, because that file is only where most desktops keep the answer and
a browser editing a shared file would have to understand everything else in it. A desktop without
that tool is offered nothing rather than offered something that fails.

### The Arch package

`packaging/PKGBUILD` builds `omaweb-git` from the repository. There is no release tarball to build
from yet, so the version comes from the nearest release tag through `git describe`, which is where
CMake takes it from as well, and the two cannot disagree
([ADR 0028](adr/0028-derive-the-version-from-the-release-tag.md)).

Qt is a dependency rather than a bundle, which is
[ADR 0013](adr/0013-preserve-engine-sandboxes-in-every-build.md)'s Linux packaging decision: an
engine security update is then the distribution's to ship rather than Omaweb's to rebuild for.
`qt6-wayland` is a dependency in its own right because native Wayland is the primary display
platform. The content-blocking library is the one thing that rides along, under `lib/omaweb`,
because no distribution package supplies it.

`fcitx5-qt` is an optional dependency, and the reason is worth knowing. Omarchy sets
`QT_IM_MODULE=fcitx` for every Qt application in
`/usr/share/omarchy/default/environment.d/10-omarchy-fcitx.conf`, but does not install the plugin
that name refers to. Qt then loads no input context, the Wayland text-input protocol is never bound,
and an input method silently does nothing. Under `WAYLAND_DEBUG=1` the difference is visible:
`zwp_text_input_manager_v3` is advertised and never bound, and with `QT_IM_MODULE` unset both it and
`v1` are. This is not Omaweb-specific, but the package is where it can be answered.

```sh
scripts/check_package.sh
```

builds the package and checks what it carries: the binary, the library, the desktop entry, the icon
and the licences, and nothing outside those directories. Run as root in a container it goes on to
install, upgrade over itself and remove, checking that a file in the reader's configuration and the
rest of the system come through untouched. It refuses to install on a host that is not disposable,
because that would be putting a package on the machine of whoever ran a check.

## Security rules

Never use Chromium's `--no-sandbox`, `--single-process`, in-process network-service flags, or
`QTWEBENGINE_DISABLE_SANDBOX` in project scripts. Fix the host configuration or packaging instead.

Omaweb checks its command line and `QTWEBENGINE_CHROMIUM_FLAGS` for these switches and refuses to
start if it finds one. It also stops when the host cannot meet the renderer sandbox requirements. On
Linux, this includes disabled or unavailable unprivileged user namespaces, missing seccomp-bpf
support, an unreadable `/proc`, and running as root. The error identifies the failed check and its
required value. You cannot disable this check.

`security/baseline.json` records the approved QtWebEngine version and the latest Chromium security
patch it includes. Settings reports whether the running engine meets this baseline.
`.github/workflows/security-baseline.yml` compares it with upstream releases each week. See
[`SECURITY.md`](../SECURITY.md) for the update process.

## Theme

Omaweb reads `theme.json` from the configuration directory when one is present, falling back to the
desktop-managed theme (Omarchy, on Linux) and then the built-in palette. `OMAWEB_THEME_FILE`
overrides all three. The order is not resolved once: every candidate is watched, so a file that
outranks the one in use takes over the moment it appears, without a restart.

On Omarchy, a stock desktop washes every window to 0.985 opacity through a Hyprland window rule, and
that reaches the webpage viewport Omaweb paints opaque. Omaweb states its window class as `omaweb`
so the exemption can be written against it; `integrations/omarchy/README.md` has the rule.

On Linux, following the desktop's theme needs no setup. The first start on a machine that has
Omarchy installs `integrations/omarchy/omaweb.json.tpl` to `~/.config/omarchy/themed/` and asks
Omarchy to render the active theme through it. A template already there stands. See
[`integrations/omarchy/README.md`](../integrations/omarchy/README.md) for the overrides, including
`OMAWEB_NO_OMARCHY_TEMPLATE`.

`scripts/import_terminal_theme.py` derives that file from the terminal it runs in. Run it from the
terminal whose colours you want, which `TERM_PROGRAM` identifies exactly, and it writes `theme.json`
into the configuration directory, which `ThemeController` is watching, so a running Omaweb repaints
without a restart. It reads Ghostty (via `ghostty +show-config`, which resolves a named theme into
concrete colours), iTerm2, kitty, Alacritty, and a customised Terminal.app profile. `--print` shows
the result instead of writing it, `--terminal` overrides detection, and `--force` replaces an
existing theme. On a Linux desktop that already renders Omaweb's template, the script refuses to
write and says so: a theme in the configuration directory outranks the desktop-rendered one, so
importing would freeze the palette at whatever the terminal looked like that day.

The derivation takes the terminal's background, foreground and sixteen ANSI colours and builds the
chrome ladder by mixing in OKLab, with the step sizes measured off the default theme. The accent is
the blue, magenta or cyan slot with the most chroma that clears 4.5:1 against the background. Two
slots within a tenth of each other count as equally vivid, and then the earlier slot wins, so a
Tokyo Night import keeps its blue instead of losing it to a magenta a rounding error more saturated.
Red, green and yellow are left alone because they already mean error, success and warning. The
private accent is the magenta the accent did not take, or the accent turned 32 degrees towards
magenta when it did; the grounds it is cast over are not written out, because Omaweb tints them from
the theme's own surfaces when it loads any palette. The type base size, tint and the semantic
opacities stay as the default theme sets them, because a terminal's `background-opacity` is a
window-wide setting and does not translate to Omaweb's per-surface opacity.

`theme.json`'s `font` block is `families` and `size`. `families` is a preference order and the first
family the host actually has installed wins, so a theme can name a font it would like without
breaking a machine that lacks it; nothing is ever handed to Qt that Qt cannot find, because a
missing family costs a font-alias sweep at startup and then draws in whatever face Qt substitutes.
`size` is the root the whole type scale grows from: every size the interface asks for is derived
from it by the Omarchy kit, which `ThemeController` drives. See
[ADR 0018](adr/0018-drive-the-kit-from-the-theme-palette.md).

## Interface components

Shared controls come from the Omarchy shell's QML kit, vendored under `third_party/omarchy-shell`
and pinned by `MANIFEST.json`. Both projects are QML on Qt 6, so the kit is used as-is rather than
reimplemented.

Vendored files are byte-for-byte copies and are never edited. `ctest` fails on any local change.
Omaweb meets the kit from three sides: `src/ui/quickshell-shim` registers the `Quickshell` and
`Quickshell.Io` types the kit's singletons import, the QML in `src/ui` adapts components to Omaweb's
call sites, keeping Omaweb's property names and its accessibility annotations, and
`src/ui/KitTheme.cpp` drives the kit's `qs.Commons` colour and type singletons from the theme
palette so they follow `ThemeController` rather than an Omarchy theme on disk. The vendor root is on
the QML import path in source builds and lands under `qrc:/qt/qml` in resource builds.

Running on Omarchy itself means the real Quickshell is already installed in Qt's qml directory,
where a module on the import path beats the shim's C++ registration. The shim ships a qmldir for
each URI it claims and puts them first, and `omaweb-qml-load-over-quickshell` loads the whole UI
with a decoy Quickshell on the import path so a regression fails on any host rather than only on
Omarchy.

```sh
scripts/sync_omarchy_ui.py --verify          # the local tree matches the manifest
scripts/sync_omarchy_ui.py --check-upstream  # what changed on quattro since the pin
scripts/sync_omarchy_ui.py --sync --ref <sha>
```

Upstream's branch moves, so a sync is deliberate: move the pin, read the diff, and run `ctest`.
`omaweb-ui` instantiates the adapted components and asserts the kit's tokens resolve, so an upstream
API change fails there.

Nothing moves the pin on its own, so the `Omarchy kit drift` workflow does the looking: every Monday
it runs `--check-upstream --report drift.json` and hands the report to
`scripts/report_omarchy_drift.py`, which keeps one `omarchy-drift` issue listing the added, removed,
and changed files with a compare link, and closes it once the pin catches up. It never syncs. An
upstream API change lands on Omaweb's adapters, so the diff wants a reader.
`ctest -R omaweb-omarchy-drift` covers both halves with GitHub stubbed out.

## Content-blocking scriptlets and substitutes

A `##+js(...)` filter rule names a function from uBlock Origin's scriptlet library and a
`$redirect=` rule names a body from its web-accessible resources. Both sets are vendored under
`third_party/ubo-scriptlets` and pinned by `MANIFEST.json` the same way the Omarchy kit is. A rule
supplies a name and arguments; it never supplies code or a body, so the set of either that can reach
a page is the set in the repository. See [ADR 0025](adr/0025-run-only-vendored-scriptlets.md) and
[ADR 0026](adr/0026-serve-substitutes-under-an-omaweb-scheme.md).

`scriptlets.json` and `redirects.json` beside the copies are the same two sets as `adblock-rust`
resource descriptors, which the content blocker builds into its own binary. They are generated
rather than written: both sets describe themselves in JavaScript, so
`scripts/build_ubo_scriptlets.mjs` and `scripts/build_ubo_redirects.mjs` import them under Node and
ask. Their digests are pinned in the manifest too, so `ctest` fails if a generated file and the
copies disagree. A build needs neither Node nor the network.

```sh
scripts/sync_ubo_scriptlets.py --verify           # the local tree matches the manifest
scripts/sync_ubo_scriptlets.py --check-upstream   # what changed since the pin
scripts/sync_ubo_scriptlets.py --sync --ref 1.70.0
```

A sync re-fetches the copies and regenerates both descriptor files from them. Read the upstream diff
before taking it: this is the one dependency whose contents run inside the pages the browser loads.
Both vendored trees share one integrity test, `tests/cmake/check_vendored_tree.cmake`.

## Keyboard navigation configuration

Omaweb copies `assets/keybindings/default.json` to `keybindings.json` in the configuration directory
on first launch, at `$XDG_CONFIG_HOME/omaweb` or `~/.config/omaweb` when that is unset. A file left
by an earlier version under the application data directory is moved there. Set `OMAWEB_CONFIG_ROOT`
to relocate the whole directory, or `OMAWEB_KEYBINDINGS_FILE` to load one specific file during
development. The version 1 format maps key sequences to the supported commands and may give a site
selected keys or the whole page:

```json
{
  "version": 1,
  "enabled": false,
  "bindings": {
    "j": "scroll-down",
    "k": "scroll-up",
    "d": "scroll-half-page-down",
    "u": "scroll-half-page-up",
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

Site rules match the named host and its subdomains. Omaweb rejects unknown schema versions and
command names instead of loading part of the file.

## Releases

The version comes from the nearest `v*` tag, so cutting a release is a tag and a push:

```sh
git tag v0.2.0
git push origin v0.2.0
```

The `Release` workflow refuses a tag that is not on `main`, generates notes from the Conventional
Commit subjects since the previous tag with `scripts/release_notes.sh`, and publishes them. Every
`v0.*` tag is marked a prerelease. No binaries are attached. See
[ADR 0028](adr/0028-derive-the-version-from-the-release-tag.md).

`cmake --preset dev` prints the version it derived. A tree with no tags falls back to
`OMAWEB_FALLBACK_VERSION` in `cmake/OmawebVersion.cmake`.

## Performance

Build and startup budgets are recorded in
[ADR 0007](adr/0007-keep-dependencies-out-of-the-fast-build.md). Run
`scripts/benchmark_build.sh dev` to measure configure, clean build, incremental C++ rebuild, and a
no-op build with the same preset. Record launch time separately when updating the baseline.

Baseline measured on the initial macOS development machine on 2026-08-29:

- Fresh configure: 1.613 seconds
- Clean development build using the project-local ccache, including the browser, UI lab, and tests:
  3.340 seconds
- Incremental C++ rebuild and relink after changing `ThemeController.cpp`: 0.252 seconds
- No-op build check: 0.020 seconds, with no compile or link work

These timings exclude application launch and prebuilt Qt installation. Re-run
`scripts/benchmark_build.sh` after changing target boundaries or build settings.
