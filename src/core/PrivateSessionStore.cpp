#include "PrivateSessionStore.h"

namespace omaweb {

PrivateSessionStore::PrivateSessionStore(QSharedPointer<QHash<QString, int>> sessionDecisions)
    : m_sessionDecisions(std::move(sessionDecisions))
{
    if (!m_sessionDecisions) {
        m_sessionDecisions = QSharedPointer<QHash<QString, int>>::create();
    }
}

PrivateSessionStore::~PrivateSessionStore() = default;

// There is no directory to make and nothing to migrate, so a Private window is
// ready as soon as it is asked.
bool PrivateSessionStore::open(QString *) { return true; }

QVector<SpaceState> PrivateSessionStore::loadSpaces() const { return {}; }

bool PrivateSessionStore::saveSpace(const SpaceState &) { return false; }

bool PrivateSessionStore::setActiveSpace(const QString &) { return false; }

bool PrivateSessionStore::spaceHasSavedContent(const QString &) const { return false; }

bool PrivateSessionStore::deleteSpace(const QString &, const QString &) { return false; }

QVector<TabState> PrivateSessionStore::loadTabs(const QString &) const { return {}; }

QVector<TabState> PrivateSessionStore::loadClosedTabs(const QString &) const { return {}; }

bool PrivateSessionStore::recordClosedTabs(const QString &, const QVector<TabState> &)
{
    return false;
}

bool PrivateSessionStore::saveTab(const TabState &, int) { return false; }

bool PrivateSessionStore::saveTabs(const QString &, const QVector<TabState> &, const QString &)
{
    return false;
}

bool PrivateSessionStore::recordTabs(const QString &, const QVector<TabState> &, const QString &)
{
    return false;
}

bool PrivateSessionStore::saveSpaceMove(const QString &, const QVector<TabState> &, const QString &,
    const QString &, const QVector<TabState> &, const QString &)
{
    return false;
}

QString PrivateSessionStore::preference(const QString &, const QString &fallback) const
{
    return fallback;
}

bool PrivateSessionStore::savePreference(const QString &, const QString &) { return false; }

bool PrivateSessionStore::recordVisit(const QString &, const QUrl &, const QString &)
{
    return false;
}

QVariantList PrivateSessionStore::history(const QString &, const QString &, int) const
{
    return {};
}

bool PrivateSessionStore::deleteHistoryVisit(const QString &, qint64) { return false; }

bool PrivateSessionStore::deleteHistoryOrigin(const QString &, const QString &) { return false; }

bool PrivateSessionStore::deleteHistorySince(const QString &, qint64) { return false; }

// Read without being spent. Whether an answer is used once or held for the
// session is the shell's decision about the answer, not this store's about
// where it is kept.
int PrivateSessionStore::permissionDecision(
    const QString &spaceId, const QString &origin, const QString &permission) const
{
    return m_sessionDecisions->value(sessionPermissionKey(spaceId, origin, permission), 0);
}

bool PrivateSessionStore::savePermissionDecision(
    const QString &spaceId, const QString &origin, const QString &permission, int decision)
{
    m_sessionDecisions->insert(sessionPermissionKey(spaceId, origin, permission), decision);
    return true;
}

// What the session holds is listed by the shell, which reads the same hash for
// every window and does not need this store to repeat it.
QVariantList PrivateSessionStore::permissionsForOrigin(const QString &, const QString &) const
{
    return {};
}

bool PrivateSessionStore::clearPermissionsForOrigin(const QString &, const QString &)
{
    return false;
}

bool PrivateSessionStore::clearPermissionsSince(const QString &, qint64) { return false; }

bool PrivateSessionStore::recordDownload(
    const QString &, const QUrl &, const QString &, const QString &, qint64, qint64)
{
    return false;
}

bool PrivateSessionStore::updateDownload(
    const QString &, const QString &, qint64, qint64, const QString &)
{
    return false;
}

QVariantList PrivateSessionStore::downloadHistory() const { return {}; }

bool PrivateSessionStore::forgetDownload(const QString &) { return false; }

} // namespace omaweb
