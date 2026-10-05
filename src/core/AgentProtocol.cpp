#include "AgentProtocol.h"

#include <QDir>
#include <QStandardPaths>

namespace omaweb {

QString agentSocketPath()
{
    const auto override = qEnvironmentVariable("OMAWEB_CONTROL_SOCKET");
    if (!override.isEmpty()) {
        return override;
    }
#if defined(Q_OS_MACOS)
    const auto base = QDir::tempPath();
#else
    // Qt reads XDG_RUNTIME_DIR, and falls back to a directory of its own that
    // it has checked belongs to this user when the session did not set one.
    const auto base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#endif
    return QDir(base).filePath(QStringLiteral("omaweb/control.sock"));
}

QJsonObject agentHello(const QString &version)
{
    return {{QStringLiteral("verb"), QStringLiteral("hello")},
        {QStringLiteral("protocol"), agentProtocolVersion}, {QStringLiteral("version"), version}};
}

QJsonObject agentHelloAnswer(const QString &version)
{
    return {{QStringLiteral("ok"), true}, {QStringLiteral("protocol"), agentProtocolVersion},
        {QStringLiteral("version"), version}};
}

QString agentProtocolMismatch(const QJsonObject &answer, const QString &version)
{
    const auto protocol = answer.value(QStringLiteral("protocol"));
    if (!protocol.isDouble()) {
        return QStringLiteral("omaweb: this omaweb speaks Agent protocol %1 (Omaweb %2), and the "
                              "browser is older than the protocol and names no version. A verb "
                              "it does not know is refused.")
            .arg(agentProtocolVersion)
            .arg(version);
    }
    if (protocol.toInt() == agentProtocolVersion) {
        return {};
    }
    return QStringLiteral("omaweb: this omaweb speaks Agent protocol %1 (Omaweb %2), and the "
                          "browser speaks protocol %3 (Omaweb %4). A verb one of them does not "
                          "know is refused.")
        .arg(agentProtocolVersion)
        .arg(version)
        .arg(protocol.toInt())
        .arg(answer.value(QStringLiteral("version")).toString());
}

} // namespace omaweb
