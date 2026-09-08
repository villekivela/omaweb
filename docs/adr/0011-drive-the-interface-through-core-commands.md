# Drive the interface through core commands

`omaweb-core` owns Space, tab, pin, and session mutations. QML renders read-only state snapshots and
submits commands for state and policy that the core owns rather than editing browser state directly.
Operations owned wholly by an engine, including page navigation, go through the active engine host
without a core relay. A core decision may request a specific engine operation, such as reloading a
tab after renderer recovery. Structural changes commit transactionally as they happen, while
high-frequency presentation state uses a short debounce. After a crash, Omaweb restores the last
committed session and identifies tabs whose loads were interrupted.

Tests cover core behavior, database migrations and recovery, QML interaction through
`omaweb-ui-lab`, a shared engine contract against fake, Qt, and Ladybird adapters, and a small
end-to-end browsing set. Screenshot checks remain limited to theme and transparency states. CI
treats warnings in Omaweb code as errors, isolates third-party warnings, runs routine macOS and
Linux checks, and builds Ladybird on a schedule or when its integration changes.

The engine contract tests renderer-crash recovery, visible keyboard focus, screen-reader labels for
browser chrome, IME composition, non-English keyboard layouts, and logical focus order. A renderer
crash replaces only the affected tab with a recovery page and does not terminate Omaweb or discard
the Space session.
