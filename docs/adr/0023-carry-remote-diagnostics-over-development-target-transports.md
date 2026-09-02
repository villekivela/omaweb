---
status: superseded by ADR-0027
---

# Carry remote Diagnostics over Development-target transports

A Project attachment names a path on a Development target, which may be the host or an SSH-accessible environment such as an OrbStack Linux machine. Omaweb launches the external terminal into that target and path, then carries the same versioned Diagnostics client protocol over the target's authenticated bidirectional transport. The browser broker never opens a Diagnostics listener to a VM, LAN, or remote network.

SSH is the first remote transport. Omaweb uses the user's SSH configuration, agent, keys, and host verification rather than storing credentials, and an explicit setup action may copy a matching checksummed CLI to a user-local target directory without administrator privileges. The transport contract does not depend on SSH, so later container or VM integrations can supply another authenticated byte stream without changing MCP, the TUI, or the browser contract.

OrbStack is a first-party setup preset that resolves a selected Linux machine to its existing SSH host and suggests its stable `machine-name.orb.local` origin. The saved target remains ordinary SSH configuration. Omaweb does not start, stop, or otherwise own the machine lifecycle.
