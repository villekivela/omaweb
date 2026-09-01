# Use engine-provided developer tools

Tanto exposes one Open developer tools command and lets each engine adapter supply its own inspector. The Qt adapter docks the Chromium DevTools frontend beside the inspected tab through QtWebEngine's in-process developer-tools view; an engine without an inspector reports the capability as unavailable. Tanto does not build a diagnostics protocol, agent-access authority model, helper process, CLI, MCP server, terminal interface, trace format, or remote Development-target transport.

Developer tools attach to one tab, survive navigation and Space switches, and keep that tab active only while attached. They close when the tab closes or moves to another Space and never restore after restart. Remote debugging is limited to an explicit development launch option bound to loopback, and Private windows are unavailable for that launch. This decision supersedes ADRs 0019 through 0024.
