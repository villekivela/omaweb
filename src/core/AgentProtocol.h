#pragma once

#include <QJsonObject>
#include <QString>

namespace omaweb {

// What the Agent socket carries (ADR 0051), between a client and a browser that
// may come from different releases: the browser and the small client are two
// packages, and a sandbox's client can be older or newer than the host's
// browser.
//
// The version rises with any change to a request or an answer that the other
// side would read differently. A verb added on both sides does not raise it: a
// browser refuses a verb it does not know rather than reading it as another.
inline constexpr int agentProtocolVersion = 1;

// `$XDG_RUNTIME_DIR/omaweb/control.sock` on Linux, and the same name under the
// per-user temporary directory on macOS, which has no runtime directory.
// `OMAWEB_CONTROL_SOCKET` names another, which comes first: a scratch browser
// runs beside the everyday one on it, and a sandbox with no login session has
// no runtime directory to look in.
QString agentSocketPath();

// The first request on a client's connection, naming the protocol it speaks
// and `version`, the release it came from.
QJsonObject agentHello(const QString &version);

// The browser's answer to it, naming its own.
QJsonObject agentHelloAnswer(const QString &version);

// The warning a client prints when the browser's answer to its hello names
// another protocol, naming both sides, or nothing when they agree. A browser
// older than the protocol refuses the hello as a verb it does not know, and
// that is a mismatch too.
QString agentProtocolMismatch(const QJsonObject &answer, const QString &version);

} // namespace omaweb
