#pragma once

#include "SessionStore.h"

#include <QHash>
#include <QSharedPointer>

namespace omaweb {

// Keeps a Private window's session nowhere on disk. Every write but the two
// below is accepted and dropped, and answers false so a caller reporting to
// the reader says what happened; every other read answers empty.
//
// Two things survive, in memory, and their writes answer true. The Site permissions the session's
// windows have agreed to live in the hash the session hands round its windows and go when the last
// of them closes. The hash is the session's, not this object's, so a decision made in one Private
// window is the same decision in the next. The favicons the window's pages showed are this
// object's, and each window is given a store of its own, so they go when the window closes.
class PrivateSessionStore final : public SessionStore {
public:
    explicit PrivateSessionStore(QSharedPointer<QHash<QString, int>> sessionDecisions);
    ~PrivateSessionStore() override;

    bool open(QString *errorMessage = nullptr) override;
    bool recordsState() const override;

    QVector<SpaceState> loadSpaces() const override;
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
    QHash<QString, SpaceProject> spaceProjects() const override;
    bool saveSpaceProject(const QString &spaceId, const SpaceProject &project) override;
    bool forgetSpaceProject(const QString &spaceId) override;

    QVector<TabState> loadTabs(const QString &spaceId) const override;
    QVector<TabState> loadClosedTabs(const QString &spaceId) const override;
    bool recordClosedTabs(const QString &spaceId, const QVector<TabState> &tabs) override;
    QVector<PutAwayTab> loadPutAwayTabs(const QString &spaceId) const override;
    bool recordPutAwayTabs(const QString &spaceId, const QVector<PutAwayTab> &tabs) override;
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
    bool deletePreference(const QString &name) override;

    bool recordVisit(const QString &spaceId, const QUrl &url, const QString &title) override;
    QVariantList history(const QString &spaceId, const QString &query, int limit) const override;
    bool deleteHistoryVisit(const QString &spaceId, qint64 id) override;
    bool deleteHistoryOrigin(const QString &spaceId, const QString &origin) override;
    bool deleteHistorySince(const QString &spaceId, qint64 since) override;
    bool recordFavicon(
        const QString &spaceId, const QUrl &pageUrl, const QByteArray &image) override;
    void findFavicon(const QString &spaceId, const QUrl &pageUrl,
        std::function<void(const QByteArray &image)> answer) const override;

    int permissionDecision(
        const QString &spaceId, const QString &origin, const QString &permission) const override;
    bool savePermissionDecision(const QString &spaceId, const QString &origin,
        const QString &permission, int decision) override;
    QVariantList permissionsForOrigin(const QString &spaceId, const QString &origin) const override;
    bool clearPermissionsForOrigin(const QString &spaceId, const QString &origin) override;
    bool clearPermissionsSince(const QString &spaceId, qint64 since) override;

    bool recordFormEntry(
        const QString &spaceId, const QString &field, const QString &value) override;
    QStringList formEntries(const QString &spaceId, const QString &field) const override;
    bool forgetFormEntry(
        const QString &spaceId, const QString &field, const QString &value) override;
    bool clearFormHistorySince(const QString &spaceId, qint64 since) override;
    QVariantList addresses() const override;
    bool saveAddress(const QVariantMap &address) override;
    bool deleteAddress(const QString &id) override;

    bool recordDownload(const QString &id, const QString &spaceId, const QUrl &url,
        const QString &path, const QString &state, qint64 receivedBytes,
        qint64 totalBytes) override;
    bool updateDownload(const QString &id, const QString &state, qint64 receivedBytes,
        qint64 totalBytes, const QString &error) override;
    QVariantList downloadHistory() const override;
    bool forgetDownload(const QString &id) override;
    bool forgetSpaceDownloads(const QString &spaceId) override;

private:
    struct Favicon {
        QString origin;
        QByteArray image;
        // Which icon of a site is newest, for a page of it that has none.
        quint64 recorded = 0;
    };

    QSharedPointer<QHash<QString, int>> m_sessionDecisions;
    // By Space, then page address. Each Private window has a store of its
    // own, so these go when the window does.
    QHash<QString, QHash<QString, Favicon>> m_favicons;
    quint64 m_faviconCount = 0;
};

} // namespace omaweb
