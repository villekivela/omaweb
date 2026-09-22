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

Go around ANGLE to the driver's own EGL:

```sh
QTWEBENGINE_CHROMIUM_FLAGS=--use-gl=egl ./build/dev/omaweb
```

Chromium then composites on virgl's OpenGL ES directly, and the page keeps everything the GPU
process gives it: `backdrop-filter`, WebGL, canvas, and hardware video decode. The flag has to come
through the environment: QtWebEngine builds Chromium's command line from
`QTWEBENGINE_CHROMIUM_FLAGS`, and the same flag on Omaweb's own argv is ignored.

`--disable-gpu` also paints the page, and used to be the advice here. It does so by putting Chromium
on its software compositor, which drops `backdrop-filter` and WebGL: a page with a glass header
shows the header with no blur, in Omaweb alone, and looks like a bug in the page. Measured on virgl
with Mesa 26.2, `--use-angle=gl` and `--use-angle=gles` fall into the same ANGLE hole,
`--use-angle=vulkan` lands on lavapipe and composites in software too, and Chromium's own
`chrome://gpu` is not reachable, so read the state over `--remote-debugging` with the DevTools
`SystemInfo.getInfo` method: `featureStatus.gpu_compositing` says `enabled` when the flag took.

This is a property of the guest's graphics rather than of Omaweb, so it stays an environment
variable rather than something the build decides. A shell that exports it for every launch is where
a rendering difference between Omaweb and the desktop's Chromium comes from first; the variable's
name does not start with `QT_`, so `env | grep QT_` does not show it. Omaweb refuses `--no-sandbox`,
`--single-process`, `--in-process-gpu`, and `--in-process-network-service` (see
[Security rules](#security-rules)); the rendering flags above are not among them.

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
reviewed without installing it. Pass `--show collapsed`, `--show peek`, `--show settings`,
`--show history` or `--show shortcuts` to open the state a capture cannot press a key to reach. Use
`--tabs --show peek` to review the floating sidebar and its page blur. A state can name a section
and a dialog over it: `--show settings:<section>` opens one of `tabs`, `keyboard`,
`content-blocking`, `network`, `downloads`, `search`, `privacy` or `about`, and
`--show settings:privacy:clear` stands the clear-browsing-data dialog on Privacy. Naming the section
is how a layout change is reviewed at a font size the page was not written at: point
`OMAWEB_THEME_FILE` at a theme whose `font.size` is larger and capture each section in turn.
`--show site` opens Site information, which is a click on the address trigger. Pass `--tabs` to seed
the Space with a day's worth of tabs, some of them pinned: the lab otherwise comes up on a Space at
rest, which draws neither the Pinned section nor the tab list, so the sidebar is the one part of the
chrome a capture cannot reach. The blank tab stays the one on show, so the viewport still draws the
Start page. `--spaces` seeds two more Spaces with pages of their own, before the interface loads, so
a Space switch has somewhere to go. Pass `--capture <path>` to render one frame to a PNG and exit,
which works headlessly with `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` for reviewing
chrome changes without a desktop session. The chrome's movements are reviewed the same way:
`--show space-step`, `tab-step`, `omnibar-step` and `settings-step` run the switch or the opening
shortly before the capture, so the frame lands part way through it, and the `-settled` spelling of
each runs it early enough to land at rest. The leaving list's picture and the command panel's growth
need the hardware renderer, so those two captures go without `QT_QUICK_BACKEND=software`.
Development presets load QML, themes, and the icon font directly from the source tree. Editing those
files requires an application restart but no compile or relink.

The `release` preset compiles the QML ahead of time instead: the shared UI, the vendored kit and the
engine view each become a static library that `qt_add_qml_module` runs `qmlcachegen` over, so
bytecode and the bindings the compiler can type ship in the binary and the first launch parses no
QML. The kit's own `qmldir` files draw its module boundaries and travel unchanged. Only the release
build graph carries these compile steps, and `omaweb-qml-build-graph` checks that in both
directions.

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

### What CI runs

`.github/workflows/ci.yml` runs seven jobs, on a pull request and on a push to `main`. `style` runs
the formatters, `qmllint`, the website's own policy check and the release-page tests;
`commit-messages` checks every non-merge subject in the range; and four Arch containers build and
publish the tree: `arch-linux` under clang, which goes on to build the `release` preset and load the
compiled QML, `arch-linux-gcc` under GCC, `arch-package` through `scripts/check_package.sh`, and
`pacman-repo` through `scripts/check_repo_publish.sh`.

Those four cost between two and ten minutes each, and a change confined to `docs/`, `website/` or
Markdown cannot break a compile, so a `changes` job decides whether they run at all. It prints the
files it decided on, and the same question can be asked of any range:

```sh
scripts/source_changed.sh origin/main
```

The test is inverted on purpose. Everything counts as source unless it is prose, so a directory
nobody has thought of yet builds rather than quietly skipping: the cost of forgetting that way is
ten minutes, and the cost of the other way is a break that reaches `main`. Prose beside source is
source, and a range with no base to compare against builds.

It is a gate job rather than a `paths-ignore:` on the workflow, and the two are not interchangeable.
`arch-linux` is a required check on `main`. A job skipped by a job-level `if:` reports as skipped
and the pull request still merges; a workflow skipped by path filtering leaves its required checks
pending indefinitely, with no job to re-run. Path filtering would also take `style` with it, and
that has to run on prose.

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
settings the reader has already answered. The dialog stands over the window the reader asked from,
which takes a name for that window: `wayland:<handle>` for a surface exported through
`zxdg_exporter_v2`, or `x11:<xid>`. Qt exports the surface already, because its own dialogs are
portal dialogs and they are parented, and `LinuxPortalWindow.cpp` is the one place Omaweb reaches
past Qt's public API to read the name back. Exporting the surface a second time would need the
window's `wl_surface`, which is no more public. A desktop that gives no name gets an empty one and
places the dialog itself, which is what every print did before.

That private call has one cost worth stating plainly. Qt warns that a private module ties a build to
one Qt build, and it is right: `portalWindowIdentifier` is reached as a virtual through
`QDesktopUnixServices`, so a Qt that renamed or removed it would break the build, and a Qt that kept
the name and moved it in the vtable would not — printing would call whatever took its place. The
only other private Qt in Omaweb is a Quick profile's settings, which
[ADR 0047](adr/0047-reach-a-quick-profiles-font-settings-through-private-qt.md) accounts for on the
same terms and whose exposure is a page drawn in the engine's own fonts and a call offered every
interface the machine has.

So the browser does not ask on a Qt it was not compiled against. `portalNameIsSafeToAsk` compares
`QT_VERSION_STR` with `qVersion()` and wants them identical, not compatible: public Qt keeps its ABI
across a patch release and private Qt promises nothing. A browser that meets a Qt it was not built
against prints with an unparented dialog, which is a placement rather than a crash and is what
printing did before any of this. Rebuilding gives the dialog its parent back, which is what a Qt
upgrade should be followed by anyway.

Pinning an exact `qt6-base` in the package would be the other answer, and it is worse: it makes the
package unbuildable the day the distribution moves a patch release, which is a hard failure in place
of a placement. CMake's warning about the private module is turned off around the one `find_package`
that raises it, because a warning printed on every configure that nobody can act on is how people
learn to read past the ones that matter.

`omaweb-notification-service` and `omaweb-print-portal` cover both exchanges against stub services
on a private bus under `dbus-run-session`. They reach what a live desktop cannot be asked for in a
test: the reader answering a notification, and the rendered document surviving the spooled copy
being taken away.

Omaweb takes two connections on the AT-SPI bus and only one of them is its own. The populated one is
Qt's bridge, carrying the frame, its named buttons, the page tabs and the rest of what the
`Accessible.*` annotations in `src/ui/*.qml` state. The empty one reports `gtk` as its toolkit and
has no children, because Omarchy sets `QT_QPA_PLATFORMTHEME=gtk3` and Qt's GTK platform theme plugin
loads GTK itself, which registers an application through `libatk-bridge-2.0` whether or not GTK
draws anything. Every Qt application on that desktop does it, quickshell included, so an assistive
client listing what is running finds a pair for each of them. `NO_AT_BRIDGE` would take Omaweb's
empty one away, but the file chooser that same plugin supplies is a GTK window a reader may need
read to them, and a browser whose Open dialog has gone quiet is worse off than one listed twice.

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

### Driving the session

```sh
scripts/check_wayland_session.py
```

drives a browser through its own commands on the desktop that is running, which is the part of the
Wayland port no headless suite reaches. Keys go in through `hl.dsp.send_shortcut`, Hyprland's own
dispatcher, so no key synthesiser has to be installed; what they did is read back from
`hyprctl clients`, `wl-paste` and the window title. It reads no pixels. A check that has to look at
the screen has to capture the desktop, and the desktop belongs to whoever is sitting at it.

The browser it drives is a second one, on a private session bus so it does not hand its argument to
the browser already open and exit, and on throwaway directories so it browses nothing of the
reader's. Off a Wayland session, or without Hyprland, it says it skipped and succeeds, so it is safe
to run anywhere and is not a CI gate.

It covers browser fullscreen taken and handed back, the sidebar commands resizing the sidebar rather
than the window, a Private window opening as a window of its own, the clipboard crossing in both
directions including the primary selection, and every browser binding in
`assets/keybindings/default.json` answering without leaving the browser unable to take the next one.
Four commands are not sent, each because sending it would end the run rather than test it: `print`
and `open-file` open dialogs only a person can answer, `minimize-window` unmaps the window every
later check reads, and `private-window` has a check of its own.

Two things stay a person's. Moving and resizing the window by its frameless regions needs a pointer
button no dispatcher synthesises. Becoming the default browser changes the machine that runs the
check.

One finding worth keeping: a clipboard check has to focus the window first. Wayland lets a client
set the selection only while it holds keyboard focus, so `Primary+Shift+C` sent to an unfocused
window fires the command and copies nothing. That is the protocol rather than the browser.

### Announcing the sounding tab

```sh
scripts/check_mpris_export.sh
```

drives the path a page's media session takes to the desktop: the first-party script in the page, the
console channel the adapter reads it off, `SoundingTabs` picking the tab, and the window handing a
media key back to the page. The unit tests cover each end of that on their own, and neither of them
covers the join.

It starts a browser of its own on a page that declares a media session, then reads the player and
sends it commands the way a bar's media widget does. The browser runs on a session bus of its own
and on throwaway directories, for the same reasons the Wayland check does: on the reader's bus the
address would be handed to the browser already open, and the player would land in the reader's own
bar. It needs a graphical session, so it is a check to run by hand rather than a CI gate.

It says what the page declared, whether a media key reached the page's own handler, and whether the
player went when the browser did. The bus it ran on is torn down after the last result, and its
daemon prints as it goes; nothing below the last `OK` line is a result.

### What a call learns about the machine

```sh
scripts/check_webrtc_candidates.sh
```

asks the engine what a page setting up a call is told about this machine's addresses, once with the
WebRTC address policy on and once with it off. The contract test reads the policy back off a
profile's settings, which is what the engine promises and all a runner with one interface can check;
this starts the browser on a page that gathers ICE candidates and prints what the page saw. On, the
page sees the server-reflexive address of the default route and no host candidate. Off, it sees a
host candidate for each interface as well, each hidden behind an mDNS name because the page holds no
media permission, so the difference reads as candidates present or absent rather than as addresses.
It puts a browser window on screen twice, on throwaway data and configuration roots, and sends one
STUN request per run, so it is a check to run by hand rather than a CI gate.

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

Two packages come out of that one file. `omaweb-git` is what `makepkg -si` builds from a checkout,
and it keeps the `-git` name because that is what pacman reads as a package to be rebuilt from
source. `omaweb` is the binary package the pacman repository serves, and

```sh
scripts/make_release_pkgbuild.sh --version 0.5.0 --output <dir>
```

writes it. The two differ in four lines: the name, a version fixed by the tag instead of computed
from a checkout the reader of a binary package does not have, a `conflicts` on each other, and a
source naming the tag. Everything else — every dependency, the release preset, the inventory, the
window-rule notice — is the same file, so the two cannot drift apart while nothing reports it. The
script checks that each line it rewrites was there to rewrite, so a rename in `packaging/PKGBUILD`
fails the derivation rather than quietly producing a package missing the change.

Qt is a dependency rather than a bundle, apart from the engine. The engine is `omaweb-qtwebengine`,
Omaweb's own build, a package of its own installed under `/usr/lib/omaweb` and published in the same
pacman repository as the browser ([ADR 0049](adr/0049-ship-omawebs-own-engine-build.md), which
supersedes [ADR 0013](adr/0013-preserve-engine-sandboxes-in-every-build.md) on this point). So an
engine security update is Omaweb's to rebuild for, and it reaches a reader as a package update
rather than as a new browser release.

Three things follow, and each is in the file rather than in anyone's memory. The build is pointed at
that prefix with `QT_ADDITIONAL_PACKAGES_PREFIX_PATH`, because `find_package(Qt6 COMPONENTS ...)`
looks for each component beside the `Qt6Config.cmake` it already found and a `CMAKE_PREFIX_PATH`
never reaches a module installed somewhere else. The installed binary's runpath names the engine's
directory as well as its own, or the dynamic linker answers from `/usr/lib` and the browser runs the
distribution's engine with the dependency satisfied on paper. And `scripts/check_package.sh`
resolves the dependency where the repository is configured and assumes it where it is not, saying
which of the two it did, so the check works on an architecture whose engine has not been built yet
without quietly claiming to have installed one.

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

An optional dependency only helps someone reading the package, so the browser says it too.
`InputMethod.cpp` reads the modules the environment names, `QT_IM_MODULES` before `QT_IM_MODULE` as
Qt reads them, against the keys the installed platform input context plugins declare, which it takes
from each plugin's metadata without loading it. Naming a module nothing answers to warns once at
startup and stands a notice in Settings under Keyboard, which is also what marks the settings button
as wanting attention. A desktop that names no input method is not misconfigured and is told nothing.
What the browser cannot answer for is the other half: a plugin that is installed still needs its
daemon running, and only typing into a page proves that (#104).

```sh
scripts/check_package.sh
```

builds the package and checks what it carries: the binary, the library, the desktop entry, the icon
and the licences, and nothing outside those directories. Run as root in a container it goes on to
install, upgrade over itself and remove, checking that a file in the reader's configuration and the
rest of the system come through untouched. It refuses to install on a host that is not disposable,
because that would be putting a package on the machine of whoever ran a check.

### Releases

A release is cut by pushing a tag, and the tag is what everything else takes its version from. The
release workflow builds the Arch package in the same container CI checks it in, generates the
inventory, and attaches both to the release beside the notes.

```sh
scripts/generate_sbom.py --output omaweb-sbom.json
```

writes a CycloneDX inventory of what a distributed build contains: the Rust dependency graph the
content blocker links, read through `cargo metadata` because a lockfile records versions but never
licences; the two vendored web-asset directories, pinned to the upstream commit their
`MANIFEST.json` names, which is what a reader needs to fetch corresponding source for the
GPL-licensed one; and the icon font, identified by the hash of the file being built rather than by a
version, because upstream publishes it from a branch.

The web engine is deliberately not in it. The package depends on `omaweb-qtwebengine`, which is
Omaweb's own patched engine and its own package (ADR 0049), and that package carries its own
notices: the LGPL-3.0 text, Chromium's notice from the tree it was built from, and a
`MODIFICATIONS.md` saying what changed and where the corresponding source is. A second answer from
Omaweb could only disagree with that one. What the inventory records instead is the approved engine
baseline, which is Omaweb's own claim about the engine it is supported on. Filter lists are out for
the same reason: they are fetched on a first run rather than shipped.

A build that bundled its engine would need all of that, and `THIRD_PARTY_NOTICES.md` says so.
[ADR 0013](adr/0013-preserve-engine-sandboxes-in-every-build.md) defers AppImage and Flatpak until
Omaweb can maintain bundled engine security updates.

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
missing family costs a font-alias sweep at startup and then draws in whatever face Qt substitutes. A
candidate may be a fontconfig alias rather than a family, and the palette then carries the family
the alias stands for. `monospace` is the alias that matters: `omarchy font set` writes the reader's
choice there rather than into any theme, so it is what the Omarchy template names first and how
Omaweb follows the desktop's font. `size` is the root the whole type scale grows from: every size
the interface asks for is derived from it by the Omarchy kit, which `ThemeController` drives. See
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
Commit subjects since the previous tag with `scripts/release_notes.sh`, and publishes them beside
the Arch packages and their inventory. Every `v0.*` tag is marked a prerelease. macOS bundles are
development artifacts and are not attached ([ADR 0029](adr/0029-distribute-only-for-linux.md)). See
[ADR 0028](adr/0028-derive-the-version-from-the-release-tag.md).

### Both architectures

A release carries an `x86_64` package and an `aarch64` one, each built on a machine of its own
architecture: `x86_64` in the official Arch container, `aarch64` on an Arm runner in a community
Arch Linux ARM image pinned by digest. What that image costs and what it buys is
[ADR 0044](adr/0044-build-the-aarch64-package-on-arch-linux-arm.md). The inventory is generated once
for the release rather than per package, because what it records is written down in this repository
and does not depend on the machine that built anything.

The two legs are the same steps with a different runner, and the `aarch64` one runs nowhere else, so
the workflow can be run by hand from a branch:

```sh
gh workflow run Release --ref <branch>
```

That builds both packages, attaches them to the run, and stops: no release, no repository, no tag.
It is how a change to the packaging path is exercised before it is exercised by a release. GitHub
accepts a dispatch only for a workflow that declares the trigger on the default branch, so a change
to `release.yml` itself is dispatched from `main` once it has merged; `--ref` then chooses which
branch is built.

A dispatched run has no tag of its own, so `scripts/release_version.sh` answers what is being built:
the tag on a release, and the nearest release tag plus the commit being built on a dispatch. Every
job asks it rather than working it out, so the packages and the inventory of one run cannot end up
named differently.

### The pacman repository

A release is also an upgrade. The workflow publishes the `omaweb` package to a pacman repository on
the `gh-pages` branch, so a reader who has Omaweb gets the next version from their own `pacman -Syu`
rather than from noticing that one was released. What was decided and why is
[ADR 0043](adr/0043-serve-upgrades-from-a-signed-pacman-repository.md); what follows is how to run
it.

```sh
scripts/publish_repo.sh --package <file.pkg.tar.zst> --repo-dir <dir> --key <signing key>
```

signs the package, writes the signed `omaweb.db` and `omaweb.files` databases with `repo-add`, and
removes the package the replaced database entry named. The repository holds one version of each
package per architecture: the GitHub releases are this project's archive, and serving history would
make the branch a second one. Two packages are served, the browser and the engine, on release
schedules of their own, so superseded means an older version of the same package, read from the
package's own `.PKGINFO` rather than off the front of its filename. `omaweb-qtwebengine` carries
dashes in its name and the filename is `pkgname-pkgver-pkgrel-arch`, which is the case a filename
comparison gets wrong. Removing the other package instead would leave the database naming a file
that is gone.

Packages sit under a directory named for their architecture, which is the `$arch` a reader's
`Server` line resolves, so the script is called once per package and says nothing about which
architectures there are. Each release replaces the branch with a single commit, because a branch
that kept every package it ever served would grow by the size of a browser each time.

`repo-add` links `omaweb.db` to `omaweb.db.tar.gz`, and a static host serves files rather than
following links, so the short names are written as copies. Without that the repository answers 404
for the only name a client asks for.

```sh
scripts/check_repo_publish.sh
```

runs that whole path against a throwaway key and a scratch directory: it derives the binary
PKGBUILD, signs and publishes two versions of a browser stand-in and one engine stand-in, checks
that the second browser replaced the first and left the engine where it was, and, as root, installs
from the repository with a pacman that trusts nothing but the signing key. The browser stand-in
declares the engine as a dependency, so what that install proves is the reader's own path: one
`pacman -S omaweb` brings both packages out of one repository. The CI job `pacman-repo` runs it on
every change. The reason it exists is that the first real run of a publishing path is otherwise a
release, and by then the release is already out.

The engine's own package is not published by a workflow. `scripts/package-locally.sh` in the patch
repository builds it from an engine tarball in the container the engine was built in, and the
detached signature is made on the machine the key is on, which is never a build machine (ADR 0049).
`publish_repo.sh` then puts it in the repository the same way a browser package goes in.

The signing key is the one piece of setup a human does. It is a signing subkey whose private half is
the `PACMAN_SIGNING_KEY` secret, with `PACMAN_SIGNING_KEY_PASSPHRASE` beside it when the export has
one. Until that secret exists the `publish-repo` job says so in the job summary and ends green: the
release publishes, and readers install it by hand.

A release installs from that repository as well as publishing to it: the `package` job adds the
`[omaweb]` block to the container's `pacman.conf` and installs `omaweb-qtwebengine`, because the
release has to be compiled against the engine it will run on. The public half of the signing key is
`security/repo-signing-key.asc` in this repository rather than fetched from a keyserver, so a
keyserver that does not answer cannot fail a release for a reason that has nothing to do with the
release. The job holds the key file's fingerprint against the published one before trusting it.

The README and the website both carry the `[omaweb]` block and the signing key's fingerprint,
because a reader installs from whichever of the two they opened.
`scripts/check_repository_instructions.py` checks that everything naming the repository names the
same one, and CI runs it: each place is correct on its own, so nothing else would report a
fingerprint that had gone stale in one of them, and the reader would find out as a signature error
on their own machine. The release workflow is the third place, since it installs the engine from
that repository, and `security/repo-signing-key.asc` is the fourth and the only one that is not a
copy. The others state a fingerprint; the key file has one. The check computes it from the key
packet rather than asking gpg, so it runs the same way wherever it runs, and
`tests/scripts/tst_repository_instructions.py` covers that computation against the published value.

The pages landed with the tag that first served the repository rather than before it: instructions
for a repository that answers nothing are worse than none. The fingerprint is published in the
README rather than taken from the keyserver, because a keyserver will hand a reader any key that
claims the name; naming the one key it must be is what makes `pacman-key --recv-keys` safe.

`scripts/rewrite_release_notes.py` then rewrites that commit list into notes addressed to a reader,
from the commit bodies in the range, the issues they reference, and the glossary in
[CONTEXT.md](../CONTEXT.md), which is what keeps the notes calling things what the project calls
them rather than what a commit subject happened to call them. It runs on every tag, prerelease
included. The rewrite carries the compare URL over itself, and repairs the layout. It has nothing to
say about the notes' markup, because `website/build/render.mjs` parses CommonMark: what a release
may write is a question of what CommonMark is rather than of what the release page has a rule for.

`repair_layout` makes every release page the same shape. It drops a title restating the version,
which the page's own `h1` already carries; drops a heading with nothing under it; and shifts a body
opening at `#` down to `##`, keeping the difference between its levels. Each is a thing a body
cannot be right about, and each is repaired rather than refused: refusing publishes the generated
commit list, which is a worse page than a heading in the wrong place.

What the sections are called is asked for in the prompt instead, because naming them is the
judgement the model is there to make. Only the skeleton is fixed: `## Breaking changes` first when
the range has one, `## Fixes` last, and the sections between them named for what actually changed.
`## Linux platform integration` tells a reader more than `## What's new` does.

It needs an `ANTHROPIC_API_KEY` secret. Without one, or when the API refuses, does not answer, or
returns an answer that ran out of tokens, the generated commit list publishes unchanged and the job
summary says the rewrite did not happen. The model thinks before it answers and that thinking is
spent from the same token budget as the notes, so the budget is sized for both; v0.4.0 published
unrewritten because it was not. The summary carries both the generated list and what was published,
which is how a release is checked after the fact. Nothing about the rewrite can block a release.

The rewritten notes reach the website through the release body: the release pages are generated from
the GitHub releases API at deploy time, so they render whatever the release published.

A release that published before the rewrite worked keeps the commit list it was published with, and
the release workflow cannot repair it: that workflow only ever sees the tag being cut. The
`Rewrite published release notes` workflow rewrites the body of releases that are already out. Run
it from the Actions tab with `all` or a space-separated list of tags, and `dry_run` to see the notes
in the job summary without touching anything. A release whose rewrite fails keeps the body it has
and the job ends red naming it, so a partial run says what it left behind. It asks Vercel for a
deploy afterwards, because the release pages only change on one.

The rewrite reads issue bodies, which anyone with a GitHub account can write, and the release body
it produces publishes without review. Wording from an issue can therefore reach a release note and
the release page on the website. What an issue cannot do is change the markup: the compare URL is
carried over rather than generated, and `website/build/render.mjs` decides what a body may become:
markup embedded in it is escaped back into the text it reads as, an image becomes the link that
reaches it, and a link the browser would not follow is nothing but its own label.

`cmake --preset dev` prints the version it derived. A tree with no tags falls back to
`OMAWEB_FALLBACK_VERSION` in `cmake/OmawebVersion.cmake`.

Publishing a release also changes the website, whose releases section and per-release pages are
generated from the GitHub releases API at deploy time. Vercel builds on a push to a branch and this
ref is a tag, so the `Deploy the website` step asks Vercel for a build itself. It needs a
`VERCEL_DEPLOY_HOOK_URL` secret holding a Deploy Hook from the Vercel project's Git settings. With
no secret the step says so in the job summary and the release still publishes; the site then shows
the previous release until the next push to `main`.

## Website

`website/` is the deployed site. Vercel builds it with the `buildCommand` in `website/vercel.json`,
which runs `website/build/site.mjs` from `website/` and serves the `dist/` it writes.

```sh
npm ci --prefix website             # the Markdown parser the renderer imports
scripts/serve_website.sh            # the committed sources, on localhost:8000
cd website && node build/site.mjs   # write dist/, the site as it deploys
node --test website/build/          # the rendering the build step does
```

The install is once per clone. `website/package.json` pins one dependency, `marked`, which is what
renders a release body; Vercel installs it the same way before running the build command.

The site is four pages a reader navigates between. `index.html` says what Omaweb is, `features/` and
`docs/` are written by hand, and `releases/` is generated.

The build step is the only generated part. It fetches the published releases and writes, from
`website/build/release.template.html`, one page per version at `dist/releases/<tag>/index.html` plus
`dist/releases/index.html` for the newest. Every page carries the whole version list beside the
notes, so master and detail are both markup and a version is an address rather than a pane the
script swaps. It writes only into `dist/`, so a local run leaves the sources alone. Set
`GITHUB_TOKEN` to raise the API rate limit; without one the unauthenticated limit applies and is
shared with everything else building from the same address.

The site offers no packages. Installing is one section on the landing page and the same two commands
whichever release it is, so a release page links to its GitHub release for the assets instead of
repeating the download per version.

A release body is Markdown from somewhere else, so `website/build/render.mjs` parses it with
`marked` and its own renderer decides what the body may become. That is what keeps the generated
pages inside the site's `default-src 'self'` policy: embedded markup is escaped back into text, an
image becomes the link that reaches it, and nothing rendered is a subresource.
`scripts/check_website_csp.py` checks the sources rather than `dist/`, so the check reads the same
files whether or not a build has run.

A failed fetch is not a failed deploy. The build leaves the committed `website/releases/index.html`,
which says where the releases are, and warns on standard error. Force that path with
`GITHUB_TOKEN=nonsense node build/site.mjs`, which makes the API answer 401.

Rebuild the per-theme palettes, screenshots and the favicon with `scripts/build_website_themes.py`
after a chrome change or an upstream theme change.

## Hardware video decode

Omaweb asks Chromium for VA-API decoding by adding `--enable-features=VaapiVideoDecodeLinuxGL` to
the engine command line before the engine starts. The GL spelling of the feature is the one
QtWebEngine reads: Chromium renders offscreen into a texture Qt Quick composites, so the Ozone and
Vulkan paths are not in use. The name is Chromium's and has changed between versions, so check it
against the Chromium in `security/baseline.json` when the engine baseline moves.

The flag Omaweb adds goes through the same audit as one from the environment, so there is one rule
about what a launch may carry rather than one rule per route in
([ADR 0034](adr/0034-audit-the-engine-flags-omaweb-adds-itself.md)).

Chromium reads one `--enable-features` list, and the last one on the command line is the one it
reads. Omaweb's goes last, carrying whatever `QTWEBENGINE_CHROMIUM_FLAGS` already named, so a host
that needed a companion feature to get its driver working keeps hardware decode instead of losing it
to the naming:

```sh
QTWEBENGINE_CHROMIUM_FLAGS=--enable-features=VaapiIgnoreDriverChecks ./build/dev/omaweb
# the engine reads --enable-features=VaapiIgnoreDriverChecks,VaapiVideoDecodeLinuxGL
```

Omaweb adds nothing where the host has already said no:

- `--disable-gpu` or `--disable-accelerated-video-decode` leaves no GPU process to decode in.
- `--disable-features=VaapiVideoDecodeLinuxGL` refuses the feature on its own, without giving up the
  rest of the GPU process.

A host with no working driver needs no configuration. Chromium finds nothing to talk to, decodes in
software, and starts as it did before.

### Confirming it is in use

Install the driver for the GPU, which is what `packaging/PKGBUILD` lists as `optdepends`:
`intel-media-driver` for Intel from Broadwell on, `libva-intel-driver` for Intel before it, `mesa`
for AMD, and `libva-nvidia-driver` for NVIDIA. `libva-utils` supplies `vainfo`, which reports the
driver a host loaded and the profiles it decodes:

```sh
vainfo
```

No driver, or no `VAProfile` line for the codec the page uses, is the answer: that page decodes in
software whatever Omaweb asks for.

With a driver present, the GPU's own counters say whether the decoder is doing the work. Watch the
video engine with `intel_gpu_top`, `radeontop`, or `nvtop` while a page plays, and compare a run
under `QTWEBENGINE_CHROMIUM_FLAGS=--disable-accelerated-video-decode`, which puts the same page back
on the CPU.

Chromium's own logging names the decoder it built. These flags name no feature list, so Omaweb still
adds its own alongside them:

```sh
QTWEBENGINE_CHROMIUM_FLAGS="--enable-logging=stderr --vmodule=*vaapi*=2" ./build/dev/omaweb
```

Take the measurement on hardware. A guest with virtualized graphics cannot answer this question, the
same constraint [cosmetic resource validation](cosmetic-resource-validation.md) records. No
measurement is recorded yet: the change was written on a guest, where the workaround above turns the
GPU process off. Record a before and after here from the first run on hardware.

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

Startup to first window is `omaweb --validate-qml` on the `release` preset against an empty data
root, which loads the shell, creates the window and quits on the first event-loop turn. Measured on
the same macOS machine on 2026-09-12, ten interleaved runs each, before and after the QML was
compiled ahead of time:

- Source QML, first launch with an empty QML disk cache: median 0.668 seconds
- Source QML, later launch reading the disk cache: median 0.564 seconds
- Compiled QML, first launch: median 0.648 seconds
- Compiled QML, later launch: median 0.596 seconds

The run-to-run spread is about 0.1 seconds, so on this machine the difference is inside the noise:
the shell's sixty-odd files parse in a few tens of milliseconds, and the launch is engine and
session start-up. What compiling buys is the first launch no longer depending on the disk cache,
which the count of cache files written shows: 58 before, 1 after. No figure from the packaged
browser on Linux hardware has been taken yet.

### Runtime probes

Eight runtime numbers are measurements the test suites keep rather than budgets. Each probe prints a
`probe <name>: <value> <unit> (threshold <limit> <unit>)` line and fails naming both numbers when
the value crosses the threshold. Each threshold is a round number set after the first measurement:
about four times it for the two startup probes, seven and ten times for the tab switch and the
frame, which are under ten milliseconds, where scheduling jitter is a larger share of a sample than
the machine is, and about twice for memory. The three session writes hold the 2 ms that #214 set as
the point to take them off the interface thread, which is the bound they were moved for rather than
a margin over their first measurement. All eight run under `ctest --preset ci`.

Baseline measured on the initial macOS development machine, an Apple M2 Max on macOS 26.6.2, with
the `dev` preset on 2026-09-12:

| Probe                                             | Measured       | Threshold | Test                               |
| ------------------------------------------------- | -------------- | --------- | ---------------------------------- |
| Startup to first window drawn                     | 470 to 550 ms  | 2200 ms   | `omaweb-startup-probes`            |
| Session restore to the visible Space's page drawn | 560 to 620 ms  | 2500 ms   | `omaweb-startup-probes`            |
| Tab switch to the destination page's frame        | 7 ms           | 50 ms     | `omaweb-ui`, `tst_performance.qml` |
| Chromeless frame time over an animated page       | 0.8 to 1.0 ms  | 10 ms     | `omaweb-ui`, `tst_performance.qml` |
| Resident memory per frozen tab                    | 103 to 104 MiB | 200 MiB   | `omaweb-qt-engine-contract`        |
| Visit record, on the interface thread             | 2 to 15 us     | 2000 us   | `omaweb-session-store`             |
| Coalesced tab write, on the interface thread      | 1 to 2 us      | 2000 us   | `omaweb-session-store`             |
| Closed-tab write, on the interface thread         | 1 to 2 us      | 2000 us   | `omaweb-session-store`             |

What each one measures:

- Startup is the UI lab, `omaweb-qml-smoke`, launched on an empty data root with `--report-startup`,
  from `main` to the first frame the window swapped. The median of three launches is kept, so the
  number is a warm start. The lab is the browser without an engine, so this is Omaweb's own startup
  rather than Chromium's, and it is a different number from the `--validate-qml` launch above: that
  one is the packaged browser quitting on its first event-loop turn, this one is the `dev` preset
  loading source QML and drawing.
- Session restore is the same launch against a data root seeded with one Space holding 100 open
  tabs, five of them Pinned, its closed-tab stack at the bound of 25, and history at the retained
  bound of 5,000 visits, from `main` to the first frame with the active tab's page in it. The
  open-tab count has no bound of its own; 100 stands for a working day.
- Tab switch is the median of ten `next-tab` commands between two pages whose engines already exist,
  from the command to the first swapped frame showing the destination.
- Frame time is the scene graph's own cost per frame, from `beforeFrameBegin` to `afterFrameEnd`,
  averaged over one second with the sidebar hidden, the navigation strip floating and the page
  redrawing every frame. The probe then takes the same second with the strip turned off, so the
  strip's own cost, its live copy of the page, blur and mask, is the difference between the two
  lines it prints. The tests draw through the software rasteriser on the offscreen platform, so the
  absolute value is meaningful only on real GPU hardware and a virtual machine's number is
  comparative only: it holds the chrome to what it cost before on the same machine. The software
  rasteriser draws no shader effect, so it cannot price the strip at all. On the same machine
  through Metal, `QT_QPA_PLATFORM=cocoa`, a tab switch costs 14 to 18 ms, the difference being a
  wait for the display.

- Resident memory is read from the operating system through `ProcessResources`, the way the
  retained-tab report reads it, for four pages served over HTTP in one shared profile, as a Space's
  tabs are, hidden and frozen, summed over their distinct renderer processes and divided by the tab
  count.
- The three session writes are what the interface thread pays to hand each one to the store thread,
  against a Space at the same bounds the restore probe uses: the longest of 257 visits, so the one
  that restores the history bound is among them, and the longest of twenty writes of 100 tabs and of
  25 closed tabs. The probe first takes the same writes on the calling thread, which is what they
  cost before the store thread existed, and prints those as `<name>-on-the-calling-thread` without
  holding them. On the M2 Max on 2026-09-13 that control put the trimming visit at 2.1 to 2.8 ms,
  the tab write at 1.3 to 1.6 ms with the odd checkpoint at 5.6 ms, and the closed-tab write at 0.2
  to 0.3 ms.

Re-run the probes on their own with:

```sh
ctest --preset dev -R omaweb-startup-probes -V
build/dev/omaweb-ui-tests -input tests/ui/tst_performance.qml
build/dev/omaweb-qt-engine-contract-tests qtKeepsAFrozenTabInsideItsMemoryBudget
build/dev/omaweb-session-store-tests aThreadedStoreTakesTheSessionsRunningWritesInsideTheirBudget
```

Set `QT_QPA_PLATFORM=offscreen` for the middle two, or `QT_QPA_PLATFORM=cocoa QSG_RHI_PROFILE=1` to
draw the frame probe through Metal and read what the frames cost the GPU. The Linux numbers are
still to be taken: re-run on Linux hardware when it is available and record them here beside the
macOS ones.

### The runtime budget

The probes above measure the browser's parts on the offscreen platform, most of them without an
engine. `scripts/benchmark_runtime.py` measures the assembled browser on a Wayland session, against
the ceilings in [`performance/budget.json`](../performance/budget.json), and exits non-zero when one
is crossed. It prints every measurement beside its ceiling whether or not it crossed one, because a
budget that speaks only when it breaks hides the drift that is about to break it.

```sh
scripts/benchmark_runtime.py
scripts/benchmark_runtime.py startup --browser build/dev/omaweb
scripts/benchmark_runtime.py spaces --spaces 4
```

Four measurements, one subcommand each, so a developer can run the one they are working on:

- `startup` is the median of three launches, from the process starting to the first buffer the
  browser attached to its toplevel's surface.
- `memory` is the proportional set size of the whole process tree, with one Space and one page.
- `spaces` opens Spaces one at a time, each with the same page loaded, and reports what each one
  after the first added. That is the price of the engine profile per Space that
  [ADR 0008](adr/0008-isolate-space-storage-on-disk.md) buys.
- `freezing` loads a page that takes a megabyte every fifth of a second into a second Space, puts
  that Space away, and reports how much the process tree grew afterwards.
  [ADR 0033](adr/0033-stop-an-away-spaces-pages-instead-of-taking-them.md) keeps a frozen page's
  document and process and stops its timers, animations and script, so what the memory it holds buys
  is not in question; a page still running in a Space nobody is reading is. The growth while that
  Space was on show is printed beside it as the control, and a page that did not grow there fails
  the run rather than passing it, because a flat line means nothing without one.

It writes nothing outside the throwaway directories it launches its own browser on, `--record`
aside, so unlike the theme repaint and the default browser it needs no opt-in guard. It does take
the keyboard focus while it runs. Off a Wayland display, without a built browser, or with no way to
synthesise a key, it says it skipped and succeeds. A browser that fails to map a window, Spaces that
never open and an allocator page that never allocates are not skips: each of those fails the run,
because each is either a broken browser or a number that would mean nothing.

CI runs it inside the `arch-linux` job, against the build that job has already made, under a sway on
the headless backend. What it measures there is what needs no hardware. Time to first paint and
scrolling are not in it: the container has no GPU, so a paint timing taken there would be a software
rasteriser's rather than a reader's.

The window mapping is read from the browser's own Wayland protocol log rather than from a
compositor, because the compositor CI runs is not the one a reader runs and the protocol is the same
under both. Memory is the process tree's, because QtWebEngine runs its renderers as children and a
number that omits them measures nothing that matters, and it is proportional set size rather than
resident, because those processes share a great deal and adding their resident sizes counts every
shared page once per process.

Keys reach the browser through `hl.dsp.send_shortcut` where there is a Hyprland, which aims them at
the window under test, and through `wtype` otherwise, which aims them wherever the focus is. A live
desktop therefore keeps its own keystrokes, and the headless compositor CI runs has one window and
nowhere else to put them.

Each Space it opens is counted on disk before any memory is read. Keys sent to a window that was not
ready for them land somewhere harmless and silently, and a run that reported four Spaces having
opened one looks exactly like a measurement.

The ceilings are ceilings with headroom rather than best-recorded times, because a budget that fails
on noise is a budget that gets turned off. `performance/budget.json` records what each was last
measured at and the machine class it was measured on; `--record` writes the measurements back
without touching the ceilings, because what counts as too slow is a decision to be reviewed rather
than a number a slow machine can move.

### The floating strip's cost

Measured under Metal with `QT_QPA_PLATFORM=cocoa QSG_RHI_PROFILE=1`, which turns on the GPU
timestamps the probe reads beside its CPU bracket, on the same M2 Max on 2026-09-13. The probe takes
one second of each state; these ranges are from eighteen of each, interleaved, over repeated runs:

| State         | Frames a second | CPU per frame | GPU per frame  |
| ------------- | --------------- | ------------- | -------------- |
| Strip present | 60 to 62        | 0.9 to 1.5 ms | 0.4 to 0.9 ms  |
| Strip absent  | 59 to 62        | 0.5 to 0.8 ms | 0.2 to 0.65 ms |

The strip adds about 0.4 ms on the CPU and 0.2 ms on the GPU to a frame and drops none, well under
the 2 ms that #213 set as the point to change how it is drawn, so it is drawn as it was. The CPU
bracket includes the wait for a display drawable when the render thread gets ahead of the display:
some seconds have every frame cost 8 or 16 ms with the GPU still under a millisecond, in either
state, and a mean from such a second says nothing about the chrome. The probe's threshold is set for
the offscreen platform CI runs it on, where there is no display to wait for. The scene graph's own
stage timings, `QSG_RENDER_TIMING=1`, which is what the QML Profiler shows, count whole milliseconds
and put sync, render and swap at 0 in both states, so the fraction the strip costs is below what
they can resolve; the GPU timestamps are what priced it. The Linux number is still to be taken.
