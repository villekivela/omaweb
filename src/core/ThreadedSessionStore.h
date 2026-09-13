#pragma once

#include "SessionStore.h"

#include <QThread>

#include <memory>

namespace omaweb {

// Runs a recording store on a thread of its own, so the interface never waits
// on the disk for the writes a session makes as it runs. The store it is given
// is opened, used and closed on that thread, which is where its SQLite
// connections then belong.
//
// The `record` writes, the visit, the coalesced tab write and the closed-tab
// stack, are queued and answered as taken. Every other call runs on the store
// thread while the caller waits for its answer, which is what a Space switch,
// move or delete needs before the interface moves on. The thread takes calls
// in the order they were made, so a call sees every queued write made before
// it, and closing the store lands whatever is still queued.
//
// A reader that wants the same ordering, such as History search, can live on
// the store's thread. A waiting call then also waits behind a search in
// progress, which is bounded by one scan of a Space's history.
class ThreadedSessionStore final : public SessionStore {
public:
    explicit ThreadedSessionStore(std::unique_ptr<SessionStore> store);
    ~ThreadedSessionStore() override;

    QThread *thread() const;

    bool open(QString *errorMessage = nullptr) override;
    QVector<SpaceState> loadSpaces() const override;
    bool saveSpace(const SpaceState &space) override;
    bool setActiveSpace(const QString &spaceId) override;
    bool spaceHasSavedContent(const QString &spaceId) const override;
    bool deleteSpace(const QString &spaceId, const QString &replacementActiveSpaceId = {}) override;
    QVector<TabState> loadTabs(const QString &spaceId) const override;
    QVector<TabState> loadClosedTabs(const QString &spaceId) const override;
    bool recordClosedTabs(const QString &spaceId, const QVector<TabState> &tabs) override;
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
    bool recordVisit(const QString &spaceId, const QUrl &url, const QString &title) override;
    QVariantList history(const QString &spaceId, const QString &query, int limit) const override;
    bool deleteHistoryVisit(const QString &spaceId, qint64 id) override;
    bool deleteHistoryOrigin(const QString &spaceId, const QString &origin) override;
    bool deleteHistorySince(const QString &spaceId, qint64 since) override;
    int permissionDecision(
        const QString &spaceId, const QString &origin, const QString &permission) const override;
    bool savePermissionDecision(const QString &spaceId, const QString &origin,
        const QString &permission, int decision) override;
    QVariantList permissionsForOrigin(const QString &spaceId, const QString &origin) const override;
    bool clearPermissionsForOrigin(const QString &spaceId, const QString &origin) override;
    bool clearPermissionsSince(const QString &spaceId, qint64 since) override;
    bool recordDownload(const QString &id, const QUrl &url, const QString &path,
        const QString &state, qint64 receivedBytes, qint64 totalBytes) override;
    bool updateDownload(const QString &id, const QString &state, qint64 receivedBytes,
        qint64 totalBytes, const QString &error) override;
    QVariantList downloadHistory() const override;
    bool forgetDownload(const QString &id) override;

private:
    // Runs a call on the store thread and waits for it. Called from that
    // thread itself, it runs the call in place, because waiting there would
    // wait forever.
    template <typename Call> auto ask(Call &&call) const;
    // Posts a write to the store thread and answers at once. A write the
    // store then refuses is reported, since the caller is no longer there to
    // hear it.
    template <typename Call> bool queue(const char *name, Call &&call);

    std::unique_ptr<QThread> m_thread;
    // The object the calls are posted to. It lives on the store thread and
    // owns the store, so the store's destructor runs there too.
    QObject *m_host = nullptr;
    SessionStore *m_store = nullptr;
};

} // namespace omaweb
