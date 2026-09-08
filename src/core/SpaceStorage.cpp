#include "SpaceStorage.h"

#include <QDir>

#include <utility>

namespace omaweb {

SpaceStorage::SpaceStorage(QString dataRoot, QString engineName)
    : m_dataRoot(std::move(dataRoot))
    , m_engineName(std::move(engineName))
{
}

QString SpaceStorage::dataRoot() const { return m_dataRoot; }

QString SpaceStorage::engineName() const { return m_engineName; }

QString SpaceStorage::profilePathFor(const QString &spaceId) const
{
    return QDir(m_dataRoot)
        .filePath(QStringLiteral("spaces/%1/engines/%2").arg(spaceId, m_engineName));
}

QString SpaceStorage::databasePathFor(const QString &spaceId) const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("spaces/%1/browser.sqlite").arg(spaceId));
}

} // namespace omaweb
