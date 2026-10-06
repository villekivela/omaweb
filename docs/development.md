# Development

## Requirements

- A current Linux development environment, or macOS 13 or newer with Xcode 15 or newer
- Qt 6.11 at or above the repository's approved patch, including Qt Quick, Qt SQL, Qt WebEngine, and
  Qt Shader Tools, which compiles the Start page's CRT glass at build time
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
`--show site` opens Site information, as a click on the lock does, and `--show site:certificate`,
`site:blocked`, `site:cookies` or `site:third-parties` opens it at that detail. `--site-page http`,
`cert` or `start` picks the page it is about, and `--site-sample` names what the lab has no engine
for: refused requests, decided permissions, third parties and the site's cookies.
`--tabs --show permission` has the last seeded tab's page ask for notifications, so the question bar
stands over it; add `--private` to read the Private window's wording. `--show prompt` has the same
page ask a JavaScript confirm, so the prompt bar stands over it. `--tabs --show ask` stands the
question `:ask` asks while Allow agents is off over that page. `--agent-activity` seeds the activity
log with two Agents' last hour in two Spaces, and `--show agent-activity` opens the Agent activity
page on it, as its command does; `--agent-filter <name>` shows one Agent's lines. Pass `--tabs` to
seed the Space with a day's worth of tabs, some of them pinned: the lab otherwise comes up on a
Space at rest, which draws neither the Pinned section nor the tab list, so the sidebar is the one
part of the chrome a capture cannot reach. The blank tab stays the one on show, so the viewport
still draws the Start page, unless `--browse` is passed too: then a documentation tab is on show and
the lab's stand-in view draws a sample page in the page palette where it would otherwise say no
engine is running. `--spaces` seeds a Work Space with pages of its own beside Personal, before the
interface loads, so a Space switch has somewhere to go. `--sample-lists` writes EasyList and
EasyPrivacy into the lab's content-blocking settings as current, so Content Blocking shows the lists
a first run has without fetching them. `--keyring <state>` has the lab's keyring in memory stand
where Payment cards is to be reviewed, with `--show settings:payment-cards`: `unavailable`,
`unreachable`, `locked`, `failed`, or `reading`, which answers after 20 seconds and holds the lab
open that long when it closes. `--agents` has an Agent at work under the real Agent rules: an Agent
Space it made is on show with the tab it is driving, which it has just clicked in, and a second
Agent Space sits unused. `--agents-away` keeps the reader's Space on show instead, and
`--agents-window` has the Agent's page open an Auxiliary window and captures that window.
`--agents-grant` has the Agent ask for the reader's page on show, so the grant prompt stands over
it. `--many-spaces` seeds two more of the reader's Spaces, and with `--agents` six more Agent
Spaces, so the footer counts the ones it has no room for; `--narrow` puts the sidebar at its minimum
width, `--sidebar-right` stands it against the window's right edge, and `--space-overflow` opens the
menu of the Spaces left out. `--agents-taken-over` has the reader take the Agent's Space over while
the Agent is still attached, so a Space of the reader's wears the Agent mark in its own colour, and
has the Agent at work in a second Agent Space of its own. Pass `--capture <path>` to render one
frame to a PNG and exit, `--capture-delay <ms>` after 700 ms by default, which works headlessly with
`QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` for reviewing chrome changes without a desktop
session. The chrome's movements are reviewed the same way: `--show space-step`, `tab-step`,
`omnibar-step` and `settings-step` run the switch or the opening shortly before the capture, so the
frame lands part way through it, and the `-settled` spelling of each runs it early enough to land at
rest. The leaving list's picture and the Omnibar's growth need the hardware renderer, so those two
captures go without `QT_QUICK_BACKEND=software`. Development presets load QML, themes, and the icon
font directly from the source tree. Editing those files requires an application restart but no
compile or relink.

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

`.github/workflows/ci.yml` runs on a pull request and on a push to `main`. Each job has a runner of
its own, and they run side by side:

| Job                  | What it is for                                                                                             |
| -------------------- | ---------------------------------------------------------------------------------------------------------- |
| `style`              | The formatters, `qmllint`, the website's own policy check and the website's tests                          |
| `commit-messages`    | Every non-merge subject in the range                                                                       |
| `changes`            | Whether the range holds anything the Arch jobs below could break                                           |
| `arch-linux-clang`   | The `ci` preset under clang against Omaweb's engine, and `ctest --preset ci`                               |
| `arch-linux-budget`  | The browser from the `ci` preset under clang, held to [the runtime budget](#the-runtime-budget)            |
| `arch-linux-release` | The `release` preset under clang against Omaweb's engine, and the tests that load its compiled QML         |
| `arch-linux`         | The required check: passes when `changes` and the three above passed or were skipped                       |
| `arch-linux-gcc`     | The `ci` preset under GCC against Arch's `qt6-webengine`, and `ctest --preset ci`                          |
| `arch-linux-cli`     | The `cli` preset with only `qt6-base` installed                                                            |
| `arch-package`       | `scripts/check_package.sh`: the Arch packages built, installed, upgraded from the last release and removed |
| `pacman-repo`        | `scripts/check_repo_publish.sh`: a release published to a scratch repository and installed from it         |

The clang build's tests, the runtime budget and the release build are jobs of their own rather than
one job's steps, so a pull request waits for the slowest of them rather than for their sum.
`arch-linux` is the one check branch protection requires: it runs after the three whatever they did,
cancelled included, and `scripts/check_job_results.py` reads their results. It does not leave the
verdict to GitHub, because a job whose dependency failed is skipped rather than failed, and a
skipped required check lets a pull request merge.

The two jobs that run the suite run `ctest --parallel "$(nproc)"`. A test that measures time is
`RUN_SERIAL`, so ctest starts nothing beside it: the startup probes, the session store's threaded
writes, and the frame-interval probes of `tests/ui-performance/tst_performance.qml`, which run as
`omaweb-ui-performance` and `omaweb-ui-themed-performance` rather than inside the UI suites.
`omaweb-probes-run-alone` holds that list. A process of their own starts the frame-interval probes
within the content blocker's first five seconds, when it checks its lists and swaps in what it
fetched, so they run that check themselves and wait for it to end.

The runtime budget has its job's runner to itself. Run the suite the same way locally with
`ctest --preset ci --parallel <n>`, a share of the machine's processors on a machine others build on
too.

The UI suites run offline. Their content blocker subscribes to the fixtures in
`tests/ui/filter-lists` (`easylist.txt` and `easyprivacy.txt`, a few rules each) instead of
easylist.to, and `tests/ui/tst_contentblockerlists.qml` checks that the check fetches, compiles and
swaps them in. `arch-linux-clang` refuses the build user every connection but loopback while it runs
`ctest`, and proves the refusal first, so a test that reached for the network fails there. To try it
locally, run
`unshare -rn sh -c 'ip link set lo up && ctest --preset ui -R "^omaweb-ui(-themed)?$"'`.

The clang and GCC jobs each restore a ccache directory from the last run and save it once they have
built, under a key per compiler and preset, capped at 500 MB. A pull request starts from the cache
of its own last run, or from `main`'s on its first. ccache checks each object against its compiler
and sources, so a stale cache costs time and nothing else. A new toolchain misses, and a missing
cache builds cold, slower but the same. `arch-linux-budget` reads `arch-linux-clang`'s cache and
saves none, because it compiles the same objects. `arch-package` builds through `makepkg` without
the launcher, as a reader's build does.

The Arch jobs cost between one and fifteen minutes each, and a change confined to `docs/`,
`website/` or Markdown cannot break a compile, so a `changes` job decides whether they run at all.
It prints the files it decided on, and the same question can be asked of any range:

```sh
scripts/source_changed.sh origin/main
```

The test is inverted on purpose. Everything counts as source unless it is prose, so a directory
nobody has thought of yet builds rather than quietly skipping: the cost of forgetting that way is
ten minutes, and the cost of the other way is a break that reaches `main`. Prose beside source is
source, and a range with no base to compare against builds.

It is a gate job rather than a `paths-ignore:` on the workflow, and the two are not interchangeable.
`arch-linux` is a required check on `main`. A job skipped by a job-level `if:` reports as skipped
and the pull request still merges, which is how a prose change passes `arch-linux`; a workflow
skipped by path filtering leaves its required checks pending indefinitely, with no job to re-run.
Path filtering would also take `style` with it, and that has to run on prose.

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
Three commands are not sent, each because sending it would end the run rather than test it: `print`
opens a dialog only a person can answer, `minimize-window` unmaps the window every later check
reads, and `private-window` has a check of its own. `open-file`, whose file chooser only a person
can answer, has no default key, so the sweep never reaches it.

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

`cmake --install` puts the small client at `bin/omaweb`, the browser and the bootstrapped
content-blocking library under `lib/omaweb`, and the desktop entry, icon and licences under `share`.
The client's install component is `cli`, with the Agent skill, and everything else is `browser`, so
each package installs its own half. The library carries no soname, so the build is told as much:
without that, linking it by path records that path as the dependency itself and an installed copy
would look for it in whatever directory happened to build it. The runpath is `$ORIGIN`-relative for
the same reason, because a package chooses its prefix when it installs rather than when it
configures.

The client is what runs `omaweb`, from a terminal and from the desktop entry. Given a verb or `mcp`
it talks to the Agent socket. Given anything else it finds the browser beside itself, as a build
tree has it, or at `lib/omaweb/omaweb-browser` relative to itself, as a package has it, and replaces
itself with it, so `./build/dev/omaweb` starts the browser built beside it. The process keeps its
id, which is what a desktop that launched it watches for a window, and takes the name
`omaweb-browser`. The `cli` preset builds the client alone on Qt's base
([ADR 0051](adr/0051-hand-the-browser-to-an-agent.md)).

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

A development build run beside the installed browser therefore needs a session bus of its own,
`dbus-run-session -- ./build/dev/omaweb`, with `OMAWEB_DATA_ROOT`, `OMAWEB_CONFIG_ROOT` and
`OMAWEB_CONTROL_SOCKET` pointing at a scratch directory. A build on a private bus cannot reach the
desktop's keyring: the bus starts the keyring daemon, which finds the desktop's already running and
exits. Payment cards there say that Omaweb could not reach the keyring. Check them with the
installed browser quit and the build on the desktop's own bus.

The same command runs the Agent verbs against the browser already running. They start nothing of
their own and exit with 0 when the browser answered, 1 when it refused, 2 for a malformed command
and 3 when no browser is running:

```sh
omaweb spaces
omaweb tabs [--space <id|name> | --all | --pick]
omaweb open <address> [--space <id|name> | --tab <id>] [--new]
omaweb close [--tab <id>]
omaweb space <id|name>
omaweb focus [--raise] <tab id|part of an address>
omaweb commands
omaweb run <command> [position]
omaweb space new [name] [--temporary]
omaweb space delete <id|name>
omaweb look [--all]
omaweb read [selector]
omaweb do <step>... [--settle <ms>] [--timeout <ms>]
omaweb shot [--full] [--output <file>]
omaweb eval <expression>
omaweb console [--level error|warning|all] [--since <cursor>]
```

`space` puts a Space on show. A Space named `new` or `delete` is switched to by its id, since those
words start `space new` and `space delete`. `focus` selects a tab by its id or else the first tab
whose address holds the text, looking in the Space on show before the others and switching to the
tab's Space, and leaves the window where it is unless `--raise` is given, which also brings the
window forward. `tabs --all` lists every Space's tabs. `tabs --pick` offers them to Omarchy 4's
`omarchy-menu-select`, then runs `focus --raise` on the one chosen. It exits 0 when the menu is
closed and 1, saying so, where `omarchy-menu-select` is not on the PATH. `commands` lists the
command scope's commands that can run now, one line each as id and title, and `run` runs one in the
ordinary window as the command scope would, exiting 0 when it ran. `select-tab` and `select-space`
take a position, 1 for the first. Every command of `src/ui/BrowserCommands.qml` is public except
`private-window`, since a Private window is never an Agent's, and the four screenshot commands,
which read the page and are `shot`'s. `run` refuses any other by name. These four need nothing
turned on, so a keybind can use them:

```sh
omaweb space Work && omaweb run toggle-sidebar
```

The last six read and drive the connection's current tab, or the one `--tab` names, and need Allow
agents. That tab has to be in an Agent Space, or in a Space the reader granted. The first page
command in one of the reader's other Spaces shows a prompt bar over the page on show,
`An Agent named <name> wants to use Space <Space>`, and waits up to a minute for Allow or Deny; it
exits refused with `denied` or `undecided` otherwise, and a denied connection is not asked again
until the browser restarts. `open --tab` into one of the reader's tabs asks the same way. Settings,
under agents, lists the granted Spaces with Revoke. A step of `do` is one argument, or several
separated by `;`: `click <label>`, `fill <label> <text>`, `press <key>`, `select <label> <option>`,
`scroll <label|up|down|top|bottom>`, `back`, `wait text <text>`, `wait url <address>`,
`dialog accept [text]`, `dialog dismiss` and `upload <label> <file>...`. Quote a text with spaces to
keep its spacing. Labels come from `look`, and `do` prints a fresh `look` after its steps.

An Agent tab's page is the Agent's to answer. A JS dialog it opens never reaches the reader: `look`
prints it, and the page waits for a `dialog` step. A download lands in `Agents/<name>/` under the
downloads location, and `look` and `do` print its path; a High-risk download still waits for the
reader to confirm it. `upload` clicks the label and gives the file chooser it opens the files named,
and only those, and only in an Agent Space: anywhere else an upload is how a page could take the
reader's files. A file chooser the Agent did not ask for is refused. An Auxiliary window the page
opens is the Agent's too: `do` prints `Opened window-1`, `tabs` lists it, and `--tab window-1`
points the page commands at it. `shot` prints the path it wrote, in `shots/` beside the socket;
`--output` names the `.png` file there and takes no directory. `console` prints what the page has
written to its console since its document loaded, one line each as level, source and line, and text,
with the text's own line breaks and tabs written as `\n`, `\r` and `\t`, then `cursor` and a number
to pass as `--since` for only what comes after. `--level warning` keeps warnings and errors, and
`--level error` errors alone.

```sh
omaweb space new Checks
omaweb open http://localhost:3000/signup --space Checks
omaweb look
omaweb do 'fill 1 reader@example.com' 'fill 2 "A Reader"' 'click 5'
```

Each takes `--json` and `--name`, which defaults to the parent process's name, and `--` ends the
options. These words, `look`, `read`, `do`, `shot`, `eval` and `console` among them, are verbs, so
`omaweb open https://…` and `omaweb tabs` are clients of a running browser and exit with 3 when
there is none. A launcher that starts the browser with an address passes the address alone,
`omaweb https://…`, as the desktop entry does. `space new` and `space delete` need Allow agents,
which has no switch in Settings yet; set `"allow-agents": true` in `privacy.json` under the
configuration root. A running browser follows the file, and turning it off there detaches every
connection at once. `space new --temporary` prints the Space's id and keeps running, and the Space
is deleted when the process stops, so start it in the background and stop it when the Agent is done.
Wait for the id before using the Space, and name it with `--space`, so a command that arrives first,
or after the holder has gone, never lands in another Space:

```sh
omaweb space new signup --temporary --name checker > space-id &
holder=$!
until [ -s space-id ]; do sleep 0.1; done
omaweb open http://localhost:3000/signup --space "$(cat space-id)" --name checker
kill "$holder"
```

A connection whose Space has gone is refused rather than sent to the Space on show, until it names
another.

`OMAWEB_CONTROL_SOCKET` moves the socket so a scratch browser can run beside the everyday one. A
socket name longer than 104 bytes on macOS, or 108 on Linux, cannot be opened, and the browser says
so at start.

`omaweb mcp [--name <name>]` serves the same verbs to an Agent as a stdio MCP server, one tool each,
with `space new` and `space delete` as `space_new` and `space_delete`. It holds one connection to
the socket for as long as it runs, so the current tab carries from call to call and a temporary
Space lasts until the Agent stops the server. When no browser answers, the first tool call starts
one and waits up to 30 seconds for its socket. A browser that runs with its socket closed would only
come forward, so one started that never answers is not started again, and later calls say so.
Register it with Claude Code:

```sh
claude mcp add omaweb -- omaweb mcp
```

The tool list costs about 1,400 tokens of schema and the server's instructions 130 more, which every
conversation the server is registered in pays. An Agent that runs shell commands can use the CLI
instead, taught by the skill the `omaweb-cli` package installs under
`/usr/share/omaweb/skills/omaweb`. Link it into the Agent's skills directory:

```sh
ln -s /usr/share/omaweb/skills/omaweb ~/.claude/skills/omaweb
```

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

One build makes two packages: the browser, and the small client with the Agent skill, which needs
only `qt6-base` so a sandbox can install it alone. The browser's package depends on the client's at
the same version. `omaweb-git` and `omaweb-cli-git` are what `makepkg -si` builds from a checkout,
and they keep the `-git` name because that is what pacman reads as a package to be rebuilt from
source. `omaweb` and `omaweb-cli` are the binary packages the pacman repository serves, and

```sh
scripts/make_release_pkgbuild.sh --version 0.5.0 --output <dir>
```

writes their PKGBUILD. The two files differ in a few lines: the names, a version fixed by the tag
instead of computed from a checkout the reader of a binary package does not have, a `conflicts` on
each source twin, and a source naming the tag. Everything else is the same file: every dependency,
the release preset, the inventory and the window-rule notice. So the two cannot drift apart while
nothing reports it. The script checks that each line it rewrites was there to rewrite, so a rename
in `packaging/PKGBUILD` fails the derivation rather than quietly producing a package missing the
change.

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

A machine that also has the distribution's `qt6-webengine` installed builds against the wrong engine
without saying so. `Qt6WebEngineCore_DIR` resolves to `/usr/lib/cmake` unless it is set, and even
set, `-isystem /usr/include/qt6` comes before the prefix's headers, so the engine's own forwarding
headers find the stock ones. CMake then reports `OMAWEB_ENGINE_OFFERS_DNS_ALIASES` false and the
browser builds without CNAME uncloaking
([ADR 0050](adr/0050-uncloak-cname-trackers-in-the-engine.md)). A clean-chroot package build has no
second engine and is not affected. To build against the patched engine on such a machine, set each
`Qt6WebEngine*_DIR` to the prefix's `lib/cmake` and put the prefix's include directory before
`/usr/include/qt6`. Homebrew on macOS has the same shape, under `/opt/homebrew/include`.
`scripts/check_omaweb_engine.sh <build-directory>` reads both from CMake's cache and fails unless
each engine package came from `/usr/lib/omaweb` and the probe found the DNS alias API. CI's clang
jobs install no `qt6-webengine`, and each runs the check after its configure in case a dependency
ever brings one in.

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

builds both packages and checks what each carries: the browser, the library, the desktop entry, the
icon and the licences in one, the client, the skill and its licence in the other, and nothing
outside those directories. Run as root in a container it goes on to install, upgrade over itself and
remove, checking that a file in the reader's configuration and the rest of the system come through
untouched. Then it upgrades the release before this one, fetched from GitHub, to this build packaged
as the release packages, and checks that `/usr/bin/omaweb` moved to the client, that the client
answers, that `omaweb --version` reaches the installed browser and that the desktop entry still
opens addresses through `omaweb`. It refuses to install on a host that is not disposable, because
that would be putting a package on the machine of whoever ran a check.

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

## Translations

User-facing strings are wrapped for translation (see
[code style](agents/code-style.md#user-facing-strings)) and the catalogues live in `translations/`.
After adding or changing a wrapped string, refresh the catalogue, translate the new entries, and
build:

```sh
cmake --build --preset ui --target update_translations
$EDITOR translations/omaweb_fi.ts
cmake --build --preset ui
```

`update_translations` runs `lupdate -locations none -no-obsolete`, so the diff holds only the
strings the change touched. The build compiles `translations/omaweb_<locale>.ts` to
`omaweb_<locale>.qm` beside the binary, and the package installs them under
`share/omaweb/translations`. Omaweb picks the catalogue from `LC_ALL`, `LC_MESSAGES` or `LANG`, and
shows English when there is none. Launch with `LANG=fi_FI.UTF-8 ./build/dev/omaweb` to see it in
Finnish, or pass `--locale fi` to the UI lab. Wording and the Omaweb glossary are in
[the localization guide](localization.md).

When `git merge origin/main` stops on a conflict in `translations/omaweb_fi.ts`, take main's file
with `git checkout --theirs translations/omaweb_fi.ts` (`--theirs` is main in a merge), rerun
`update_translations` so the branch's strings return as unfinished entries, and copy the branch's
translations back in from `git show HEAD:translations/omaweb_fi.ts`. Keep both sides' translations.
Merge main rather than rebasing, so the branch needs no force push. The localization guide has the
full steps.

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
Omaweb meets the kit from three sides: `src/ui/quickshell-shim` registers the `Quickshell`,
`Quickshell.Hyprland` and `Quickshell.Io` types the kit's singletons import, the QML in `src/ui`
adapts components to Omaweb's call sites, keeping Omaweb's property names and its accessibility
annotations, and `src/ui/KitTheme.cpp` drives the kit's `qs.Commons` colour and type singletons from
the theme palette so they follow `ThemeController` rather than an Omarchy theme on disk, and the
kit's `Style.reduceMotion` from `SystemMotion`. The vendor root is on the QML import path in source
builds and lands under `qrc:/qt/qml` in resource builds.

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

The `Release` workflow refuses a tag that is not on `main` or on its series' maintenance branch
([Maintenance branches](agents/commits.md#maintenance-branches)), generates notes from the
Conventional Commit subjects since the previous tag with `scripts/release_notes.sh`, and publishes
them beside the Arch packages and their inventory. Every `v0.*` tag is marked a prerelease. macOS
bundles are development artifacts and are not attached
([ADR 0029](adr/0029-distribute-only-for-linux.md)). See
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

A release is also an upgrade. The workflow publishes the `omaweb` and `omaweb-cli` packages to a
pacman repository, so a reader who has Omaweb gets the next version from their own `pacman -Syu`
rather than from noticing that one was released. What was decided and why is
[ADR 0043](adr/0043-serve-upgrades-from-a-signed-pacman-repository.md); what follows is how to run
it.

The repository is the assets of one GitHub release per architecture, `repo-x86_64` and
`repo-aarch64`. Those tags are places rather than versions: every release writes to them and nothing
renames them, so the download URL is fixed, which is what a `Server` line needs. It began on a
`gh-pages` branch and moved because the engine package is 101 MB and a git push refuses a file over
100 MB.

Three scripts, in the order a publish runs them. `fetch_repo.sh` takes the release's current assets
into a directory, so `repo-add` updates what is published instead of starting from nothing and
dropping the packages already there. `publish_repo.sh` makes that directory correct. `serve_repo.sh`
uploads it back, replacing each asset and deleting any the directory no longer holds, because an
asset the database does not name is a version the repository offers and cannot deliver.

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

The engine has a publishing path of its own, because it has a release schedule of its own. The two
repositories split it along what each one owns. `scripts/package-locally.sh` in the patch repository
builds the package, because the PKGBUILD and the notices are there; the `Publish the engine`
workflow here signs and publishes it, because the signing key and the pacman repository are here.
One file crosses between them, the unsigned package, attached to a release in this repository:

```sh
gh release create engine-6.11.2 --title "Engine 6.11.2" out/*.pkg.tar.zst
gh workflow run "Publish the engine" -f tag=engine-6.11.2
```

The workflow will not serve whatever happens to be attached. It drops any signature it finds, since
it signs what it publishes itself, and it reads each package's `.PKGINFO` and stops unless the name
is the one the tag calls for and the architecture in the file's name is the one it was built for. An
`engine-*` tag calls for `omaweb-qtwebengine`, and a `v*` tag for `omaweb`. The second case is the
recovery path for a browser release whose own `publish-repo` job failed after the release was out:
re-running that job runs the tag's copy of the workflow, so the packages are taken off the release
and published from here instead. It also holds the secret key's fingerprint against
`security/repo-signing-key.asc` before signing, so a rotated secret that the pages have not followed
fails the run rather than publishing a repository nobody can install from.

Signing there rather than on a laptop is a departure from ADR 0049's wording, which says signing
never happens on a build machine. What that rule is about is a _rented_ machine: a host someone else
owns, kept for hours, with the series and the key both on it. GitHub Actions already holds this key
and already signs every browser package with it, so the engine adds no one to the set of things that
can sign. `package-locally.sh` still signs when `OMAWEB_REPO_KEY` names a usable key, which is the
shorter path when there is one.

The signing key is the one piece of setup a human does. It is a signing subkey whose private half is
the `PACMAN_SIGNING_KEY` secret, with `PACMAN_SIGNING_KEY_PASSPHRASE` beside it when the export has
one. Until that secret exists the `publish-repo` job says so in the job summary and ends green: the
release publishes, and readers install it by hand.

A release installs from that repository as well as publishing to it: the `package` job runs
`scripts/trust_omaweb_repository.sh`, which adds the `[omaweb]` block to the container's
`pacman.conf`, and installs `omaweb-qtwebengine`, because the release has to be compiled against the
engine it will run on. CI's clang jobs do the same to test against that engine. The public half of
the signing key is `security/repo-signing-key.asc` in this repository rather than fetched from a
keyserver, so a keyserver that does not answer cannot fail a build for a reason that has nothing to
do with the build. The script holds the key file's fingerprint against the published one before
trusting it.

The README and the website both carry the `[omaweb]` block and the signing key's fingerprint,
because a reader installs from whichever of the two they opened.
`scripts/check_repository_instructions.py` checks that everything naming the repository names the
same one, and CI runs it: each place is correct on its own, so nothing else would report a
fingerprint that had gone stale in one of them, and the reader would find out as a signature error
on their own machine. `scripts/trust_omaweb_repository.sh` is the third place, since the release
workflow and CI install the engine from that repository through it, and
`security/repo-signing-key.asc` is the fourth and the only one that is not a copy. The others state
a fingerprint; the key file has one. The check computes it from the key packet rather than asking
gpg, so it runs the same way wherever it runs, and `tests/scripts/tst_repository_instructions.py`
covers that computation against the published value.

The pages landed with the tag that first served the repository rather than before it: instructions
for a repository that answers nothing are worse than none. The fingerprint is published in the
README rather than taken from the keyserver, because a keyserver will hand a reader any key that
claims the name; naming the one key it must be is what makes `pacman-key --recv-keys` safe.

`scripts/rewrite_release_notes.py` then rewrites that commit list into notes addressed to a reader,
from the commit bodies in the range, the issues they reference, and the glossary in
[GLOSSARY.md](../GLOSSARY.md), which is what keeps the notes calling things what the project calls
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

Publishing a release also publishes the website, whose releases section and per-release pages are
generated from the GitHub releases API at deploy time. The release workflow moves the `website`
branch to the tag with `scripts/publish_website.sh`, and the push is what asks Vercel for the build.
A branch already at the tag gets no push, so the script asks through a Deploy Hook instead, which
needs a `VERCEL_DEPLOY_HOOK_URL` secret holding a hook created for the `website` branch in the
Vercel project's Git settings. With no secret the step says so in the job summary and the release
still publishes.

## Website

`website/` is the deployed site. Vercel builds it with the `buildCommand` in `website/vercel.json`,
which runs `website/build/site.mjs` from `website/` and serves the `dist/` it writes.

The live site is built from the `website` branch, the project's production branch in Vercel, and not
from `main`. A change to `website/` merged to `main` goes live with the next release, so a page
describing a feature can merge with the feature and still not reach readers before they can install
it. Preview deployments are turned off in the Vercel project's settings
(`previewDeploymentsDisabled`), because every branch push used one from the account's quota, so a
pull request gets no preview: `scripts/serve_website.sh` serves the site locally instead. If
previews are turned back on, the `ignoreCommand` in `website/vercel.json` runs
`scripts/website_unchanged.sh`, which builds one only when a push changes what the site serves:
`website/` or a file `SHARED` in `website/build/site.mjs` names. Production always builds. To
publish between releases, for a site-only fix or to roll a bad site back to an earlier release, run
the `Publish the website` workflow with the tag or commit to publish; everything that commit's
`website/` says goes live. Only the workflows move the branch.

```sh
npm ci --prefix website             # the Markdown parser the renderer imports
scripts/serve_website.sh            # build without the network, serve dist/ on localhost:8000
cd website && node build/site.mjs   # write dist/, the site as it deploys
node --test website/build/          # the rendering the build step does
```

The install is once per clone. `website/package.json` pins one dependency, `marked`, which is what
renders a release body; Vercel installs it the same way before running the build command.

The site is static files: `website/index.html`, its stylesheet and scripts, and `website/assets/`.
The build copies them into `dist/` as they are, leaving out `build/`, the package files and every
`README.md`, and writes the release pages beside them. Nothing but the release pages is generated,
so the landing page can be opened from `website/` directly; `scripts/serve_website.sh` builds first
so the release pages are there too, and `--local` skips the network.

The release pages are the generated part. `website/build/releases.mjs` fetches every published
release, the engine builds and the package repository as well as the browser's versions, and writes
one page per release at `dist/releases/<tag>/index.html` plus `dist/releases/index.html` for the
newest browser release. Each page is `index.html` with its `<main>` swapped for
`website/build/release.html`, so the header and the footer have one copy, and the chrome's relative
addresses are rewritten to reach back to the root. Every page carries the whole release list beside
the notes, so a release is an address rather than a pane the script swaps; the list scrolls on its
own however long it grows. Set `GITHUB_TOKEN` to raise the API rate limit; without one the
unauthenticated limit applies and is shared with everything else building from the same address.

The site offers no packages. Installing is one section on the landing page and the same two commands
whichever release it is, so a release page links to its GitHub release for the assets instead of
repeating the download per version. `scripts/check_repository_instructions.py` checks that the
landing page's commands and fingerprint agree with the README, the release workflow and the key in
`security/`.

A release body is Markdown from somewhere else, so `website/build/render.mjs` parses it with
`marked` and its own renderer decides what the body may become. That is what keeps the generated
pages inside the site's `default-src 'self'` policy: embedded markup is escaped back into text, an
image becomes the link that reaches it, and nothing rendered is a subresource.
`scripts/check_website_csp.py` checks the sources rather than `dist/`, so the check reads the same
files whether or not a build has run.

A failed fetch is not a failed deploy. The build writes a releases page that says where the releases
are, and warns on standard error. Force that path with `GITHUB_TOKEN=nonsense node build/site.mjs`,
which makes the API answer 401.

The site once had Features, Docs and Sync pages. `website/vercel.json` redirects their addresses:
Features and Docs to the README, and `/sync`, which is the GitHub App's homepage link, to
[What is synced, and what is not](sync-privacy.md).

Regenerate the interface captures with `scripts/build_website_themes.py` after a chrome change or an
upstream theme change. It writes `website/assets/shots/`: the window alone per theme and state, and
`themes.css`, each theme's colours for the frame the captures stand in.

The introductory film is recorded, not captured from the lab: `scripts/record_film.sh` builds the
real browser in CI's Arch container, plays it through six beats under headless cage, and writes the
film to `build/film/`. It fails rather than encodes when a beat did not happen. The files are kept
as assets of the `film` release rather than in the repository, so it does not grow with each
recording, and the website's build copies them into the site. [`film/README.md`](../film/README.md)
has the beats and the upload command.

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
a margin over their first measurement. All eight run under `ctest --preset ci`, as do the
[frame intervals](#frame-intervals) of the chrome's movements, which are held to a budget instead.

Baseline measured on the initial macOS development machine, an Apple M2 Max on macOS 26.6.2, with
the `dev` preset on 2026-09-12, and on CI's runners on 2026-10-06:

| Probe                                             | M2 Max         | CI, EPYC 7763  | CI, EPYC 9V45 | Threshold | Test                        |
| ------------------------------------------------- | -------------- | -------------- | ------------- | --------- | --------------------------- |
| Startup to first window drawn                     | 470 to 550 ms  | 640 to 660 ms  | 429 ms        | 2200 ms   | `omaweb-startup-probes`     |
| Session restore to the visible Space's page drawn | 560 to 620 ms  | 960 to 980 ms  | 618 ms        | 2500 ms   | `omaweb-startup-probes`     |
| Tab switch to the destination page's frame        | 7 ms           | 5.5 to 6.1 ms  | 5.4 to 5.6 ms | 50 ms     | `omaweb-ui-performance`     |
| Chromeless frame time over an animated page       | 0.8 to 1.0 ms  | 0.4 to 0.6 ms  | 0.3 ms        | 10 ms     | `omaweb-ui-performance`     |
| Resident memory per frozen tab                    | 103 to 104 MiB | 108 to 117 MiB | 108 MiB       | 200 MiB   | `omaweb-qt-engine-contract` |
| Visit record, on the interface thread             | 2 to 15 us     | 10 to 42 us    | 12 us         | 2000 us   | `omaweb-session-store`      |
| Coalesced tab write, on the interface thread      | 1 to 2 us      | 2.9 to 5.7 us  | 2.7 us        | 2000 us   | `omaweb-session-store`      |
| Closed-tab write, on the interface thread         | 1 to 2 us      | 2.0 to 5.0 us  | 2.2 us        | 2000 us   | `omaweb-session-store`      |

The CI columns are the `arch-linux-clang` and `arch-linux-gcc` jobs of `main` at `d4561d3` and
`9a39d1c`, on 2026-10-06 with QtWebEngine 6.11.2-5, read from the step that prints the probes'
numbers from ctest's log. That step prints them on a passing run.

The two commits ran four jobs. Each job names its processor in the step before the build: three ran
on an AMD EPYC 7763, which fills the first CI column, and one, the clang job at `9a39d1c`, on an
EPYC 9V45, which fills the second. The 9V45 starts and restores about a third faster. The clang jobs
measured 108 MiB per frozen tab and the gcc jobs 114 to 117 MiB; the other probes agree between
compilers.

Every CI number is inside its threshold with at least 40% of it to spare. Against the M2 Max,
startup takes 1.2 to 1.4 times as long on the 7763 and the restore 1.5 to 1.8 times. Memory is 5 to
13% higher. The three session writes take two to three times as long, a few microseconds
against 2000. The tab switch and the frame time are lower on CI, and the frame time comes from the
software rasteriser rather than a GPU.

CI runs every probe on `QT_QPA_PLATFORM=offscreen`, so the two frame numbers are comparative there,
as the frame time below explains. The memory probe and the session writes open no window.

The baseline from Omarchy on real hardware, with the GPU drawing, is still to be taken (#577).

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
- The Start page's frame time is the same cost over one second of a resting Space, its road driving:
  the Scene's moving parts placed thirty times a second, its small picture captured, and the CRT
  glass drawn over the window. The window draws up to two frames for each of the road's, because a
  `ShaderEffectSource` asks for one more frame after its source changes. Through the software
  rasteriser on the offscreen platform it was 0.6 ms. Through Metal on an M2 Max, with the window
  forced active since a test window there never is, it was 1.2 ms, 0.3 ms of it on the GPU.

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
build/dev/omaweb-ui-tests -input tests/ui-performance/tst_performance.qml
build/dev/omaweb-qt-engine-contract-tests qtKeepsAFrozenTabInsideItsMemoryBudget
build/dev/omaweb-session-store-tests aThreadedStoreTakesTheSessionsRunningWritesInsideTheirBudget
```

Set `QT_QPA_PLATFORM=offscreen` for the middle two, or `QT_QPA_PLATFORM=cocoa QSG_RHI_PROFILE=1` to
draw the frame probe through Metal and read what the frames cost the GPU. On a Linux desktop,
`QT_QPA_PLATFORM=wayland QSG_RHI_PROFILE=1` draws through its GPU, as the
[frame intervals](#frame-intervals) section's command does.

### Frame intervals

ADR 0007 lets Omaweb's own animation follow the display without holding the interface thread for
more than one frame. Four probes in `tst_performance.qml` hold the chrome's movements to that: the
sidebar leaving and coming back, a Space switch, the Omnibar opening over a Space of a hundred tabs
and filtering them as a word is typed, and a Glance opening over a page and going. Each surface is
opened and closed once unwatched, then five more times with every frame watched, over a page that
redraws every frame, so the last movement is a closing and the next probe starts with nothing open.

A fifth probe watches the Shortcut sheet's openings one at a time, since each is a different one:
the window's first, over a moving page, then five over the Start page and five over the page in
turn, whose commands differ from the Start page's. Each opening's slowest interval, from the frame
before the input to the frame the sheet rests in, counts as a movement, and at most one of the
eleven may go over the ceiling. Single openings on the laptop below took 15 to 24 ms offscreen and
17 to 23 ms on its GPU, the window's first the slowest. On CI's runner, in #597's `arch-linux` and
`arch-linux-gcc` jobs, every opening took 17 to 23 ms and none went over.

What is read is the time from each frame's end to the next one's, not the frame's cost: the cost is
the scene graph's own, and a movement's script and layout run on the interface thread between
frames, where the cost bracket does not see them. A frame that waited on one held frame arrives two
frames after the last, so the ceiling is two frames at the display's refresh rate, 33.3 ms at 60 Hz.
Each probe holds two numbers against it:

- The 95th percentile of every interval watched, by nearest rank, with the slowest printed beside
  it.
- How many of the ten movements had any interval over the ceiling, at most one. A surface that holds
  the interface thread once each time it opens has one held frame in about twenty, which the pooled
  percentile lets through and this count does not. One in ten is allowed for a garbage collection or
  a busy machine.

A movement's intervals run until the next movement begins, so a frame held up by work a movement
leaves for after it comes to rest is that movement's. Each movement is the pointer's, because after
a key the chrome steps rather than eases, and each probe fails unless the surface's own position
changed in at least three frames a movement: the page under it redraws every frame, so frames alone
do not say the surface moved. The Omnibar's word is set into the field a letter at a time rather
than sent as keys, because a compositor need not give the test window the keyboard.

The sidebar, a Space switch and the Omnibar hold the budget. The Glance does not hold it in CI yet.
It prints both budget lines with the reason it is over, and holds a guard instead: the same count of
movements, at most one in ten, with a frame slower than its guard rather than the ceiling. The guard
sits over the slowest movement CI's runner has drawn. It catches a change that puts a frame over the
guard in more than one movement, such as a stall on every opening, and nothing smaller: on CI a
stall on each opening goes unnoticed under about 25 ms. The change that brings the Glance inside
removes its `overBudget` argument, and the budget is held.

- The Glance. Inside the budget on this laptop and on the GPU, but on CI's runner three of its ten
  movements hold a frame, at 36 to 40 ms.

The Omnibar reads the commands when it opens and when the Space changes, and the tabs and the Spaces
then and again after one of them changes, rather than for each keystroke. It ranks them once for
each edit. When History's answer or the engine's arrives, only History's rows, the put-away tabs and
the keywords are ranked again, and they are merged into the edit's ranking, with the engine's
proposals listed after as before. An answer that lists the very rows already shown, as an empty one
does, leaves them standing rather than building them again. Reading and ranking all of them again
for each edit and each answer held two to five of the ten movements over the budget offscreen and
five on CI. Ranking once for each edit alone still held two on CI's `arch-linux` job, at 35 to 42
ms.

What the reader cannot see lays nothing out while the chrome moves. A closed Shortcut sheet lists
nothing and keeps the rows it laid out when the window started or when it last opened; it works its
list out and takes the page area's width as it opens. Closed Settings keeps the width it was last
drawn at. The UI lab's stand-in page follows the page area's width only while it is drawn, since a
real engine's hidden view costs the interface thread little at a new width.

Measured on an AMD Ryzen 7 PRO 7840HS with Radeon 780M graphics, on Omarchy, with the `ci` preset on
2026-10-05. Offscreen is twelve runs, nine of them three at a time. CI is the `arch-linux` and
`arch-linux-gcc` jobs' runs of #588. GPU is three runs in the Hyprland session on the laptop's 60 Hz
display, drawn through radeonsi. The Omnibar's row is #595's: offscreen is eight runs one at a time,
CI is its pull request's `arch-linux` and `arch-linux-gcc` runs, which then printed a probe's
numbers only when it failed, and GPU is three runs. The sidebar and Space switch rows are #594's, on
2026-10-06: offscreen is three runs of the whole `tst_performance.qml`, CI is #597's two jobs at
816e61e, read from the step that prints the probes' numbers from ctest's log, and GPU is three runs:

| Surface      | Offscreen p95 | Held, offscreen | CI p95      | CI slowest  | Held, CI | GPU p95 | Held, GPU | Guard |
| ------------ | ------------- | --------------- | ----------- | ----------- | -------- | ------- | --------- | ----- |
| Sidebar      | 17 to 18 ms   | 0               | 17 to 18 ms | 21 to 23 ms | 0        | 17 ms   | 0         | none  |
| Space switch | 17 to 18 ms   | 0 to 1          | 17 to 19 ms | 24 to 31 ms | 0        | 17 ms   | 0         | none  |
| Omnibar      | 14 to 15 ms   | 0 of 10         | not printed | not printed | 0 to 1   | 17 ms   | 0         | none  |
| Glance       | 18 to 22 ms   | 0 to 1          | 26 to 27 ms | 39 to 40 ms | 3        | 17 ms   | 1         | 67 ms |

On this laptop the Glance's one held movement is the same opening in every run, the fourth, at 32 to
35 ms. On the GPU all four are inside the budget. The software rasteriser draws no blur, so the
Omnibar's glass, the Glance's backdrop and the sidebar's floating shelf are priced only by the GPU
columns. Take those from a terminal in the session, where the test window opens over the desktop for
about fifteen seconds:

```sh
QT_QPA_PLATFORM=wayland QSG_RHI_PROFILE=1 build/ci/omaweb-ui-tests \
    -input tests/ui-performance/tst_performance.qml
```

The chromeless probe fails there, at 16.7 ms against 10 ms, because the CPU bracket includes the
wait for the display, as [the floating strip's cost](#the-floating-strips-cost) describes for Metal.

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
scripts/benchmark_runtime.py pageload
scripts/benchmark_runtime.py livetabs
```

Six measurements, one subcommand each, so a developer can run the one they are working on:

- `startup` is the median of three launches, from the process starting to the first buffer the
  browser attached to its toplevel's surface.
- `memory` is the proportional set size of the whole process tree, with one Space and one page.
- `spaces` opens Spaces one at a time, each with the same page loaded, and reports what each one
  after the first added. That is the price of the engine profile per Space that
  [ADR 0008](adr/0008-isolate-space-storage-on-disk.md) buys.
- `freezing` loads a page that takes a megabyte every fifth of a second into a second Space, puts
  that Space away, and reports how much the engine's processes grew over the ten seconds after they
  had settled. The browser's own process is left out: a page runs in a renderer, and on CI the
  browser process takes a step of about 6 MiB at a moment nothing can predict, more than the whole
  budget. A Space switch leaves the engine busy for a moment, so the probe first reads every two
  seconds until three readings in a row agree within a quarter of a mebibyte, for at most thirty
  seconds. The log says how long that took and how far the reading moved meanwhile.
  [ADR 0033](adr/0033-stop-an-away-spaces-pages-instead-of-taking-them.md) keeps a frozen page's
  document and process and stops its timers, animations and script, so what the memory it holds buys
  is not in question; a page still running in a Space nobody is reading is. The growth while that
  Space was on show is printed beside it as the control, and a page that did not grow there fails
  the run rather than passing it, because a flat line means nothing without one.
- `pageload` is what Content blocking as a whole adds to a page load: the rule check, CNAME
  uncloaking where the engine carries it, the Refusal tally and the refused-request list. From the
  same loads it takes how soon a page first paints. Both are described below.
- `livetabs` opens five Spaces, then 20 tabs and then 50 across them, and reports what each tab past
  a Space's first added at each count and how much of a core the process tree uses over 30 seconds
  left alone. It is described below.

It writes nothing outside the throwaway directories it launches its own browser on, `--record`
aside, so unlike the theme repaint and the default browser it needs no opt-in guard. It does take
the keyboard focus while it runs. Off a Wayland display or without a built browser, it says it
skipped and succeeds. A measurement that needs something the machine lacks, a way to synthesise a
key or `pageload`'s DNS server, says it skipped that one and takes the rest. A browser that fails to
map a window, Spaces or tabs that never open and an allocator page that never allocates are not
skips: each of those fails the run, because each is either a broken browser or a number that would
mean nothing.

CI runs it in the `arch-linux-budget` job, which builds the browser from the `ci` preset and runs
nothing else, under cage on the headless backend, with `--require-dns` so that `pageload` fails
there rather than skips. What it measures there is what needs no hardware, and a page's first paint,
which is held against CI's own software renderer rather than a reader's GPU. Scrolling is not in it.

To run it on a Hyprland desktop without its keys reaching the desktop, run it as the program of a
headless cage, as CI does, with `HYPRLAND_INSTANCE_SIGNATURE` unset so that keys go through `wtype`
to cage:

```sh
env -u HYPRLAND_INSTANCE_SIGNATURE WLR_BACKENDS=headless WLR_RENDERER=pixman \
    WLR_LIBINPUT_NO_DEVICES=1 QT_QPA_PLATFORM=wayland cage -- \
    scripts/benchmark_runtime.py livetabs --browser build/dev/omaweb-browser
```

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
than a number a slow machine can move. The budget keeps only that last recording, so `--record` also
appends the run to [the performance history](#the-performance-history), where a number drifting
towards its ceiling shows. Give the line a machine name with `--machine`; without it, the machine is
described from its hardware.

#### The page-load measurement

`pageload` loads three pages, each with 40 one-pixel images that no rule refuses, so a load with
blocking on fetches everything a load with it off does and the difference is what blocking cost.

- The worst case takes its 40 images from 40 hosts, and every load has hostnames of its own, so
  every load pays its lookups.
- The common case takes them from 4 hosts, the same on every load, after one load in each mode that
  is not counted, so the engine's remembered answers apply.
- The procedural case is the common one with the markup and rules of
  `tests/content-blocking/procedural-rules.json` on the page: one procedural cosmetic rule per
  operator and action the pinned parser reads, written for both page hosts. Blocking on, the view
  loads the matcher and runs the rules and their watch; switched off, the same markup loads without
  them. Its difference less the common case's is what procedural rules cost
  ([ADR 0052](adr/0052-apply-procedural-cosmetic-filters.md)).

Each page is loaded ten times in each mode, the modes alternating so that drift on the machine falls
on both. "Off" is the per-site switch: the page is served from a second host, and blocking is
switched off for that one, which is the comparison a reader makes and the one
[ADR 0050](adr/0050-uncloak-cname-trackers-in-the-engine.md) made. The page times itself, from its
navigation starting to its `load` event. A load that shows fewer than 40 images is not counted,
because the two modes would no longer have loaded the same page. A spare of the same case and mode,
whose hosts are already in the zone, is loaded in its place and the run lists it. Running out of
spares fails the run. What the budget holds is the median with blocking on minus the median with it
off; the two medians are printed beside it.

The engine writes each response body into a 2 MiB data pipe in `/dev/shm`, and the worst case's 40
images can all be in flight at once. Docker gives a container 64 MiB of `/dev/shm`, which a run
outgrows: the engine then cancels the last images of a load as their answers arrive, because no pipe
can be made for them. So `pageload` fails when `/dev/shm` has less than 256 MiB free, and CI's
`arch-linux-budget` job starts its container with `--shm-size=2g`.

The rules are EasyList and EasyPrivacy from `third_party/filter-lists`, snapshots pinned by digest
so a run next month measures the same rules. `ctest` checks them against their manifest
(`omaweb-vendored-filter-lists`), and `scripts/sync_filter_lists.py --sync` fetches them again. A
refresh changes what the budget measures, so it is a change of its own with the numbers re-recorded.

The hosts resolve through a real DNS server, because a host-resolver rule maps a host to an IP
literal and an IP literal makes no lookup, which would hide what uncloaking costs. Each image host
is a CNAME chain of two aliases to one address, served by `dnsmasq` from a zone the script writes.
The run happens in a network namespace of its own, entered without privilege through a user
namespace, where loopback is the only interface and the machine's `resolv.conf` and `nsswitch.conf`
are covered by the run's own. The browser runs in a further user namespace that maps the developer's
own user back, because Chromium's sandbox refuses to run as root, and the namespace's root is who
the rest runs as. Nothing outside the namespace sees the server or the changed files, and they go
when it does. The run needs `dnsmasq`, `ip`, `mount` and `unshare`; without them it skips, and with
`--require-dns` it fails instead.

CI's container will not make a user namespace, so there the job, which is root, serves the zone
itself: `--print-dns-zone` writes it, the job starts `dnsmasq` on it and points the container's
`resolv.conf` at loopback for the measurement. When the machine's own resolver already answers the
run's names, `pageload` measures on the machine's network rather than making one.

Only the default DNS path is budgeted, with Secure DNS off. That path makes two lookups for a host
it has not seen (ADR 0050, "What the engine does") and is the one more likely to regress. With
Secure DNS on the lookups go to a public server, whose speed from GitHub's network is not Omaweb's
to hold to a number, so measure that path by hand when it changes.

CI builds against Omaweb's engine, so the ceilings hold CNAME uncloaking's lookups with the rest of
Content blocking's cost. They were recorded there on 2026-09-28
([#394](https://github.com/villekivela/omaweb/issues/394)). The worst case added 18.2 ms, against
6.1 ms on Arch's `qt6-webengine`, where uncloaking is compiled out.

Each ceiling is four times the difference recorded on CI's runner, and never under 20 ms. The
differences are a few milliseconds, and a shared runner's noise is bigger than that multiplied.

#### The first-paint measurement

`pageload` also holds the fresh-host and known-host pages to how soon they first paint, with
blocking on. It takes no loads of its own: the counted blocking-on loads of those two cases each
report their `first-contentful-paint` entry beside their load time, so the spares, the warm-up loads
and the DNS server are the page-load measurement's. The procedural case is left out, because it is
there to price its rules rather than to be a page a reader opens.

The entry is the page's own, from its performance timeline, on the clock its load time uses: from
the navigation starting to the first frame that drew an image. The pages have no text, so that frame
is the first of the 40 images arriving, decoded and presented. The entry is added when the frame is
presented, which on a slow runner can come after the `load` event, so the page watches for it for up
to five seconds. A counted load with no entry by then fails the run rather than leaving the median
to the loads that had one.

The log prints, for each case, the median of its ten loads and the fastest and slowest of them. Two
CI runs on 2026-10-05 printed:

```text
  first contentful paint, 40 fresh hosts, blocking on: 62.0 ms (median of 10 loads, 44.0 to 96.0 ms)
  first contentful paint, 4 known hosts, blocking on: 46.0 ms (median of 10 loads, 44.0 to 80.0 ms)

  first contentful paint, 40 fresh hosts, blocking on: 94.0 ms (median of 10 loads, 64.0 to 112.0 ms)
  first contentful paint, 4 known hosts, blocking on: 60.0 ms (median of 10 loads, 44.0 to 104.0 ms)
```

What the budget holds is that median. The first contentful paint waits on one image, not on all 40,
so the fresh-host page pays one host's lookups before it and the known-host page none. A rise in
both medians is the engine drawing later. A rise in the fresh-host page's alone is the lookup in
front of the first image, and `pageload_fresh_hosts_milliseconds` is where to look next. A wide
spread with a steady median is the runner, not the browser.

The same browser's fresh-host median moved from 62 to 94 ms between the two runs, so the runner
alone moves it by half. Each ceiling is four times the slower run's median, rounded up to ten
milliseconds, as the startup and page-load ceilings are: 380 ms for the fresh-host page and 240 ms
for the known-host one.

CI has no GPU, so the paint is Chromium's software rasteriser under cage's pixman renderer. The
number is CI's, held against itself to catch a regression, and is not what a reader's GPU takes.

#### The live-tabs measurement

`livetabs` holds what a reader's open tabs cost: the memory each tab adds, at 20 tabs and at 50, and
the CPU a window of 50 uses while nobody touches it. The run opens five Spaces with one tab each
first, then spreads the tabs evenly over them, four each at 20 and ten each at 50, so the two counts
are the same browser with more tabs in each Space rather than one with more Spaces.

A live tab is one with its engine. A tab of a restored Space that was never shown has none, so each
tab is opened as a reader opens one: a Space's first tab is the empty tab it starts with, given an
address with the address key, and every other is the new-tab key and an address typed into the
Omnibar. The browser is launched with no address, because a fresh profile opens an empty tab and the
new-tab key would reuse it rather than add one. The browser makes a tab when its address is entered,
not at the new-tab key, so each new tab is counted in its Space's database then, and keys that made
none are sent again. The run then waits for a window title that names the tab's page and its Space,
and a run whose count at 20 or 50 is wrong fails.

The pages come from the run itself. Each tab is its own site, served from its own loopback address,
`127.0.0.2` to `127.0.0.51`, because the engine gives a renderer to a site, a host without its port,
and tabs on one address would share processes that tabs on different sites do not. Each page is an
article with a stylesheet, a picture, a table, a form and a script that builds an index once, with
its own text. Nothing on them runs after they load: no timer, animation or media. Only Linux routes
the whole of `127.0.0.0/8` to the loopback interface, so the step runs on Linux only.

The memory is the proportional set size of the whole process tree, read once three readings two
seconds apart agree within a quarter of a mebibyte, as the frozen reading is. What a tab costs is
read against the browser with all five Spaces open and one tab in each: the tree at 20 tabs less
that, over 15, and the same over 45 at 50. A Space costs more than a tab, the second most of all, as
`space_mebibytes` shows, so read against a browser with one Space the tabs would carry four Spaces'
cost, and more of it per tab at 20 than at 50. Dividing the whole tree by the tab count would spread
the browser's own cost over the tabs in the same way. Every tab but the one on show is frozen, as a
reader's are, so the number is what a reader's background tabs hold rather than what running pages
take.

The idle CPU is the tree's user and system time over 30 seconds with 50 tabs open, as a share of one
core, read from `/proc` at both ends of the window. Each process's time holds that of the children
it reaped, so a process that ended in the window is counted through its parent, less what it had
used before the window began. The window starts as soon as the memory at 50 has settled, without
waiting for the CPU to fall: work the browser goes on doing after its tabs have opened is what it is
there to catch. The log names each process that used any, busiest first, and any that ended in the
window. The browser writes its Wayland protocol log for the whole run, and that costs it a little
for every frame it presents, so a window that keeps drawing shows here with the logging's cost on
top.

### Against Chromium

`scripts/benchmark_chromium.py` runs Speedometer 3.1, JetStream 2.2 and MotionMark 1.3.2 in Omaweb
and in two Chromiums, and prints Omaweb's score as a share of each:

```sh
scripts/benchmark_chromium.py
scripts/benchmark_chromium.py --suite speedometer --runs 5
scripts/benchmark_chromium.py --omaweb build/dev/omaweb --record
scripts/benchmark_chromium.py --profile
```

The two Chromiums separate the engine's version lag from what Omaweb costs:

- The latest stable Chromium, `chromium` on the path by default, which is what Arch readers have.
  Its gap is the one readers see.
- The Chromium the engine is based on. Its gap is what Omaweb and Qt WebEngine add. The harness
  reads the engine's Chromium version from `omaweb --version`, which reports
  `qWebEngineChromiumVersion()`, and fetches the newest build of that major that the Playwright
  project publishes. Playwright publishes a Chromium build for aarch64 and for x86_64 with every
  release. When an engine update moves the base, the next run follows without a code change. The
  report says so when the engine's version is not the `chromium` in `security/baseline.json`.
  `--matched-chromium` takes a Chromium binary instead of the download.

The suites are pinned to a commit and a SHA-256 digest each and served from loopback. They and the
Chromium builds are cached under `~/.cache/omaweb-benchmarks`, one directory per suite commit and
per Chromium version, so after the first run no network is needed. Everything is fetched over HTTPS
with the certificate verified, and a redirect to plain HTTP is refused.

Playwright publishes no digest for its builds, so the matched Chromium's is pinned on its first
fetch: the archive's SHA-256 goes into `~/.cache/omaweb-benchmarks/chromium/majors.json`, and the
archive is kept. Every later run hashes the kept archive against that digest, and a fetch after
that, for example once the archive has been deleted, is held to it too. A mismatch fails the run.
The browser is extracted afresh from the verified archive every run, so what runs is what was
hashed. The digest is printed in the report and stored in the history line, so a run says which
binary it measured. With `--matched-chromium`, the digest is that executable's.

Each launch gets a fresh profile. Omaweb runs on scratch data and configuration roots, so it reads
no `sync.json` and none of the reader's settings, and its content-blocking lists are seeded from
`third_party/filter-lists` so a first run does not fetch and compile them during a suite. Chromium
gets a scratch `--user-data-dir`, and a scratch `XDG_CONFIG_HOME` so that Arch's launcher reads no
`chromium-flags.conf`, where a reader may have added extensions. Chromium is told to draw on Wayland
with `--ozone-platform=wayland`, as Omaweb does: its own choice follows `XDG_SESSION_TYPE`, which a
session started from a terminal or a container does not set, and it would then exit looking for an X
server. The harness drives Omaweb over `--remote-debugging=<port>` and Chromium over
`--remote-debugging-port`.

Omaweb's GL flags come from `QTWEBENGINE_CHROMIUM_FLAGS`, as they do at every launch, and by default
the harness passes the same GL flags to both Chromiums, so all three browsers composite the same
way. On the Omarchy VM, left to itself, Arch's Chromium composited in software and Playwright's on
the GPU, and the comparison would have measured the compositors. Only the switches that choose how a
browser draws are passed on: `--use-gl`, `--use-angle`, `--use-vulkan`, `--disable-gpu*`,
`--enable-gpu*`, `--ignore-gpu-blocklist`, `--disable-software-rasterizer` and `--enable-zero-copy`.
`--chromium-flags` replaces them for both Chromiums, and `--chromium-flags ""` launches them with
none. The report names each browser's flags and GPU compositing state either way.

Each suite runs `--runs` times, 3 by default, in each browser. The browsers alternate, and the first
browser changes from round to round. The report names each browser's version and flags, the engine
library Omaweb loaded (ours under `/usr/lib/omaweb`, or Arch's `qt6-webengine`) and each run's
score. It gives the Omaweb/Chromium ratio per suite and baseline: the mean of Omaweb's runs over the
mean of Chromium's. The latest Chromium's spread across runs is printed as the check that the host
was idle. A virtual machine's score moves with its host's load, which the guest cannot see: in #356,
memory pressure on the Mac host moved Speedometer between 2.5 and 22.6, and Chromium on an idle host
scored 24.5 in every run.

MotionMark measures the compositing path. A MotionMark run in which a browser composited without a
GPU is marked in the report and in the history as not a result. The harness decides this from
`--disable-gpu` or `--disable-gpu-compositing` in the flags, and otherwise from Chromium's own
`gpu_compositing` status over DevTools. It does not create a WebGL context to ask: on the Omarchy
VM, with GPU compositing off, doing so left Omaweb not responding.

The harness only reports. It never fails because Omaweb is behind, because host noise here is larger
than the gap being measured. It exits non-zero only when a browser does not finish a suite.

`--profile` adds one run of the first suite per browser, not counted in the ratios. The harness
waits 15 seconds for the run to get going, then records the busiest renderer for 30 seconds with
`perf record -F 999` and prints the split by library and the top 20 symbols. #356 took the same
profile by hand. The data and the full reports stay under `~/.cache/omaweb-benchmarks/profiles/`. At
`kernel.perf_event_paranoid` 2, Arch's default, `perf` records only user space, so the kernel's
share is not in the split. Chromium's binary is stripped, so its symbols are addresses.

### The performance history

`performance/history.jsonl` holds one JSON line per recorded run, from
`benchmark_runtime.py --record` (`"kind": "budget"`) and `benchmark_chromium.py --record`
(`"kind": "comparison"`). The first `--record` creates it. A line can be read on its own. It
carries:

- `date`: when the run finished, in UTC
- `commit` and `omaweb`: the Omaweb commit and version. The commit is `HEAD` for a build in the
  checkout, or the release tag for an installed package.
- `machine`: set with `--machine` or described from the hardware. Runs from different machines share
  the file and are told apart by this field.
- `engine`: the engine library, its package and version, its Qt WebEngine and Chromium versions, and
  its `toolchain`, `clang` or `gcc`, read from the library's ELF `.comment` section. One package
  version has been built with both (#575), so the version alone cannot say which was measured. A
  library that is not ELF, such as a macOS framework, records it blank.
- for a budget run, `measurements`: each value beside the ceiling it was held to at the time
- for a comparison, `browsers` with each browser's version, flags and GPU status and the matched
  Chromium's `sha256` and the file it is of (`sha256_of`), `suites` with the pinned commits,
  `scores` with every run's score, and `no_gpu` naming the suites that ran without GPU compositing

```sh
scripts/benchmark_chromium.py plot
scripts/benchmark_chromium.py plot --output /tmp/history.html
```

`plot` writes one self-contained HTML page, `build/performance-history.html` by default, with no
script and nothing fetched. It draws the Omaweb/Chromium ratio per suite over time, with a solid
series for the latest Chromium and a dashed one for the matched Chromium, and each budget
measurement over time with its ceiling. Each machine has its own series. Hovering a point shows its
raw scores and the Chromium version it was measured against. Engine updates are marked on the time
axis. The version-lag gap should jump there, and the matched baseline changes Chromium version at
the same marks.

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
