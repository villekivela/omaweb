# Tanto

Tanto is a keyboard-driven browser for developers. It runs on macOS during development and targets Linux with first-class Wayland support.

The default build uses QtWebEngine. Ladybird is the target engine, but it stays in a separate pinned build until its embedding and security contracts are mature enough for daily use.

## Current status

The repository contains the first vertical slice: one frameless window, an isolated Personal Space, vertical tabs, pinned tabs, a centered Omnibar, session persistence, transparent chrome, and an engine-free UI lab.

See [product requirements](docs/product/requirements.md), [architecture](docs/architecture.md), [development instructions](docs/development.md), and the [automatic network-request policy](docs/network-requests.md).

## Build

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

On macOS, run `./build/dev/tanto.app/Contents/MacOS/tanto`. On Linux, run `./build/dev/tanto`. Use the `ui` preset for interface work without QtWebEngine.

Configuration lives in `$XDG_CONFIG_HOME/tanto`, or `~/.config/tanto` when that is unset: `keybindings.json`, and a `theme.json` if you want one.

Tanto-owned code is licensed under MPL 2.0. Third-party engines and data retain their own licenses.
