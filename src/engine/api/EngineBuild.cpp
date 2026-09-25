#include "EngineBuild.h"

#include <QQmlEngine>

namespace omaweb {

EngineBuild::EngineBuild(QObject *parent)
    : QObject(parent)
{
}

bool EngineBuild::knownExtensions() const
{
#if OMAWEB_KNOWN_EXTENSIONS
    return true;
#else
    return false;
#endif
}

bool EngineBuild::cnameUncloaking() const
{
#if OMAWEB_CNAME_UNCLOAKING
    return true;
#else
    return false;
#endif
}

// Every browser Omaweb builds runs the Qt engine, which applies them from
// script and needs nothing from the engine's patches. The Ladybird adapter
// (#7) reports its own answer when it lands.
bool EngineBuild::proceduralCosmeticFiltering() const { return true; }

void registerEngineBuild()
{
    qmlRegisterSingletonType<EngineBuild>("Omaweb.Engine", 1, 0, "EngineBuild",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new EngineBuild; });
}

} // namespace omaweb
