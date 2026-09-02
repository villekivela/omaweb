#pragma once

#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QHash>
#include <QSqlDatabase>
#include <QUrl>
#include <QVariantList>

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
    QString preference(const QString &name, const QString &fallback = {}) const;
    bool savePreference(const QString &name, const QString &value);
    bool recordVisit(const QString &spaceId, const QUrl &url, const QString &title);
    QVariantList historySuggestions(const QString &spaceId, const QString &query, int limit) const;
    QVariantList history(const QString &spaceId, const QString &query, int limit) const;
    bool deleteHistoryVisit(const QString &spaceId, qint64 id);
    bool deleteHistoryOrigin(const QString &spaceId, const QString &origin);
    bool deleteHistorySince(const QString &spaceId, qint64 since);
    bool clearPermissionsSince(const QString &spaceId, qint64 since);
    int permissionDecision(const QString &spaceId, const QString &origin,
        const QString &permission) const;
    bool savePermissionDecision(const QString &spaceId, const QString &origin,
        const QString &permission, int decision);
    bool recordDownload(const QString &id, const QUrl &url, const QString &path,
        const QString &state, qint64 receivedBytes, qint64 totalBytes);
    bool updateDownload(const QString &id, const QString &state,
        qint64 receivedBytes, qint64 totalBytes, const QString &error);
    QVariantList downloadHistory() const;

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
