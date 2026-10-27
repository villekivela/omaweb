#include "Scenes.h"

#include "LifeBoard.h"

#include <QDir>
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

static bool underSystem(const QString &path)
{
    return path.startsWith(QStringLiteral("/proc/")) || path.startsWith(QStringLiteral("/sys/"));
}

QString Scenes::systemFile(const QString &path) const
{
    if (!underSystem(path)) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QStringList Scenes::systemEntries(const QString &path) const
{
    if (!underSystem(path)) {
        return {};
    }
    return QDir(path).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
}

QString Scenes::telemetryText() const { return qEnvironmentVariable("OMAWEB_TELEMETRY_TEXT"); }

void registerScenes()
{
    qmlRegisterSingletonType<Scenes>("Omaweb", 1, 0, "Scenes",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new Scenes; });
    qmlRegisterType<LifeBoard>("Omaweb", 1, 0, "LifeBoard");
}

} // namespace omaweb
