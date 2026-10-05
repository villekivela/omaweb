#pragma once

#include <QString>

#include <vector>

namespace omaweb {

// The small `omaweb` client (ADR 0051), which is what `omaweb` on the PATH is.
// It links Qt's Core and Network and nothing else, so it runs where the
// browser cannot: in a container, a Distrobox, a VM or on another host, with
// the browser's Agent socket forwarded to it.

// The browser for a client in `clientDirectory`: beside it, as a build tree
// has it, or where the package installs it, `installedFromClient` away.
// Nothing when neither is there.
QString findAgentBrowser(const QString &clientDirectory, const QString &installedFromClient);

// Starts the browser at `path` apart from this process, which it outlives,
// as `omaweb mcp` does when no browser answers.
bool startAgentBrowser(const QString &path);

// Runs the client on `commandLine`, its `argv` as it came. A verb goes to the
// browser on `socketPath` and `mcp` serves MCP, starting the browser at
// `browserPath` when nothing answers. Anything else is the browser's: a launch,
// or an address the desktop opens, so the browser replaces this process and
// reads the same command line. Without a browser that says it is not installed
// and answers 1.
int runAgentClient(
    const std::vector<char *> &commandLine, const QString &socketPath, const QString &browserPath);

} // namespace omaweb
