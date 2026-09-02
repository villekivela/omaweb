#pragma once

#include "SessionStore.h"
#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QObject>
#include <QHash>
#include <QSharedPointer>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

namespace tanto {

class BrowserController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *spaces READ spaces CONSTANT)
    Q_PROPERTY(QAbstractItemModel *tabs READ tabs CONSTANT)
    Q_PROPERTY(QAbstractItemModel *pinnedTabs READ pinnedTabs CONSTANT)
    Q_PROPERTY(QAbstractItemModel *unpinnedTabs READ unpinnedTabs CONSTANT)
    Q_PROPERTY(QString activeSpaceId READ activeSpaceId NOTIFY activeSpaceChanged)
    Q_PROPERTY(QString activeSpaceName READ activeSpaceName NOTIFY activeSpaceChanged)
    Q_PROPERTY(QString activeTabId READ activeTabId NOTIFY activeTabChanged)
    Q_PROPERTY(QUrl activeUrl READ activeUrl NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeProfilePath READ activeProfilePath NOTIFY activeSpaceChanged)
    Q_PROPERTY(bool activeTabPinned READ activeTabPinned NOTIFY activeTabChanged)
    // How large the page in the tab on show is drawn, as a factor: 1.0 is 100
    // percent. Zoom belongs to the tab rather than to the site, so nothing here
    // is keyed by origin, and it is kept in the session so a restored tab comes
    // back at the size it was left.
    Q_PROPERTY(double activeTabZoom READ activeTabZoom NOTIFY activeTabChanged)
    // Whether the tab on show has an address to load. A blank one has no page
    // to draw and nothing to draw it with, whether it is the Space resting or
    // an address the reader typed, so the interface stands something else in
    // its place rather than showing an empty viewport.
    Q_PROPERTY(bool activeTabBlank READ activeTabBlank NOTIFY activeTabChanged)
    // A Space is at rest when its only ordinary tab is blank: nothing has been
    // opened in it, or the last page in it has been closed. The interface has
    // no page to show then and no ordinary tab to list, so both read this
    // rather than each deciding what counts as blank for itself.
    Q_PROPERTY(bool atRest READ atRest NOTIFY atRestChanged)
    // The one tab the engine's inspector is attached to, and whether the tab on
    // show is that tab. Attachment lives in memory only: Developer tools never
    // come back after a restart, and nothing about them is written to a session.
    Q_PROPERTY(QString developerToolsTabId READ developerToolsTabId
        NOTIFY developerToolsChanged)
    Q_PROPERTY(bool activeTabInspected READ activeTabInspected NOTIFY activeTabChanged)
    Q_PROPERTY(bool activeRendererFailed READ activeRendererFailed NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeRendererFailureReason READ activeRendererFailureReason NOTIFY activeTabChanged)
    Q_PROPERTY(bool privateBrowsing READ privateBrowsing CONSTANT)
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(QString errorMessage READ errorMessage CONSTANT)
    Q_PROPERTY(QString downloadDirectory READ downloadDirectory CONSTANT)
    Q_PROPERTY(bool acceptDownloads READ acceptDownloads CONSTANT)

public:
    enum PermissionDecision {
        Ask = 0,
        AllowOnce = 1,
        AllowPersistently = 2,
        Block = 3,
    };
    Q_ENUM(PermissionDecision)

    BrowserController(QString dataRoot, QString engineName, QObject *parent = nullptr);
    BrowserController(QString dataRoot, QString engineName, bool privateBrowsing,
        QObject *parent = nullptr);
    BrowserController(QString dataRoot, QString engineName, bool privateBrowsing,
        QSharedPointer<QHash<QString, int>> sessionPermissionDecisions,
        QObject *parent = nullptr);

    ~BrowserController() override;

    QAbstractItemModel *spaces();
    QAbstractItemModel *tabs();
    QAbstractItemModel *pinnedTabs();
    QAbstractItemModel *unpinnedTabs();
    QString activeSpaceId() const;
    QString activeSpaceName() const;
    QString activeTabId() const;
    QUrl activeUrl() const;
    QString activeTitle() const;
    QString activeProfilePath() const;
    bool activeTabPinned() const;
    double activeTabZoom() const;
    bool activeTabBlank() const;
    bool atRest() const;
    QString developerToolsTabId() const;
    bool activeTabInspected() const;
    bool activeRendererFailed() const;
    QString activeRendererFailureReason() const;
    bool privateBrowsing() const;
    bool ready() const;
    QString errorMessage() const;
    QString downloadDirectory() const;
    bool acceptDownloads() const;

    Q_INVOKABLE void activateTab(const QString &tabId);
    Q_INVOKABLE QString createSpace(const QString &name);
    Q_INVOKABLE bool switchSpace(const QString &spaceId);
    Q_INVOKABLE bool renameSpace(const QString &spaceId, const QString &name);
    Q_INVOKABLE bool deleteSpace(const QString &spaceId, const QString &confirmationName);
    Q_INVOKABLE bool requestTabMoveToSpace(const QString &tabId, const QString &destinationSpaceId,
        bool hasEditedFormState);
    Q_INVOKABLE bool confirmTabMoveToSpace(const QString &tabId, const QString &destinationSpaceId);
    Q_INVOKABLE void openInput(const QString &input, bool inNewTab);
    Q_INVOKABLE void openInputInBackground(const QUrl &url);
    Q_INVOKABLE void closeTab(const QString &tabId);
    Q_INVOKABLE void closeActiveTab();
    Q_INVOKABLE void reopenClosedTab();
    Q_INVOKABLE void toggleActivePinned();
    Q_INVOKABLE void updateTab(const QString &tabId, const QUrl &url, const QString &title);
    Q_INVOKABLE void setTabLoading(const QString &tabId, bool loading);
    Q_INVOKABLE void setTabIcon(const QString &tabId, const QUrl &iconUrl);
    Q_INVOKABLE void setTabAudible(const QString &tabId, bool audible);
    Q_INVOKABLE void setTabMuted(const QString &tabId, bool muted);
    Q_INVOKABLE void toggleTabMuted(const QString &tabId);
    // Zoom moves along a fixed ladder rather than by a percentage, so every
    // step lands on a size the reader has seen before and the ends are bounded.
    Q_INVOKABLE void setTabZoom(const QString &tabId, double zoom);
    Q_INVOKABLE void stepActiveZoom(int direction);
    Q_INVOKABLE void resetActiveZoom();
    Q_INVOKABLE void reportTabRendererFailure(const QString &tabId, const QString &reason);
    Q_INVOKABLE void recoverActiveTab();
    // One inspector inspects one tab. Asking for it on another tab moves it
    // there rather than opening a second one.
    Q_INVOKABLE void openDeveloperTools();
    Q_INVOKABLE void toggleDeveloperTools();
    Q_INVOKABLE void closeDeveloperTools();
    Q_INVOKABLE void requestBack();
    Q_INVOKABLE void requestForward();
    Q_INVOKABLE void requestReload();
    // Three separate asks, because they mean three different things to a page:
    // read it again, read it again from the network, and stop reading it.
    Q_INVOKABLE void requestReloadBypassingCache();
    Q_INVOKABLE void requestStopLoading();
    Q_INVOKABLE void recordVisit(const QUrl &url, const QString &title);
    Q_INVOKABLE QVariantList historySuggestions(const QString &query, int limit = 8) const;
    Q_INVOKABLE int permissionDecision(const QUrl &url, const QString &permission);
    Q_INVOKABLE bool setPermissionDecision(const QUrl &url, const QString &permission,
        int decision);
    Q_INVOKABLE bool externalProtocolAllowed(const QUrl &origin, const QString &scheme) const;
    Q_INVOKABLE bool rememberExternalProtocolDecision(const QUrl &origin,
        const QString &scheme);
    Q_INVOKABLE QString recordDownload(const QString &runtimeId, const QUrl &url, const QString &path,
        const QString &state, qint64 receivedBytes, qint64 totalBytes);
    Q_INVOKABLE bool updateDownload(const QString &id, const QString &state,
        qint64 receivedBytes, qint64 totalBytes, const QString &error);
    Q_INVOKABLE QVariantList downloadHistory() const;
    Q_INVOKABLE QString preference(const QString &name, const QString &fallback = {}) const;
    Q_INVOKABLE bool setPreference(const QString &name, const QString &value);

signals:
    void activeSpaceChanged();
    void activeTabChanged();
    void atRestChanged();
    void developerToolsChanged();
    void spaceSuspended(const QString &spaceId);
    void spaceRestored(const QString &spaceId);
    void spaceDiscarded(const QString &spaceId);
    void tabMoveConfirmationRequested(const QString &tabId, const QString &destinationSpaceId);
    void backRequested();
    void forwardRequested();
    void reloadRequested();
    void reloadBypassingCacheRequested();
    void stopLoadingRequested();
    void closeWindowRequested();

private:
    struct ClosedTab {
        TabState tab;
        bool valid = false;
    };

    void initialize();
    void ensureDefaultSpace();
    void ensureActiveTab();
    bool persistTabs();
    void schedulePersistTabs();
    void setActiveTab(const QString &tabId);
    static TabState makeBlankTab(const QString &spaceId);
    static double steppedZoom(double zoom, int direction);
    static bool isBlank(const QUrl &url);
    bool restingOnBlankTab() const;
    void refreshAtRest();
    void setDeveloperToolsTab(const QString &tabId, const QString &spaceId);
    static QUrl resolveInput(const QString &input);
    static QString normalizedOrigin(const QUrl &url);
    QString sessionPermissionKey(const QString &origin, const QString &permission) const;

    SessionStore m_store;
    QTimer m_persistTabsTimer;
    SpaceListModel m_spaces;
    TabListModel m_tabs;
    QSortFilterProxyModel m_pinnedTabs;
    QSortFilterProxyModel m_unpinnedTabs;
    QString m_activeSpaceId;
    QString m_activeSpaceName;
    QString m_activeTabId;
    QString m_developerToolsTabId;
    // The Space the inspected tab belongs to, so deleting that Space takes the
    // attachment with it: the tab is gone from the store, and while another
    // Space is active it is not in the tab model to be noticed missing.
    QString m_developerToolsSpaceId;
    QString m_engineName;
    QString m_errorMessage;
    ClosedTab m_closedTab;
    bool m_ready = false;
    bool m_atRest = false;
    bool m_privateBrowsing = false;
    QSharedPointer<QHash<QString, int>> m_sessionPermissionDecisions;
};

} // namespace tanto
