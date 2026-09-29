#pragma once

#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVector>

#include <functional>

namespace omaweb {

// How a session's own Site permissions are keyed, in memory, for as long as
// the session lasts. The layout is named here because both a Private window's
// store and the shell that reads those answers back have to spell it the same
// way.
inline QString sessionPermissionKey(
    const QString &spaceId, const QString &origin, const QString &permission)
{
    return spaceId + QChar(0x1f) + origin + QChar(0x1f) + permission;
}

// What a window's session is kept in. A Private window writes nothing down, so
// the rule is the adapter it is given rather than a test every caller has to
// remember: `SqliteSessionStore` records, `PrivateSessionStore` accepts and
// drops. A write that was not written down answers false.
//
// The writes named `record` are the ones a session makes as it runs: the
// visit, the favicon, the coalesced tab write and the closed-tab stack. An
// adapter may take them and land them later, answering whether it took the
// write, provided every later call sees them landed; `ThreadedSessionStore`
// does. The writes named `save`, and the deletes, answer once they are
// written, which a Space switch, move or delete has to know before the
// interface moves on.
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
    // Whether this adapter makes durable browser state available to features
    // outside the live window. A Private session answers false, which keeps a
    // caller from reaching around its write-nothing rule through global files.
    virtual bool recordsState() const = 0;

    virtual QVector<SpaceState> loadSpaces() const = 0;
    virtual bool saveSpace(const SpaceState &space) = 0;
    virtual bool saveSpaces(const QVector<SpaceState> &spaces) = 0;
    virtual bool setActiveSpace(const QString &spaceId) = 0;
    virtual bool spaceHasSavedContent(const QString &spaceId) const = 0;
    virtual bool deleteSpace(const QString &spaceId, const QString &replacementActiveSpaceId = {})
        = 0;

    // The Spaces an Agent created (ADR 0051), each with the name of the
    // connection that created it. The label is kept beside the Space records
    // rather than in them, so nothing that copies a SpaceState, Sync included,
    // can carry it to another machine. Deleting a Space takes its label with
    // it.
    virtual QHash<QString, QString> agentSpaces() const = 0;
    // The Agent Spaces made to last only as long as their connection. Any
    // still here when a browser starts outlived one that crashed.
    virtual QStringList temporaryAgentSpaceIds() const = 0;
    virtual bool saveAgentSpace(const QString &spaceId, const QString &creator, bool temporary) = 0;
    virtual bool forgetAgentSpace(const QString &spaceId) = 0;

    virtual QVector<TabState> loadTabs(const QString &spaceId) const = 0;
    virtual QVector<TabState> loadClosedTabs(const QString &spaceId) const = 0;
    virtual bool recordClosedTabs(const QString &spaceId, const QVector<TabState> &tabs) = 0;
    virtual bool saveTab(const TabState &tab, int position) = 0;
    virtual bool saveTabs(
        const QString &spaceId, const QVector<TabState> &tabs, const QString &activeTabId) = 0;
    // The same write as `saveTabs`, for the coalesced path that has nothing to
    // do with the answer.
    virtual bool recordTabs(
        const QString &spaceId, const QVector<TabState> &tabs, const QString &activeTabId) = 0;
    virtual bool saveSpaceMove(const QString &sourceSpaceId, const QVector<TabState> &sourceTabs,
        const QString &sourceActiveTabId, const QString &destinationSpaceId,
        const QVector<TabState> &destinationTabs, const QString &destinationActiveTabId) = 0;

    virtual QString preference(const QString &name, const QString &fallback = {}) const = 0;
    virtual bool savePreference(const QString &name, const QString &value) = 0;

    virtual bool recordVisit(const QString &spaceId, const QUrl &url, const QString &title) = 0;
    virtual QVariantList history(const QString &spaceId, const QString &query, int limit) const = 0;
    // Deleting History also deletes the stored favicons of the pages it names,
    // except one a tab in that Space's sidebar still shows.
    virtual bool deleteHistoryVisit(const QString &spaceId, qint64 id) = 0;
    virtual bool deleteHistoryOrigin(const QString &spaceId, const QString &origin) = 0;
    virtual bool deleteHistorySince(const QString &spaceId, qint64 since) = 0;

    // The favicon a page in a Space showed, as encoded image bytes, kept by
    // the page's address with its origin beside it. A newer icon for the same
    // address replaces the older one. Only a web page, an address with an
    // origin, has one to keep.
    virtual bool recordFavicon(const QString &spaceId, const QUrl &pageUrl, const QByteArray &image)
        = 0;
    // Answers the favicon stored for the address, or else the newest one
    // stored for its origin, or empty bytes. The answer comes on the thread
    // the adapter reads on: in place for an adapter the caller's thread owns,
    // later and on the store's own thread for `ThreadedSessionStore`, so a
    // caller that must not wait on the disk is never made to.
    virtual void findFavicon(const QString &spaceId, const QUrl &pageUrl,
        std::function<void(const QByteArray &image)> answer) const = 0;

    virtual int permissionDecision(
        const QString &spaceId, const QString &origin, const QString &permission) const = 0;
    virtual bool savePermissionDecision(
        const QString &spaceId, const QString &origin, const QString &permission, int decision) = 0;
    virtual QVariantList permissionsForOrigin(const QString &spaceId, const QString &origin) const
        = 0;
    virtual bool clearPermissionsForOrigin(const QString &spaceId, const QString &origin) = 0;
    virtual bool clearPermissionsSince(const QString &spaceId, qint64 since) = 0;

    // `spaceId` is the Space the download came from, or nothing for a file
    // Omaweb wrote itself, so a temporary Space's records can go with it.
    virtual bool recordDownload(const QString &id, const QString &spaceId, const QUrl &url,
        const QString &path, const QString &state, qint64 receivedBytes, qint64 totalBytes) = 0;
    virtual bool updateDownload(const QString &id, const QString &state, qint64 receivedBytes,
        qint64 totalBytes, const QString &error) = 0;
    virtual QVariantList downloadHistory() const = 0;
    virtual bool forgetDownload(const QString &id) = 0;
    virtual bool forgetSpaceDownloads(const QString &spaceId) = 0;

protected:
    SessionStore() = default;
};

} // namespace omaweb
