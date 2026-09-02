#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace tanto {

#if defined(Q_OS_MACOS)
QString applicationNameForMac(const QUrl &destination);
#endif

class ExternalProtocolHandler final : public QObject {
    Q_OBJECT

public:
    explicit ExternalProtocolHandler(QObject *parent = nullptr);
    Q_INVOKABLE QString applicationName(const QUrl &destination) const;
    Q_INVOKABLE bool open(const QUrl &destination) const;
};

void registerExternalProtocolHandler();

} // namespace tanto
