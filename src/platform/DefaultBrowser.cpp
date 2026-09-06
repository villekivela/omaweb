#include "DefaultBrowser.h"

#include <QQmlEngine>

namespace omaweb {

#ifdef Q_OS_MACOS

// macOS records the default browser through Launch Services, and asking for it
// there means an unsigned development build prompting for a change the system
// will not keep. Omaweb does not distribute macOS builds (ADR 0029), so the
// offer is not made rather than made and refused.
DefaultBrowser::DefaultBrowser(QObject *parent)
    : QObject(parent)
{
}

bool DefaultBrowser::available() const { return false; }

bool DefaultBrowser::isDefault() const { return false; }

bool DefaultBrowser::makeDefault() { return false; }

void DefaultBrowser::refresh() { }

#endif

void registerDefaultBrowser()
{
    qmlRegisterSingletonType<DefaultBrowser>("Omaweb", 1, 0, "DefaultBrowser",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new DefaultBrowser; });
}

} // namespace omaweb
