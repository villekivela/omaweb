#include "EngineVariant.h"

#include <QDir>

namespace omaweb {

QString EngineVariant::applicationName(Kind kind)
{
    return kind == Ladybird ? QStringLiteral("Omaweb-Ladybird") : QStringLiteral("Omaweb");
}

QString EngineVariant::configRoot(Kind kind, const QString &configHome)
{
    return QDir(configHome)
        .filePath(kind == Ladybird ? QStringLiteral("omaweb-ladybird") : QStringLiteral("omaweb"));
}

} // namespace omaweb
