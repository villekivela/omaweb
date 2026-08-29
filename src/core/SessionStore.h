#pragma once

#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QHash>
#include <QSqlDatabase>

namespace tanto {

class SessionStore final {
public:
    explicit SessionStore(QString dataRoot);
    ~SessionStore();

    SessionStore(const SessionStore &) = delete;
    SessionStore &operator=(const SessionStore &) = delete;

    bool open(QString *errorMessage = nullptr);
    QVector<SpaceState> loadSpaces() const;
    QVector<TabState> loadTabs(const QString &spaceId) const;
    bool saveSpace(const SpaceState &space);
    bool setActiveSpace(const QString &spaceId);
    bool spaceHasSavedContent(const QString &spaceId) const;
    bool deleteSpace(const QString &spaceId, const QString &replacementActiveSpaceId = {});
    bool saveTab(const TabState &tab, int position);
    bool saveTabs(const QString &spaceId, const QVector<TabState> &tabs,
        const QString &activeTabId);
    bool saveSpaceMove(const QString &sourceSpaceId, const QVector<TabState> &sourceTabs,
        const QString &sourceActiveTabId, const QString &destinationSpaceId,
        const QVector<TabState> &destinationTabs, const QString &destinationActiveTabId);

    QString dataRoot() const;
    QString engineProfilePath(const QString &spaceId, const QString &engineName) const;

private:
    bool executeSchema(QString *errorMessage);
    bool migrateLegacyTabs(QString *errorMessage);
    void cleanupPendingSpaceDeletions();
    QSqlDatabase spaceDatabase(const QString &spaceId) const;
    void closeSpaceDatabase(const QString &spaceId);

    QString m_dataRoot;
    QString m_connectionName;
    QSqlDatabase m_database;
    mutable QHash<QString, QSqlDatabase> m_spaceDatabases;
    mutable QHash<QString, QString> m_spaceConnectionNames;
};

} // namespace tanto
