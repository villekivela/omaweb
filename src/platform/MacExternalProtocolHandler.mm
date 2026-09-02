#include "ExternalProtocolHandler.h"

#include <AppKit/AppKit.h>
namespace tanto {

QString applicationNameForMac(const QUrl &destination)
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

} // namespace tanto
