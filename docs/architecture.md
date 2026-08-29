# Tanto architecture

## Build variants

`tanto-qt` is the default development and current daily-driver variant. `tanto-ladybird` is configured and built separately against a pinned Ladybird revision. Ladybird never enters the default Qt configure or compile graph.

The variants share core state, persistence, QML, themes, platform code, and engine-contract tests. Engine-specific headers stay inside adapter targets. Live tabs do not migrate between engines, and each Space keeps separate engine-managed login state.

## Targets

- `tanto-core` owns Spaces, tabs, sessions, commands, persistence policy, history, permissions, downloads, and errors.
- `tanto-ui` contains shared QML and read-only UI-facing state.
- `tanto-engine-api` defines navigation, lifecycle, input, content-blocking, accessibility, and diagnostics contracts.
- `tanto-engine-qt` supplies the default engine view without leaking QtWebEngine imports into shared QML.
- `tanto-engine-ladybird` is an optional build outside the default graph.
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

## Keyboard navigation

Core loads and validates the versioned keybinding and site-passthrough policy. Engine adapters receive the resolved configuration for the active URL through the shared engine-view contract. The Qt adapter injects the page command handler at document readiness; the UI-lab mock exposes the same contract for shared UI tests and for the Ladybird adapter to implement.

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
