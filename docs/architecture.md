# Tanto architecture

## Build variants

`tanto-qt` is the default development and current daily-driver variant. `tanto-ladybird` is configured and built separately against a pinned Ladybird revision. Ladybird never enters the default Qt configure or compile graph.

The variants share core state, persistence, QML, themes, platform code, and engine-contract tests. Engine-specific headers stay inside adapter targets. Live tabs do not migrate between engines, and each Space keeps separate engine-managed login state.

## Targets

- `tanto-core` owns Spaces, tabs, sessions, commands, persistence policy, history, permissions, downloads, and errors.
- `tanto-ui` contains shared QML and read-only UI-facing state, and drives the vendored kit's `qs.Commons` colour and type singletons from the theme palette.
- `tanto-engine-api` defines navigation, lifecycle, input, content-blocking, accessibility, and Developer-tools capabilities.
- `tanto-engine-qt` supplies the default engine view without leaking QtWebEngine imports into shared QML.
- `tanto-engine-ladybird` is an optional build outside the default graph.
- `tanto-quickshell-shim` registers the `Quickshell` and `Quickshell.Io` QML types the vendored Omarchy component kit imports.
- `tanto-platform` contains small macOS and Linux window integrations.
- `modules/*` contains independently compiled first-party Feature modules.
- `tanto-ui-lab` loads the shared QML against fake browser state and a mock engine.

QML renders read-only snapshots and submits commands. It does not mutate durable browser state directly.

## Storage

One global SQLite database records application state and the Space list. Each Space owns a directory containing its browser database and separate Qt and Ladybird engine-profile directories. Versioned JSON stores settings, keybindings, and themes.

Structural state commits transactionally. High-frequency presentation state saves on a short debounce. Engine caches and compiled blocker data are disposable and never enter Sync.

## Content blocker

The Qt adapter calls a separately built `adblock-rust` shared library through a narrow C interface. Its version matches the pinned Ladybird revision. Ladybird uses its internal copy. Both adapters run the same conformance fixtures.

Request matching uses an immutable in-memory snapshot. Subscription updates compile off the request path and atomically replace the active matcher.

## Developer tools

Core exposes one Open developer tools command. An adapter with an inspector attaches it to one tab and reports that capability; an adapter without one leaves the command unavailable. The Qt adapter hosts the bundled Chromium DevTools frontend in a second `WebEngineView` docked beside the inspected page. Tanto does not expose Chromium's debugging protocol during ordinary browsing.

An inspected tab stays active when another Space is selected because Qt requires both the inspected page and its Developer-tools view to remain active. Closing Developer tools releases that exception. Moving the tab to another Space closes Developer tools before the engine profile changes.

The frontend is themed by naming its own CSS custom properties again on the page it is drawn on, so Tanto ships no patched frontend and speaks no debugging protocol to it. Chromium themes it through the tonal ramp its browser UI theme serves at `devtools://theme/colors.css`, and every one of the frontend's design tokens is a tone of that ramp, so the Qt adapter names the ramp as well as the tokens it is sure about. The theme palette carries a `syntax` section for the colours code is read in, and the frontend's interface takes the theme's type family.

Remote debugging exists only behind the `--remote-debugging[=port]` launch option. Tanto binds it to loopback, refuses to start when a Chromium debugging switch reaches the engine through the command line or the environment, and takes Private windows away from that session.

## Keyboard navigation

Core loads and validates the versioned keybinding and site-passthrough policy. Engine adapters receive the resolved configuration for the active URL and the shared page-command script through the engine-view contract. The Qt adapter injects that script at document readiness; the UI-lab mock accepts the same inputs, and the Ladybird adapter must inject the same script when ticket #7 adds it.

Page commands ignore editable controls, IME composition, and unbound keys. Link hints use DOM accessibility labels, CSS lengths that follow page zoom, and system-color keywords that remain visible in forced-color modes.

## Window composition

Main and Private windows are frameless and expose explicit system-move and resize regions. Tanto does not draw visible window controls. Platform-native menus and keyboard commands retain minimize, close, and quit behavior.

Qt Quick owns the browser chrome. Webpage viewports stay opaque. macOS blur uses a small AppKit adapter; Wayland blur remains compositor-dependent and always has alpha and opaque fallbacks.

## Fast path

The default build uses C++23, Qt 6.11, CMake 3.30 or newer, Ninja, and Clang. Qt, Chromium, Ladybird, and the Rust blocker never compile during ordinary application edits.

On the initial M2 Max development machine, provisional budgets are:

- QML-only change visible in under 2 seconds
- Ordinary incremental C++ rebuild and launch under 10 seconds
- Clean Tanto build under 60 seconds, excluding prebuilt dependencies
- UI lab interactive within 500 milliseconds
- Browser chrome visible within 1 second warm and 2 seconds cold

Measurements replace these estimates once the toolchain and first executable exist.
