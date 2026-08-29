# Keep dependencies out of the fast build

Tanto's default development path uses C++23, the latest patch of a pinned Qt 6.11 SDK, QML and Qt Quick, CMake 3.30 or newer, Ninja, and Clang on macOS and Linux. Ordinary configure and compile steps never build Qt, Chromium, or Ladybird from source and do not fetch dependencies. Bootstrap tooling obtains or verifies binary dependencies separately so routine application edits touch only Tanto targets.

The source tree uses a small number of targets split by change frequency: `tanto-core`, `tanto-ui`, `tanto-engine-api`, `tanto-engine-qt`, optional `tanto-engine-ladybird`, `tanto-platform`, and independently built Feature modules under `modules/`. Engine headers stay inside their adapters. An engine-free `tanto-ui-lab` loads QML from the source tree against fake browser data so interface and theme work does not link or launch a web engine.

The Qt build's content blocker is a separately built Rust shared library imported into CMake as a verified bootstrap artifact. Cargo runs only when the blocker's source, wrapper, or lockfile changes. Rust types do not appear in public C++ headers, so ordinary Tanto changes do not rebuild the blocker.

CMake exposes separate `dev`, `ui`, `ladybird`, `asan`, `release`, and `ci` presets. `dev` is the default and includes only the Qt adapter; `ladybird` uses its own configure and build directory. On the initial M2 Max development machine, provisional budgets are under two seconds to see a QML-only change, under ten seconds for an ordinary incremental Tanto C++ rebuild and launch, and under sixty seconds for a clean Tanto build that excludes prebuilt dependencies. Measurements replace these estimates once the pinned toolchain is installed.

On the same machine, provisional runtime budgets are 500 milliseconds for `tanto-ui-lab` to become interactive, one second for warm browser chrome, and two seconds for cold browser chrome. Session restoration loads tabs progressively after the interface appears. Tanto-owned animation follows the display refresh rate without delaying command completion or blocking the UI thread beyond one frame.
