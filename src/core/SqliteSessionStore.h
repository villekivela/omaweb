#pragma once

#include "SessionStore.h"
#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QHash>
#include <QSqlDatabase>
#include <QUrl>
#include <QVariantList>

namespace omaweb {

// Records a window's session in SQLite: one global database for the application
// state and the Space list, and one database per Space.
class SqliteSessionStore final : public SessionStore {
public:
    explicit SqliteSessionStore(QString dataRoot);
    ~SqliteSessionStore() override;

    bool open(QString *errorMessage = nullptr) override;
    QVector<SpaceState> loadSpaces() const override;
    QVector<TabState> loadTabs(const QString &spaceId) const override;
    QVector<TabState> loadClosedTabs(const QString &spaceId) const override;
    bool saveClosedTabs(const QString &spaceId, const QVector<TabState> &tabs) override;
    bool saveSpace(const SpaceState &space) override;
    bool setActiveSpace(const QString &spaceId) override;
    bool spaceHasSavedContent(const QString &spaceId) const override;
    bool deleteSpace(const QString &spaceId, const QString &replacementActiveSpaceId = {}) override;
    bool saveTab(const TabState &tab, int position) override;
    bool saveTabs(
        const QString &spaceId, const QVector<TabState> &tabs, const QString &activeTabId) override;
    bool saveSpaceMove(const QString &sourceSpaceId, const QVector<TabState> &sourceTabs,
        const QString &sourceActiveTabId, const QString &destinationSpaceId,
        const QVector<TabState> &destinationTabs, const QString &destinationActiveTabId) override;
    QString preference(const QString &name, const QString &fallback = {}) const override;
    bool savePreference(const QString &name, const QString &value) override;
    // Records one visit, and restores the retention bound once a batch of them
    // has arrived. A long session trims as it goes rather than only when the
    // Space database is first opened.
    bool recordVisit(const QString &spaceId, const QUrl &url, const QString &title) override;
    QVariantList history(const QString &spaceId, const QString &query, int limit) const override;
    bool deleteHistoryVisit(const QString &spaceId, qint64 id) override;
    bool deleteHistoryOrigin(const QString &spaceId, const QString &origin) override;
    bool deleteHistorySince(const QString &spaceId, qint64 since) override;
    bool clearPermissionsSince(const QString &spaceId, qint64 since) override;
    int permissionDecision(
        const QString &spaceId, const QString &origin, const QString &permission) const override;
    bool savePermissionDecision(const QString &spaceId, const QString &origin,
        const QString &permission, int decision) override;
    QVariantList permissionsForOrigin(const QString &spaceId, const QString &origin) const override;
    bool clearPermissionsForOrigin(const QString &spaceId, const QString &origin) override;
    bool recordDownload(const QString &id, const QUrl &url, const QString &path,
        const QString &state, qint64 receivedBytes, qint64 totalBytes) override;
    bool updateDownload(const QString &id, const QString &state, qint64 receivedBytes,
        qint64 totalBytes, const QString &error) override;
    QVariantList downloadHistory() const override;
    bool forgetDownload(const QString &id) override;

    QString dataRoot() const;

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
    QHash<QString, int> m_visitsSinceHistoryCleanup;
};

} // namespace omaweb
