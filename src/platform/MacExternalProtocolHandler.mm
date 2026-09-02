#include "ExternalProtocolHandler.h"

#include <AppKit/AppKit.h>
#include <QDesktopServices>
#include <QQmlEngine>

namespace tanto {

ExternalProtocolHandler::ExternalProtocolHandler(QObject *parent)
    : QObject(parent)
{
}

QString ExternalProtocolHandler::applicationName(const QUrl &destination) const
{
    const auto address = destination.toString().toNSString();
    NSURL *applicationUrl = [[NSWorkspace sharedWorkspace]
        URLForApplicationToOpenURL:[NSURL URLWithString:address]];
    if (applicationUrl) {
        NSBundle *bundle = [NSBundle bundleWithURL:applicationUrl];
        NSString *name = [bundle objectForInfoDictionaryKey:@"CFBundleDisplayName"];
        if (!name) name = [bundle objectForInfoDictionaryKey:@"CFBundleName"];
        if (!name) name = applicationUrl.lastPathComponent.stringByDeletingPathExtension;
        if (name) return QString::fromNSString(name);
    }
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
