#include "ThreadedSessionStore.h"

#include <QDebug>
#include <QMetaObject>

#include <utility>

namespace omaweb {

namespace {

    class StoreHost final : public QObject {
    public:
        explicit StoreHost(std::unique_ptr<SessionStore> store)
            : m_store(std::move(store))
        {
        }

        SessionStore *store() const { return m_store.get(); }
        void closeStore() { m_store.reset(); }

    private:
        std::unique_ptr<SessionStore> m_store;
    };

} // namespace

ThreadedSessionStore::ThreadedSessionStore(std::unique_ptr<SessionStore> store)
    : m_thread(std::make_unique<QThread>())
{
    auto *host = new StoreHost(std::move(store));
    m_store = host->store();
    m_host = host;
    m_thread->setObjectName(QStringLiteral("omaweb-session-store"));
    host->moveToThread(m_thread.get());
    QObject::connect(m_thread.get(), &QThread::finished, host, &QObject::deleteLater);
    m_thread->start();
}

template <typename Call> auto ThreadedSessionStore::ask(Call &&call) const
{
    using Result = std::invoke_result_t<Call>;
    if (QThread::currentThread() == m_thread.get()) {
        return call();
    }
    if constexpr (std::is_void_v<Result>) {
        QMetaObject::invokeMethod(m_host, std::forward<Call>(call), Qt::BlockingQueuedConnection);
    } else {
        Result result {};
        QMetaObject::invokeMethod(m_host, [&] { result = call(); }, Qt::BlockingQueuedConnection);
        return result;
    }
}

template <typename Call> bool ThreadedSessionStore::queue(const char *name, Call &&call)
{
    QMetaObject::invokeMethod(
        m_host,
        [name, call = std::forward<Call>(call)] {
            if (!call()) {
                qWarning("The session store refused a %s", name);
            }
        },
        Qt::QueuedConnection);
    return true;
}

// The queue is drained by waiting for one more call on it, then the store is
// closed where it was opened. Quitting first would drop what was still posted.
ThreadedSessionStore::~ThreadedSessionStore()
{
    ask([host = static_cast<StoreHost *>(m_host)] { host->closeStore(); });
    m_store = nullptr;
    m_thread->quit();
    m_thread->wait();
}

QThread *ThreadedSessionStore::thread() const { return m_thread.get(); }

bool ThreadedSessionStore::open(QString *errorMessage)
{
    return ask([this, errorMessage] { return m_store->open(errorMessage); });
}

QVector<SpaceState> ThreadedSessionStore::loadSpaces() const
{
    return ask([this] { return m_store->loadSpaces(); });
}

bool ThreadedSessionStore::saveSpace(const SpaceState &space)
{
    return ask([this, &space] { return m_store->saveSpace(space); });
}

bool ThreadedSessionStore::setActiveSpace(const QString &spaceId)
{
    return ask([this, &spaceId] { return m_store->setActiveSpace(spaceId); });
}

bool ThreadedSessionStore::spaceHasSavedContent(const QString &spaceId) const
{
    return ask([this, &spaceId] { return m_store->spaceHasSavedContent(spaceId); });
}

bool ThreadedSessionStore::deleteSpace(
    const QString &spaceId, const QString &replacementActiveSpaceId)
{
    return ask([this, &spaceId, &replacementActiveSpaceId] {
        return m_store->deleteSpace(spaceId, replacementActiveSpaceId);
    });
}

QVector<TabState> ThreadedSessionStore::loadTabs(const QString &spaceId) const
{
    return ask([this, &spaceId] { return m_store->loadTabs(spaceId); });
}

QVector<TabState> ThreadedSessionStore::loadClosedTabs(const QString &spaceId) const
{
    return ask([this, &spaceId] { return m_store->loadClosedTabs(spaceId); });
}

bool ThreadedSessionStore::recordClosedTabs(const QString &spaceId, const QVector<TabState> &tabs)
{
    return queue("closed-tab write",
        [this, spaceId, tabs] { return m_store->recordClosedTabs(spaceId, tabs); });
}

bool ThreadedSessionStore::saveTab(const TabState &tab, int position)
{
    return ask([this, &tab, position] { return m_store->saveTab(tab, position); });
}

bool ThreadedSessionStore::saveTabs(
    const QString &spaceId, const QVector<TabState> &tabs, const QString &activeTabId)
{
    return ask([this, &spaceId, &tabs, &activeTabId] {
        return m_store->saveTabs(spaceId, tabs, activeTabId);
    });
}

bool ThreadedSessionStore::recordTabs(
    const QString &spaceId, const QVector<TabState> &tabs, const QString &activeTabId)
{
    return queue("tab write", [this, spaceId, tabs, activeTabId] {
        return m_store->recordTabs(spaceId, tabs, activeTabId);
    });
}

bool ThreadedSessionStore::saveSpaceMove(const QString &sourceSpaceId,
    const QVector<TabState> &sourceTabs, const QString &sourceActiveTabId,
    const QString &destinationSpaceId, const QVector<TabState> &destinationTabs,
    const QString &destinationActiveTabId)
{
    return ask([&] {
        return m_store->saveSpaceMove(sourceSpaceId, sourceTabs, sourceActiveTabId,
            destinationSpaceId, destinationTabs, destinationActiveTabId);
    });
}

QString ThreadedSessionStore::preference(const QString &name, const QString &fallback) const
{
    return ask([this, &name, &fallback] { return m_store->preference(name, fallback); });
}

bool ThreadedSessionStore::savePreference(const QString &name, const QString &value)
{
    return ask([this, &name, &value] { return m_store->savePreference(name, value); });
}

bool ThreadedSessionStore::recordVisit(
    const QString &spaceId, const QUrl &url, const QString &title)
{
    return queue("visit record",
        [this, spaceId, url, title] { return m_store->recordVisit(spaceId, url, title); });
}

QVariantList ThreadedSessionStore::history(
    const QString &spaceId, const QString &query, int limit) const
{
    return ask([this, &spaceId, &query, limit] { return m_store->history(spaceId, query, limit); });
}

bool ThreadedSessionStore::deleteHistoryVisit(const QString &spaceId, qint64 id)
{
    return ask([this, &spaceId, id] { return m_store->deleteHistoryVisit(spaceId, id); });
}

bool ThreadedSessionStore::deleteHistoryOrigin(const QString &spaceId, const QString &origin)
{
    return ask([this, &spaceId, &origin] { return m_store->deleteHistoryOrigin(spaceId, origin); });
}

bool ThreadedSessionStore::deleteHistorySince(const QString &spaceId, qint64 since)
{
    return ask([this, &spaceId, since] { return m_store->deleteHistorySince(spaceId, since); });
}

int ThreadedSessionStore::permissionDecision(
    const QString &spaceId, const QString &origin, const QString &permission) const
{
    return ask([this, &spaceId, &origin, &permission] {
        return m_store->permissionDecision(spaceId, origin, permission);
    });
}

bool ThreadedSessionStore::savePermissionDecision(
    const QString &spaceId, const QString &origin, const QString &permission, int decision)
{
    return ask([this, &spaceId, &origin, &permission, decision] {
        return m_store->savePermissionDecision(spaceId, origin, permission, decision);
    });
}

QVariantList ThreadedSessionStore::permissionsForOrigin(
    const QString &spaceId, const QString &origin) const
{
    return ask(
        [this, &spaceId, &origin] { return m_store->permissionsForOrigin(spaceId, origin); });
}

bool ThreadedSessionStore::clearPermissionsForOrigin(const QString &spaceId, const QString &origin)
{
    return ask(
        [this, &spaceId, &origin] { return m_store->clearPermissionsForOrigin(spaceId, origin); });
}

bool ThreadedSessionStore::clearPermissionsSince(const QString &spaceId, qint64 since)
{
    return ask([this, &spaceId, since] { return m_store->clearPermissionsSince(spaceId, since); });
}

bool ThreadedSessionStore::recordDownload(const QString &id, const QUrl &url, const QString &path,
    const QString &state, qint64 receivedBytes, qint64 totalBytes)
{
    return ask(
        [&] { return m_store->recordDownload(id, url, path, state, receivedBytes, totalBytes); });
}

bool ThreadedSessionStore::updateDownload(const QString &id, const QString &state,
    qint64 receivedBytes, qint64 totalBytes, const QString &error)
{
    return ask(
        [&] { return m_store->updateDownload(id, state, receivedBytes, totalBytes, error); });
}

QVariantList ThreadedSessionStore::downloadHistory() const
{
    return ask([this] { return m_store->downloadHistory(); });
}

bool ThreadedSessionStore::forgetDownload(const QString &id)
{
    return ask([this, &id] { return m_store->forgetDownload(id); });
}

} // namespace omaweb
