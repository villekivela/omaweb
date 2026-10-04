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
    bool recordsState() const override;
    QVector<SpaceState> loadSpaces() const override;
    QVector<TabState> loadTabs(const QString &spaceId) const override;
    QVector<TabState> loadClosedTabs(const QString &spaceId) const override;
    bool recordClosedTabs(const QString &spaceId, const QVector<TabState> &tabs) override;
    QVector<PutAwayTab> loadPutAwayTabs(const QString &spaceId) const override;
    bool recordPutAwayTabs(const QString &spaceId, const QVector<PutAwayTab> &tabs) override;
    bool saveSpace(const SpaceState &space) override;
    bool saveSpaces(const QVector<SpaceState> &spaces) override;
    bool setActiveSpace(const QString &spaceId) override;
    bool spaceHasSavedContent(const QString &spaceId) const override;
    bool deleteSpace(const QString &spaceId, const QString &replacementActiveSpaceId = {}) override;
    QHash<QString, QString> agentSpaces() const override;
    QStringList temporaryAgentSpaceIds() const override;
    bool saveAgentSpace(const QString &spaceId, const QString &creator, bool temporary) override;
    bool forgetAgentSpace(const QString &spaceId) override;
    QStringList spaceGrants() const override;
    bool saveSpaceGrant(const QString &spaceId) override;
    bool forgetSpaceGrant(const QString &spaceId) override;
    bool saveTab(const TabState &tab, int position) override;
    bool saveTabs(
        const QString &spaceId, const QVector<TabState> &tabs, const QString &activeTabId) override;
    bool recordTabs(
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
    bool recordFavicon(
        const QString &spaceId, const QUrl &pageUrl, const QByteArray &image) override;
    void findFavicon(const QString &spaceId, const QUrl &pageUrl,
        std::function<void(const QByteArray &image)> answer) const override;
    bool clearPermissionsSince(const QString &spaceId, qint64 since) override;
    int permissionDecision(
        const QString &spaceId, const QString &origin, const QString &permission) const override;
    bool savePermissionDecision(const QString &spaceId, const QString &origin,
        const QString &permission, int decision) override;
    QVariantList permissionsForOrigin(const QString &spaceId, const QString &origin) const override;
    bool clearPermissionsForOrigin(const QString &spaceId, const QString &origin) override;
    bool recordFormEntry(
        const QString &spaceId, const QString &field, const QString &value) override;
    QStringList formEntries(const QString &spaceId, const QString &field) const override;
    bool forgetFormEntry(
        const QString &spaceId, const QString &field, const QString &value) override;
    bool clearFormHistorySince(const QString &spaceId, qint64 since) override;

    bool recordDownload(const QString &id, const QString &spaceId, const QUrl &url,
        const QString &path, const QString &state, qint64 receivedBytes,
        qint64 totalBytes) override;
    bool updateDownload(const QString &id, const QString &state, qint64 receivedBytes,
        qint64 totalBytes, const QString &error) override;
    QVariantList downloadHistory() const override;
    bool forgetDownload(const QString &id) override;
    bool forgetSpaceDownloads(const QString &spaceId) override;

    QString dataRoot() const;

private:
    bool executeSchema(QString *errorMessage);
    // Adds a column a table created by an earlier version lacks. A column that
    // is already there is left alone, and only a real failure answers false.
    bool addColumn(const QString &table, const QString &column, const QString &definition,
        QString *errorMessage);
    bool migrateLegacyTabs(QString *errorMessage);
    // Removes the directories of deleted Spaces. At start the rows naming them
    // go too; straight after a deletion they stay, for the next start.
    enum class PendingRows { Forget, Keep };
    void cleanupPendingSpaceDeletions(PendingRows rows);
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
