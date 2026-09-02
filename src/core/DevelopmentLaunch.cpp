#include "DevelopmentLaunch.h"

namespace omaweb {
namespace {

constexpr auto debuggingOption = "--remote-debugging";
constexpr int defaultPort = 9222;

// Every way Chromium can be told to open a debugging channel that is not
// Omaweb's own option. Passing one of these means the channel would be opened
// behind Omaweb's back: on whatever interface the flag named, with Private
// windows still on offer.
bool isEngineDebuggingFlag(const QString &argument)
{
    static const QStringList flags = {
        QStringLiteral("--remote-debugging-port"),
        QStringLiteral("--remote-debugging-address"),
        QStringLiteral("--remote-debugging-pipe"),
    };
    for (const auto &flag : flags) {
        if (argument == flag || argument.startsWith(flag + QStringLiteral("="))) {
            return true;
        }
    }
    return false;
}

} // namespace

DevelopmentLaunch readDevelopmentLaunch(const QStringList &arguments,
    const QStringList &engineFlags)
{
    DevelopmentLaunch launch;
    const auto refuse = [&launch](const QString &reason) {
        launch = {};
        launch.refusal = reason;
        return launch;
    };

    for (const auto &argument : arguments + engineFlags) {
        if (isEngineDebuggingFlag(argument)) {
            return refuse(QStringLiteral("%1 opens a debugging listener Omaweb does not "
                                         "control. Use --remote-debugging=<port> instead.")
                    .arg(argument));
        }
        if (argument != QLatin1String(debuggingOption)
            && !argument.startsWith(QLatin1String(debuggingOption) + QStringLiteral("="))) {
            continue;
        }
        auto port = defaultPort;
        const auto separator = argument.indexOf(u'=');
        if (separator != -1) {
            auto valid = false;
            port = argument.sliced(separator + 1).toInt(&valid);
            // Below 1024 needs privilege Omaweb does not want, and 0 would have
            // the engine pick a port nothing here could report.
            if (!valid || port < 1024 || port > 65535) {
                return refuse(QStringLiteral("%1 is not a port between 1024 and 65535")
                        .arg(argument.sliced(separator + 1)));
            }
        }
        launch.remoteDebugging = true;
        launch.listenAddress = QStringLiteral("127.0.0.1:%1").arg(port);
        // A Private window's pages would be readable through the listener by
        // anything running as this user, which is the opposite of what asking
        // for a Private window means.
        launch.privateWindowsAvailable = false;
    }
    return launch;
}

} // namespace omaweb
