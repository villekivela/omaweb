#include "AgentsFile.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace omaweb {
namespace {

    QString path(const QString &configRoot)
    {
        return QDir(configRoot).filePath(AgentsFile::fileName());
    }

    QJsonObject contents(const QString &configRoot)
    {
        QFile file(path(configRoot));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }

} // namespace

QString AgentsFile::fileName() { return QStringLiteral("agents.json"); }

QJsonValue AgentsFile::read(const QString &configRoot, QLatin1StringView key)
{
    if (configRoot.isEmpty()) {
        return {};
    }
    return contents(configRoot).value(key);
}

// A file that cannot be read is replaced rather than kept: the reader's
// decision has to land, and a file nobody could read held nobody's.
void AgentsFile::write(const QString &configRoot, QLatin1StringView key, const QJsonValue &value)
{
    if (configRoot.isEmpty() || !QDir().mkpath(configRoot)) {
        return;
    }
    auto object = contents(configRoot);
    if (value.isUndefined() || value.isNull()) {
        object.remove(key);
    } else {
        object.insert(key, value);
    }
    QSaveFile file(path(configRoot));
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    file.commit();
}

} // namespace omaweb
