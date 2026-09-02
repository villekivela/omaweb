#include "ExternalProtocolHandler.h"

#include <QDesktopServices>
#include <QQmlEngine>

namespace tanto {

ExternalProtocolHandler::ExternalProtocolHandler(QObject *parent)
    : QObject(parent)
{
}

QString ExternalProtocolHandler::applicationName(const QUrl &destination) const
{
    return QStringLiteral("the application registered for %1").arg(destination.scheme());
}

bool ExternalProtocolHandler::open(const QUrl &destination) const
{
    return destination.isValid() && QDesktopServices::openUrl(destination);
}

void registerExternalProtocolHandler()
{
    qmlRegisterSingletonType<ExternalProtocolHandler>("Tanto", 1, 0, "ExternalProtocolHandler",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new ExternalProtocolHandler; });
}

} // namespace tanto
