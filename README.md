<img src="assets/icons/omaweb.svg" alt="" width="72">

# Omaweb

Omaweb is a keyboard-driven web browser. One window, tabs down the side, Spaces that keep logins
apart, and a palette that follows the desktop's own theme.

Linux with first-class Wayland support is the platform Omaweb ships on, and the only one CI builds.
It also builds on macOS as a development platform; those bundles are never distributed. Pages are
rendered by QtWebEngine.

**Omaweb is alpha.** It ships no phishing, malware, or download-reputation service, so a malicious
address raises no warning ([ADR 0032](docs/adr/0032-ship-without-url-reputation.md)). Becoming the
desktop's default browser is the one Wayland check still to be run on real hardware
([#103](https://github.com/villekivela/omaweb/issues/103)).

## Status

The browser contract is complete: Spaces, tabs, the Omnibar, Keyboard navigation, docked Developer
tools, page commands, History and Settings, Site information, Content blocking, and download
hardening all work. On Linux it runs frameless on Wayland, opens the addresses the desktop hands it,
sends notifications and prints through the desktop's own services, and offers to become the default
browser.

What is left is the validation sweep across input, accessibility and IME on a real desktop
([#103](https://github.com/villekivela/omaweb/issues/103)). The renderer sandbox is already verified
under Hyprland. [The roadmap](docs/roadmap.md) has the ordered breakdown, and what else is planned.

## Install

Build the package from the repository. It uses system Qt, which means an engine security update
arrives from the distribution rather than from a rebuild here.

```sh
git clone https://github.com/villekivela/omaweb.git
cd omaweb/packaging && makepkg -si
```

From `v0.2.0` on, a release carries a built `.pkg.tar.zst` that installs without a build. The
`v0.1.x` releases carry notes alone.

```sh
sudo pacman -U omaweb-git-*.pkg.tar.zst
```

That asset is `x86_64`. Releases are built in Arch's own container image, which is published for
that architecture alone, so `aarch64` is a supported architecture to build for and has no built
asset of its own ([#185](https://github.com/villekivela/omaweb/issues/185)). On `aarch64`, use the
source build above.

Install `fcitx5-qt` as well if you use an input method. Omarchy points every Qt application at
`fcitx` without shipping the plugin, so without it Qt binds no text-input protocol and an input
method silently does nothing.

## What it does

- Spaces with separate cookies, logins, history, permissions, sessions, and tabs
- Ordinary and Pinned tabs, Keep active for a Pinned tab, Private windows, and site-requested
  Auxiliary windows
- A centered Omnibar for addresses, search, tabs, Spaces, and browser commands
- Keyboard navigation, configurable bindings, link hints, and a Start page that reads its shortcut
  sheet from the running keymap
- Docked Chromium DevTools, Inspect element, and loopback-only remote debugging behind an explicit
  launch option
- Find, per-tab zoom, reload with and without cache, Stop loading, fullscreen, printing, and inline
  PDF viewing
- Omaweb-drawn page context menus and browser prompts, and external-protocol confirmation
- Tab reordering, duplication, bulk closing, and a per-Space stack of the 25 most recently closed
  tabs
- Searchable Space-local History, browsing-data controls, Site permissions, and a site-information
  panel carrying connection state, certificate errors, and the third-party-cookie allowance
- Downloads that ask before writing a program down, mark what they saved with where it came from,
  strip its execute bits, and never open it
- Content blocking with EasyList, EasyPrivacy, cosmetic rules, Scriptlets, and Substitute resources
- Runtime themes, terminal-theme import, Omarchy theme following, and Private-window styling

Everything the browser can do is reachable from the command panel. An action that is not is a
defect.

## Keyboard model

`Primary` is Control on Linux and Command on macOS.

| Keys                          | Action                                    |
| ----------------------------- | ----------------------------------------- |
| `Primary+K` or `:`            | Open the command panel                    |
| `Primary+L` or `o`            | Open an address or search                 |
| `Primary+T` or `t`            | Start a new tab                           |
| `Primary+W` or `x`            | Close the current tab                     |
| `Primary+F` or `/`            | Find in the page                          |
| `Primary+Shift+I`             | Toggle Developer tools                    |
| `Primary+B`                   | Hide or show the sidebar                  |
| `f`, `Shift+F`                | Follow a link here or in a background tab |
| `j`, `k`, `d`, `u`, `gg`, `G` | Scroll the page                           |
| `Primary+/` or `?`            | Show every current binding                |

Single-key commands follow the Keyboard navigation setting. Editable controls still receive typing,
and a site can keep selected conflicting keys. The full map is
[`assets/keybindings/default.json`](assets/keybindings/default.json).

## Configuration

User-editable files live in `$XDG_CONFIG_HOME/omaweb`, or `~/.config/omaweb`: `keybindings.json` for
bindings and per-site key passthrough, `theme.json` for the palette, and `search-engines.json` for
the local engine list, which starts on DuckDuckGo with remote suggestions off.

Following the desktop's theme needs no setup. The first start on a machine with Omarchy installs the
template Omaweb ships, so `omarchy theme set` repaints the browser along with the bar and the
terminal, without a restart. Blur behind the browser's transparent surfaces is the compositor's to
apply and needs nothing from Omaweb; `integrations/omarchy/README.md` has that rule and the window
opacity one.

## Building

Qt 6.11 at or above the approved patch level, CMake 3.30, Ninja, Clang with C++23, and ccache.

```sh
scripts/bootstrap_content_blocker.sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

Run it with `./build/dev/omaweb`. The UI lab draws the same QML without a web engine, which is the
cheap way to review chrome changes. [The development guide](docs/development.md) covers the presets,
the platform layer, packaging, and releases.

Read [the architecture](docs/architecture.md),
[the product requirements](docs/product/requirements.md), and the [domain glossary](CONTEXT.md)
before changing a browser contract. The glossary is the one to read first: this project is strict
about its nouns, and a review will be too. [CONTRIBUTING.md](CONTRIBUTING.md) covers the rest.

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

Omaweb has no telemetry, advertising identifier, browser account, cloud sync, push service, or
automatic crash upload. Every automatic request the browser makes is listed in
[the network request ledger](docs/network-requests.md).

Every page runs in its own sandboxed renderer. Omaweb refuses to start when the sandbox is disabled
through its own command line or through `QTWEBENGINE_CHROMIUM_FLAGS`, and refuses to start on a
Linux host whose kernel cannot meet the sandbox's prerequisites. There is no flag that turns that
check off. QtWebEngine runs the network service inside the browser process rather than in a
sandboxed process of its own, and Omaweb does not describe it as isolated.

An ordinary session opens no listening socket. `security/baseline.json` names the approved
QtWebEngine and the Chromium release whose security fixes it carries, so Settings reports whether
the running engine meets the baseline and CI compares it against upstream weekly.

[SECURITY.md](SECURITY.md) has the vulnerability reporting form and the response commitment.

## License

Omaweb's own code is under MPL 2.0. The engine, filter data, interface components, and other
third-party material keep their own licenses. Every release carries an inventory of what is in it;
see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
