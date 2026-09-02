---
status: superseded by ADR-0027
---

# Broker engine-neutral Diagnostics sessions

Omaweb exposes Diagnostics through temporary, permission-checked Debugging sessions attached to Omaweb tab identities. A bundled Feature module brokers versioned semantic operations to MCP and CLI Diagnostics clients, while engine adapters translate their native documents, frames, workers, network activity, console output, and page controls into the common contract. The public contract never exposes engine identifiers, payloads, or an escape hatch for raw engine commands.

The browser process owns approvals, tab identity, capability grants, and visible session status. A separate Diagnostics helper owns client transport, bounded event and body buffers, filtering, Debug traces, and engine-protocol translation, so diagnostic workloads and failures do not block browsing or corrupt browser state. One automatic helper restart discards temporary evidence and client credentials; repeated failure disables Diagnostics until the user starts it again.

The Qt adapter may use Chromium's debugging protocol internally because QtWebEngine's public APIs do not provide the complete network, DOM, and runtime evidence agents need. Qt 6.11 exposes that protocol only through a startup-enabled TCP port, so Omaweb binds an unpredictable loopback port and never advertises it to clients. This is not a security boundary against hostile code already running as the same operating-system user; Agent access constrains cooperating Diagnostics clients. Ladybird can implement the same meanings through its own APIs, and every adapter reports unsupported capabilities instead of imitating Chromium behavior it cannot provide.

The Qt build enables that internal endpoint by default so attaching does not require a browser restart, but starts neither the helper nor detailed capture until a session needs them. Disabling Diagnostics removes the endpoint and Incident buffer after restart. The helper wire protocol remains private and versioned; MCP tools, CLI JSON output, and the Debug-trace format are the supported client contracts.
