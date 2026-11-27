#include "EngineCapabilities.h"

#include <QQmlEngine>

namespace omaweb {

EngineCapabilities::Capabilities ladybirdCapabilities()
{
    return EngineCapabilities::Navigation | EngineCapabilities::PersistentProfiles;
}

void registerEngineCapabilities()
{
    qmlRegisterUncreatableMetaObject(EngineCapabilities::staticMetaObject, "Omaweb.Engine", 1, 0,
        "EngineCapabilities", QStringLiteral("EngineCapabilities only names engine features"));
}

} // namespace omaweb
