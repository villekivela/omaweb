#pragma once

#include "RetainedTab.h"
#include "SessionStore.h"
#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QObject>
#include <QHash>
#include <QSet>
#include <QSharedPointer>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

namespace omaweb {

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
    // Whether the Pinned tab on show keeps its page running while another
    // Space is active. A pin never implies it and an ordinary tab cannot
    // carry it, so this is false for every tab the reader has not asked for.
    Q_PROPERTY(bool activeTabKeepActive READ activeTabKeepActive NOTIFY activeTabChanged)
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
    // How many tabs this Space can still take back. The stack is the Space's
    // own, holds its most recent closes, and survives a restart; a Private
    // session keeps the same depth in memory and writes none of it down.
    Q_PROPERTY(int closedTabCount READ closedTabCount NOTIFY closedTabsChanged)
    // Every tab still running in a Space that is not the one on show: a Pinned
    // tab the reader marked Keep active, or the tab an inspector is attached
    // to. Nothing else outlives its Space's suspension, and what does is named
    // here so the reader can see what is holding a renderer they cannot see.
    Q_PROPERTY(QVariantList retainedTabs READ retainedTabs NOTIFY retainedTabsChanged)
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

    // What Omaweb will do with an answer about a capability, which is not the
    // same question as what the answer was. A capability whose use the reader
    // cannot see being spent gets no memory at all, and one outside the
    // daily-driver contract is refused without a question being asked.
    enum PermissionPolicy {
        RefusedPermission = 0,
        ApprovedEachTime = 1,
        RememberablePermission = 2,
    };
    Q_ENUM(PermissionPolicy)

    BrowserController(QString dataRoot, QString engineName, QObject *parent = nullptr);
    BrowserController(QString dataRoot, QString engineName, QString configRoot,
        QObject *parent = nullptr);
    BrowserController(QString dataRoot, QString engineName, bool privateBrowsing,
        QObject *parent = nullptr);
    BrowserController(QString dataRoot, QString engineName, bool privateBrowsing,
        QSharedPointer<QHash<QString, int>> sessionPermissionDecisions,
        QObject *parent = nullptr);
    BrowserController(QString dataRoot, QString engineName, bool privateBrowsing,
        QSharedPointer<QHash<QString, int>> sessionPermissionDecisions,
        QString configRoot, QObject *parent = nullptr);
    // The third-party-cookie allowances a shared private session hands round
    // its windows, beside the Site permissions it already shares. Both tables
    // live in memory for exactly as long as that session does.
    BrowserController(QString dataRoot, QString engineName, bool privateBrowsing,
        QSharedPointer<QHash<QString, int>> sessionPermissionDecisions,
        QSharedPointer<QHash<QString, QString>> sessionCookieAllowances,
        QString configRoot, QObject *parent = nullptr);

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
    Q_INVOKABLE QString profilePathForSpace(const QString &spaceId) const;
    bool activeTabPinned() const;
    bool activeTabKeepActive() const;
    int closedTabCount() const;
    QVariantList retainedTabs() const;
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
    Q_INVOKABLE bool retryActiveUrlInsecurely();
    Q_INVOKABLE void closeTab(const QString &tabId);
    Q_INVOKABLE void closeActiveTab();
    // The two closes a row can ask for on its neighbours. Both mean the
    // ordinary tabs and only those: a Pinned tab is the Space's furniture and
    // is never swept away by a command aimed at the list below it.
    Q_INVOKABLE void closeOtherTabs(const QString &tabId);
    Q_INVOKABLE void closeTabsBelow(const QString &tabId);
    Q_INVOKABLE void reopenClosedTab();
    // A new ordinary tab at the same address. Duplicate copies the
    // destination and nothing else: no history to step back through, no form
    // state, and no share of the page the original is running.
    Q_INVOKABLE QString duplicateTab(const QString &tabId);
    // Order is the reader's, and it is theirs within one section: a Pinned tab
    // moves among the pins and an ordinary tab among the ordinary rows.
    // Crossing between them is what pinning is for, so the destination is
    // counted inside the tab's own section and a move that would leave it is
    // refused. Both are written through immediately — an arrangement the
    // reader made should not be waiting in a coalescing window at a quit.
    Q_INVOKABLE bool moveTab(const QString &tabId, int destinationIndex);
    Q_INVOKABLE bool moveTabBy(const QString &tabId, int offset);
    Q_INVOKABLE int tabSectionIndex(const QString &tabId) const;
    Q_INVOKABLE int tabSectionCount(const QString &tabId) const;
    Q_INVOKABLE void toggleActivePinned();
    // Keep active belongs to one Pinned tab and survives restart. Asking it of
    // an ordinary tab is refused rather than remembered: an ordinary tab
    // belongs to the session the reader is looking at.
    Q_INVOKABLE bool setTabKeepActive(const QString &tabId, bool keepActive);
    // Asked of one tab rather than read off the model by role number: a menu
    // is about the row it was opened on, which is not always the tab on show.
    Q_INVOKABLE bool tabPinned(const QString &tabId) const;
    Q_INVOKABLE bool tabKeepActive(const QString &tabId) const;
    Q_INVOKABLE bool toggleActiveKeepActive();
    // Stopping one retained tab from the list that names them. The tab is in a
    // Space that is not on show, so its Space's store is where the setting
    // lives rather than the tab model, which holds one Space at a time.
    Q_INVOKABLE bool releaseRetainedTab(const QString &tabId);
    // The tabs of the Space on show that will keep running once it is put
    // away. The interface hands this to the engine host at suspension, which
    // is the only moment the answer is about a Space that is still active.
    Q_INVOKABLE QStringList retainedTabIds() const;
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
    // Who a notification is for. A page may notify while the reader is looking
    // at its Space, and otherwise only from a tab that is retained; anything
    // else is a page whose Space was put away and has no business interrupting.
    // The origin names the tab because a notification arrives from a Space's
    // profile rather than from one page.
    Q_INVOKABLE QVariantMap notificationTarget(const QString &spaceId,
        const QUrl &origin) const;
    Q_INVOKABLE bool activateNotificationTarget(const QString &spaceId, const QString &tabId);
    // Audible autoplay waits for the reader to have dealt with the origin
    // themselves. The memory is the session's and one Space's: another Space
    // is another browsing identity, and nothing here reaches across a restart.
    Q_INVOKABLE void recordOriginInteraction(const QUrl &url);
    Q_INVOKABLE bool originInteracted(const QUrl &url) const;
    Q_INVOKABLE bool tabSoundSuppressed(const QString &tabId) const;
    // The reader asking a row for its sound. That is the reader dealing with
    // the origin — the same answer as touching the page — so it is recorded as
    // one rather than becoming a muting decision of its own.
    Q_INVOKABLE void grantTabSound(const QString &tabId);
    Q_INVOKABLE void recordVisit(const QUrl &url, const QString &title);
    Q_INVOKABLE QVariantList historySuggestions(const QString &query, int limit = 8) const;
    Q_INVOKABLE QVariantList history(const QString &query, int limit = 500) const;
    Q_INVOKABLE bool deleteHistoryVisit(qint64 id);
    Q_INVOKABLE bool deleteHistoryOrigin(const QUrl &url);
    Q_INVOKABLE bool deleteHistorySince(qint64 since);
    Q_INVOKABLE QVariantList searchEngines() const;
    Q_INVOKABLE QVariantList searchEnginePresets() const;
    Q_INVOKABLE bool addSearchEnginePreset(const QString &id);
    Q_INVOKABLE bool addSearchEngine(const QString &name, const QString &queryUrl,
        const QString &keyword = {});
    Q_INVOKABLE bool deleteSearchEngine(const QString &id);
    Q_INVOKABLE bool setDefaultSearchEngine(const QString &id);
    Q_INVOKABLE bool clearBrowsingData(const QStringList &dataTypes, qint64 since,
        bool everySpace = false, const QString &confirmation = {});
    Q_INVOKABLE int permissionDecision(const QUrl &url, const QString &permission);
    Q_INVOKABLE bool setPermissionDecision(const QUrl &url, const QString &permission,
        int decision);
    Q_INVOKABLE int permissionPolicy(const QString &permission) const;
    // What Site information reads and what its reset action does: one origin's
    // decisions inside the Space on show, and nothing beyond it.
    Q_INVOKABLE QVariantList sitePermissions(const QUrl &url) const;
    Q_INVOKABLE bool resetSitePermissions(const QUrl &url);
    // A certificate failure blocks. Whether Omaweb will even offer the reader
    // an exception is this: the engine's own facts about the failure, and
    // whether the address is a Local-development site's own main frame.
    // Nothing here records an answer — there is no remembered exception.
    Q_INVOKABLE bool mayOfferCertificateException(const QUrl &url, bool overridable,
        bool mainFrame, bool fatal) const;
    Q_INVOKABLE bool localDevelopmentSite(const QUrl &url) const;
    // Third-party cookies are blocked. An authentication or payment flow may be
    // given one origin's allowance in one Space: temporary, listed in Site
    // information, and revocable there.
    Q_INVOKABLE bool thirdPartyCookiesAllowed(const QString &spaceId, const QUrl &origin) const;
    Q_INVOKABLE bool allowThirdPartyCookies(const QUrl &origin, const QString &purpose);
    Q_INVOKABLE bool revokeThirdPartyCookieAllowance(const QUrl &origin);
    Q_INVOKABLE QVariantList thirdPartyCookieAllowances() const;
    // Read by the engine adapter enforcing the blocking, for a Space that is
    // not necessarily the one on show: a retained tab's profile is asked about
    // its own Space.
    QStringList allowedThirdPartyCookieOrigins(const QString &spaceId) const;
    // How much site data the engine is holding for one Space, in bytes, or -1
    // where there is nothing on disk to measure. Site information shows it only
    // where the adapter claims a persistent profile.
    Q_INVOKABLE double siteDataBytes(const QString &spaceId) const;
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
    void closedTabsChanged();
    void retainedTabsChanged();
    // The Space being put away, and the tabs inside it that keep running
    // anyway. Named together because the exceptions are only knowable while
    // that Space is still the active one.
    void spaceSuspended(const QString &spaceId, const QStringList &retainedTabIds);
    void spaceRestored(const QString &spaceId);
    void spaceDiscarded(const QString &spaceId);
    void tabMoveConfirmationRequested(const QString &tabId, const QString &destinationSpaceId);
    void backRequested();
    void forwardRequested();
    void reloadRequested();
    void reloadBypassingCacheRequested();
    void stopLoadingRequested();
    void closeWindowRequested();
    void engineDataClearRequested(const QStringList &spaceIds,
        const QStringList &dataTypes, qint64 since);
    void thirdPartyCookieAllowancesChanged();

private:
    void initialize();
    void ensureDefaultSpace();
    void ensureActiveTab();
    bool persistTabs();
    void schedulePersistTabs();
    void setActiveTab(const QString &tabId);
    qsizetype pinnedTabCount() const;
    qsizetype tabRow(const QString &tabId) const;
    static bool retains(const TabState &tab, const QString &developerToolsTabId);
    bool suppressesSound(const TabState &tab) const;
    void refreshSoundSuppression();
    void rememberClosedTab(const TabState &tab);
    void loadClosedTabs();
    void persistClosedTabs();
    void refreshRetainedTabs();
    const RetainedTab *findRetainedTab(const QString &tabId) const;
    QString originInteractionKey(const QUrl &url) const;
    static TabState makeBlankTab(const QString &spaceId);
    static double steppedZoom(double zoom, int direction);
    static bool isBlank(const QUrl &url);
    bool restingOnBlankTab() const;
    void refreshAtRest();
    void setDeveloperToolsTab(const QString &tabId, const QString &spaceId);
    static QUrl resolveInput(const QString &input);
    QUrl resolveConfiguredInput(const QString &input) const;
    bool loadSearchEngines();
    bool saveSearchEngines(const QVariantList &engines, const QString &defaultEngineId);
    static QString normalizedOrigin(const QUrl &url);
    QString sessionPermissionKey(const QString &origin, const QString &permission) const;
    static QString cookieAllowanceKey(const QString &spaceId, const QString &origin);
    static bool localDevelopmentHost(const QString &host);

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
    QString m_configRoot;
    QVariantList m_searchEngines;
    QString m_defaultSearchEngineId;
    QString m_errorMessage;
    QVector<TabState> m_closedTabs;
    QVector<RetainedTab> m_retainedTabs;
    QSet<QString> m_interactedOrigins;
    bool m_ready = false;
    bool m_atRest = false;
    bool m_privateBrowsing = false;
    QSharedPointer<QHash<QString, int>> m_sessionPermissionDecisions;
    QSharedPointer<QHash<QString, QString>> m_sessionCookieAllowances;
};

} // namespace omaweb
