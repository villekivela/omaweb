#include "SystemNotifier.h"

#include <QQmlEngine>

namespace omaweb {

void registerSystemNotifier()
{
    qmlRegisterSingletonType<SystemNotifier>("Omaweb", 1, 0, "SystemNotifier",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SystemNotifier; });
}

} // namespace omaweb
