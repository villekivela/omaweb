#include "ExternalProtocolHandler.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QQmlEngine>

namespace omaweb {

ExternalProtocolHandler::ExternalProtocolHandler(QObject *parent)
    : QObject(parent)
{
}

QString ExternalProtocolHandler::applicationName(const QUrl &destination) const
{
#if defined(Q_OS_MACOS)
    return applicationNameForMac(destination);
#else
    return QCoreApplication::translate(
        "ExternalProtocolHandler", "the application registered for %1")
        .arg(destination.scheme());
#endif
}

bool ExternalProtocolHandler::open(const QString &destination) const
{
    const QUrl url(destination);
    if (!url.isValid() || url.scheme().isEmpty()) {
        return false;
    }
#if defined(Q_OS_MACOS)
    return QDesktopServices::openUrl(url);
#else
    // What QDesktopServices runs for it outside a sandbox, given the address
    // before Qt has parsed it.
    return QProcess::startDetached(QStringLiteral("xdg-open"), {destination});
#endif
}

void registerExternalProtocolHandler()
{
    qmlRegisterSingletonType<ExternalProtocolHandler>("Omaweb", 1, 0, "ExternalProtocolHandler",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new ExternalProtocolHandler; });
}

} // namespace omaweb
