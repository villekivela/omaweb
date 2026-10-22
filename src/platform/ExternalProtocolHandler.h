#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace omaweb {

#if defined(Q_OS_MACOS)
QString applicationNameForMac(const QUrl &destination);
#endif

class ExternalProtocolHandler final : public QObject {
    Q_OBJECT

public:
    explicit ExternalProtocolHandler(QObject *parent = nullptr);
    Q_INVOKABLE QString applicationName(const QUrl &destination) const;
    // Opens the address in the app the desktop has for its scheme, as the
    // page wrote it. A QUrl would lowercase what follows the scheme, which an
    // app may read as a name: Slack's sign-in puts a workspace ID there.
    Q_INVOKABLE bool open(const QString &destination) const;
};

void registerExternalProtocolHandler();

} // namespace omaweb
