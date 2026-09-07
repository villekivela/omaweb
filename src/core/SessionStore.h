#pragma once

#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVector>

namespace omaweb {

// What a window's session is kept in. A Private window writes nothing down, so
// the rule is the adapter it is given rather than a test every caller has to
// remember: `SqliteSessionStore` records, `PrivateSessionStore` accepts and
// drops. A write that was not written down answers false.
//
// Where the data root itself lives is not asked here. An adapter that keeps
// nothing has no directory to name, so the paths a Space's engine profile and
// history search need are static on the adapter that owns the layout.
class SessionStore {
public:
    virtual ~SessionStore() = default;

    SessionStore(const SessionStore &) = delete;
    SessionStore &operator=(const SessionStore &) = delete;

    // Called once before anything else. An adapter that records answers for
    // the directory it could not create; one that keeps nothing always agrees.
    virtual bool open(QString *errorMessage = nullptr) = 0;

    virtual QVector<SpaceState> loadSpaces() const = 0;
    virtual bool saveSpace(const SpaceState &space) = 0;
    virtual bool setActiveSpace(const QString &spaceId) = 0;
    virtual bool spaceHasSavedContent(const QString &spaceId) const = 0;
    virtual bool deleteSpace(const QString &spaceId, const QString &replacementActiveSpaceId = {})
        = 0;

    virtual QVector<TabState> loadTabs(const QString &spaceId) const = 0;
    virtual QVector<TabState> loadClosedTabs(const QString &spaceId) const = 0;
    virtual bool saveClosedTabs(const QString &spaceId, const QVector<TabState> &tabs) = 0;
    virtual bool saveTab(const TabState &tab, int position) = 0;
    virtual bool saveTabs(
        const QString &spaceId, const QVector<TabState> &tabs, const QString &activeTabId) = 0;
    virtual bool saveSpaceMove(const QString &sourceSpaceId, const QVector<TabState> &sourceTabs,
        const QString &sourceActiveTabId, const QString &destinationSpaceId,
        const QVector<TabState> &destinationTabs, const QString &destinationActiveTabId) = 0;

    virtual QString preference(const QString &name, const QString &fallback = {}) const = 0;
    virtual bool savePreference(const QString &name, const QString &value) = 0;

    virtual bool recordVisit(const QString &spaceId, const QUrl &url, const QString &title) = 0;
    virtual QVariantList history(const QString &spaceId, const QString &query, int limit) const = 0;
    virtual bool deleteHistoryVisit(const QString &spaceId, qint64 id) = 0;
    virtual bool deleteHistoryOrigin(const QString &spaceId, const QString &origin) = 0;
    virtual bool deleteHistorySince(const QString &spaceId, qint64 since) = 0;

    virtual int permissionDecision(
        const QString &spaceId, const QString &origin, const QString &permission) const = 0;
    virtual bool savePermissionDecision(
        const QString &spaceId, const QString &origin, const QString &permission, int decision) = 0;
    virtual QVariantList permissionsForOrigin(const QString &spaceId, const QString &origin) const
        = 0;
    virtual bool clearPermissionsForOrigin(const QString &spaceId, const QString &origin) = 0;
    virtual bool clearPermissionsSince(const QString &spaceId, qint64 since) = 0;

    virtual bool recordDownload(const QString &id, const QUrl &url, const QString &path,
        const QString &state, qint64 receivedBytes, qint64 totalBytes) = 0;
    virtual bool updateDownload(const QString &id, const QString &state, qint64 receivedBytes,
        qint64 totalBytes, const QString &error) = 0;
    virtual QVariantList downloadHistory() const = 0;
    virtual bool forgetDownload(const QString &id) = 0;

protected:
    SessionStore() = default;
};

} // namespace omaweb
