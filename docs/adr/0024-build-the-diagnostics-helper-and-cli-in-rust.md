---
status: superseded by ADR-0027
---

# Build the Diagnostics helper and CLI in Rust

Omaweb implements the out-of-process Diagnostics helper and the single `omaweb` command-line client
in Rust, while browser state, permission policy, visible status, and engine adapters remain C++ and
QML. The helper and CLI need bounded concurrent streams, terminal rendering, trace processing, and a
self-contained binary that Omaweb can install on a Development target; keeping that work outside the
browser also contains crashes while parsing engine and client data.

The browser and helper exchange length-prefixed JSON headers followed by optional raw payloads, so
control messages remain inspectable while bodies, screenshots, sources, and trace chunks avoid
base64. The Rust targets build independently and follow ADR 0007's artifact boundary: ordinary
browser edits do not run Cargo, and a Diagnostics source change rebuilds only its separately
packaged artifacts.
