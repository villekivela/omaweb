#include "Scenes.h"

#include "LifeBoard.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>

namespace omaweb {

QVariantMap Scenes::parameters(const QString &id) const
{
    QFile file(QStringLiteral(":/omaweb/scenes/%1.json").arg(id));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}

void registerScenes()
{
    qmlRegisterSingletonType<Scenes>("Omaweb", 1, 0, "Scenes",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new Scenes; });
    qmlRegisterType<LifeBoard>("Omaweb", 1, 0, "LifeBoard");
}

} // namespace omaweb
