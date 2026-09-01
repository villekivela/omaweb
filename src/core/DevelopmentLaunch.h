#pragma once

#include <QString>
#include <QStringList>

namespace tanto {

// What an ordinary launch is not allowed to have. Developer tools are docked
// in-process and need no listener; a listener is a second, unauthenticated way
// into every page in the browser, so it exists only when the reader asked for
// it on the command line, only on loopback, and only in a session that opens no
// Private window — a private session's pages would be readable through it.
struct DevelopmentLaunch {
    bool remoteDebugging = false;
    // Host and port, bound to loopback whatever the reader asked for.
    QString listenAddress {};
    bool privateWindowsAvailable = true;
    // Non-empty when Tanto must refuse to start, and why.
    QString refusal {};
};

// `arguments` is the command line; `engineFlags` is whatever the environment
// passes straight through to the engine. A debugging switch aimed at the engine
// through either route is a refusal rather than a listener Tanto did not ask
// for: `--remote-debugging[=port]` is the one way in.
DevelopmentLaunch readDevelopmentLaunch(const QStringList &arguments,
    const QStringList &engineFlags = {});

} // namespace tanto
