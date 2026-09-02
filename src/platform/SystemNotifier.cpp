#include "SystemNotifier.h"

#include <QQmlEngine>

namespace tanto {

#ifndef Q_OS_MACOS

// No notification service here yet. Linux notifications arrive with the
// Wayland port; until then a page's request is refused, which is what the
// reader needs the page to believe rather than a notification nobody saw.
SystemNotifier::SystemNotifier(QObject *parent)
    : QObject(parent)
{
}

SystemNotifier::~SystemNotifier() = default;

bool SystemNotifier::available() const
{
    return false;
}

bool SystemNotifier::present(const QString &key, const QString &title, const QString &body)
{
    Q_UNUSED(key)
    Q_UNUSED(title)
    Q_UNUSED(body)
    return false;
}

void SystemNotifier::withdraw(const QString &key)
{
    Q_UNUSED(key)
}

#endif

void registerSystemNotifier()
{
    qmlRegisterSingletonType<SystemNotifier>("Tanto", 1, 0, "SystemNotifier",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SystemNotifier; });
}

} // namespace tanto
