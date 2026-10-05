#pragma once

#include "Downloads.h"
#include "PutAwayTab.h"
#include "RetainedTab.h"
#include "SessionSiteState.h"
#include "SessionStore.h"
#include "ExtensionInstaller.h"
#include "SpaceListModel.h"
#include "SpaceProject.h"
#include "SpaceStorage.h"
#include "TabListModel.h"
#include "WindowCapabilities.h"

#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QHash>
#include <QSet>
#include <QSharedPointer>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <memory>
#include <optional>
#include <utility>

namespace omaweb {

class AddressWatch;
class EngineSuggestions;
class HistorySearch;
class PaymentCards;
class ThreadedSessionStore;

// A window implements DownloadPermissions so its downloads can ask what an
// origin has been allowed without holding the window's Site permissions
// themselves.
class BrowserController final : public QObject, public DownloadPermissions {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *spaces READ spaces CONSTANT)
    Q_PROPERTY(QAbstractItemModel *tabs READ tabs CONSTANT)
    Q_PROPERTY(QAbstractItemModel *pinnedTabs READ pinnedTabs CONSTANT)
    Q_PROPERTY(QAbstractItemModel *unpinnedTabs READ unpinnedTabs CONSTANT)
    Q_PROPERTY(QString activeSpaceId READ activeSpaceId NOTIFY activeSpaceChanged)
    Q_PROPERTY(QString activeSpaceName READ activeSpaceName NOTIFY activeSpaceChanged)
    // The Space that this session's browsing state keys on: the Space on show,
    // or the empty name a Private window uses because its shared session has
    // no Space of its own. Answered here so that the chrome, which reads
    // Content blocking's Refusal tally for the address on show, does not work
    // the Private window's exception out again in each place it asks.
    Q_PROPERTY(QString sessionSpaceId READ sessionSpaceId NOTIFY activeSpaceChanged)
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
    // Each project's Space, by Space id, as its `directory`, `address`,
    // `agentCommand`, and whether the directory is `present` on this machine.
    Q_PROPERTY(QVariantMap spaceProjects READ spaceProjectRows NOTIFY spaceProjectsChanged)
    // The Space on show waits for its project's address to answer, and the
    // interface shows the road driving until it does.
    Q_PROPERTY(
        bool activeSpaceAwaitsAddress READ activeSpaceAwaitsAddress NOTIFY addressAwaitChanged)
    // The one tab the engine's inspector is attached to, and whether the tab on
    // show is that tab. Attachment lives in memory only: Developer tools never
    // come back after a restart, and nothing about them is written to a session.
    Q_PROPERTY(QString developerToolsTabId READ developerToolsTabId NOTIFY developerToolsChanged)
    Q_PROPERTY(bool activeTabInspected READ activeTabInspected NOTIFY activeTabChanged)
    // The split on show: the active tab's split, as its left and right tab and
    // the tab beside, or nothing while the active tab is not in one. A split
    // the reader has left for another tab is still in the tab model, as two
    // tabs naming each other, and is not answered here: the interface draws
    // that from the rows and the page area draws this.
    Q_PROPERTY(QString splitLeftTabId READ splitLeftTabId NOTIFY splitChanged)
    Q_PROPERTY(QString splitRightTabId READ splitRightTabId NOTIFY splitChanged)
    Q_PROPERTY(QString tabBesideId READ tabBesideId NOTIFY splitChanged)
    Q_PROPERTY(bool splitOnShow READ splitOnShow NOTIFY splitChanged)
    Q_PROPERTY(bool activeRendererFailed READ activeRendererFailed NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeRendererFailureReason READ activeRendererFailureReason NOTIFY
            activeTabChanged)
    // How many tabs this Space can still take back. The stack is the Space's
    // own, holds its most recent closes, and survives a restart; a Private
    // session keeps the same depth in memory and writes none of it down.
    Q_PROPERTY(int closedTabCount READ closedTabCount NOTIFY closedTabsChanged)
    // The tabs Omaweb put away in the Space on show because the reader had not
    // shown them for longer than the setting allows, newest first, each as
    // its `id`, `url`, `host`, `title` and `putAwayAt` in milliseconds since
    // the epoch. Kept for 30 days, and never in a Private window.
    Q_PROPERTY(QVariantList putAwayTabs READ putAwayTabs NOTIFY putAwayTabsChanged)
    // How long an ordinary tab may go unshown before it is put away, in
    // seconds: 0 for never, or an hour, 12 hours, a day or a week. Kept with
    // this installation, outside the Sync projection. 12 hours by default.
    Q_PROPERTY(int putAwayAfterSeconds READ putAwayAfterSeconds NOTIFY putAwayAfterChanged)
    // Whether the notice that Omaweb puts unused tabs away waits for the
    // reader. It is raised by the first put-away of the installation, once,
    // and never again once dismissed.
    Q_PROPERTY(bool putAwayNotice READ putAwayNotice NOTIFY putAwayNoticeChanged)
    // Every tab still running in a Space that is not the one on show: a Pinned
    // tab the reader marked Keep active, or the tab an inspector is attached
    // to. Nothing else outlives its Space's suspension, and what does is named
    // here so the reader can see what is holding a renderer they cannot see.
    Q_PROPERTY(QVariantList retainedTabs READ retainedTabs NOTIFY retainedTabsChanged)
    // The address of the Agent activity page, which the interface draws itself
    // and no engine loads.
    Q_PROPERTY(QUrl agentActivityAddress READ agentActivityAddress CONSTANT)
    Q_PROPERTY(bool privateBrowsing READ privateBrowsing CONSTANT)
    // Where the payment cards stand: "unavailable" in a Private window and on a
    // desktop with no secret store, "unread" until something needs them,
    // "reading", "ready", or "unreadable" when the keyring would not give them up.
    Q_PROPERTY(QString paymentCardsState READ paymentCardsState NOTIFY paymentCardsChanged)
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(QString errorMessage READ errorMessage CONSTANT)
    // The window's downloads, running and recorded. The download directory
    // above is a setting rather than part of this list, so it stays here.
    Q_PROPERTY(omaweb::Downloads *downloads READ downloads CONSTANT)
    Q_PROPERTY(QString downloadDirectory READ downloadDirectory NOTIFY downloadDirectoryChanged)
    Q_PROPERTY(bool acceptDownloads READ acceptDownloads CONSTANT)
    // Every Agent Space of this window, which the footer and the Space's
    // notice mark as an Agent's until the reader takes it over.
    Q_PROPERTY(QStringList agentSpaceIds READ agentSpaceIds NOTIFY agentSpacesChanged)

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
        Refused = 0,
        AskedEachTime = 1,
        Rememberable = 2,
    };
    Q_ENUM(PermissionPolicy)

    enum DownloadDisposition {
        AcceptDownload = 0,
        ConfirmDownload = 1,
        AskDownloadPermission = 2,
        RefuseDownload = 3,
        SaveDownloadAs = 4,
    };
    Q_ENUM(DownloadDisposition)

    // Where this window's Spaces live on disk, which is a data root and the
    // name of the engine reading it. A window built this way keeps what it
    // browses; the store-taking constructor below is how one that keeps
    // nothing is built.
    explicit BrowserController(
        SpaceStorage storage, QString configRoot = {}, QObject *parent = nullptr);
    // A Private window is handed the store its session already has, so what one
    // window agreed to is what the next one finds. Whether a window is private
    // stays a fact of its own: a store that keeps nothing is also how an
    // ordinary window could be built for a test. Either way the window has no
    // SpaceStorage, because nothing it browses is kept anywhere.
    BrowserController(std::shared_ptr<SessionStore> store, bool privateBrowsing,
        QSharedPointer<QHash<QString, int>> sessionPermissionDecisions,
        QSharedPointer<SessionSiteState> sessionSiteState, QString configRoot,
        QObject *parent = nullptr);

    ~BrowserController() override;

    QAbstractItemModel *spaces();
    QAbstractItemModel *tabs();
    QAbstractItemModel *pinnedTabs();
    QAbstractItemModel *unpinnedTabs();
    QString activeSpaceId() const;
    QString sessionSpaceId() const;
    QString activeSpaceName() const;
    QString activeTabId() const;
    QUrl activeUrl() const;
    QString activeTitle() const;
    // Where the active Space's Engine profile belongs, and where one Space's
    // does. Both only say where: a window with no SpaceStorage, and a Space
    // this window does not have, answer with nothing.
    QString activeProfilePath() const;
    Q_INVOKABLE QString profilePathForSpace(const QString &spaceId) const;
    // The same directory, made to exist. Called where an engine host is about
    // to be built over it, and answering with nothing when it cannot be
    // created — a host pointed at a directory that is not there would find out
    // later and less clearly.
    Q_INVOKABLE QString prepareProfileForSpace(const QString &spaceId) const;
    bool activeTabPinned() const;
    bool activeTabKeepActive() const;
    int closedTabCount() const;
    QVariantList putAwayTabs() const;
    int putAwayAfterSeconds() const;
    bool putAwayNotice() const;
    QVariantList retainedTabs() const;
    double activeTabZoom() const;
    bool activeTabBlank() const;
    bool atRest() const;
    QString developerToolsTabId() const;
    bool activeTabInspected() const;
    QString splitLeftTabId() const;
    QString splitRightTabId() const;
    QString tabBesideId() const;
    bool splitOnShow() const;
    bool activeRendererFailed() const;
    QString activeRendererFailureReason() const;
    bool privateBrowsing() const;
    bool ready() const;
    QString errorMessage() const;
    Downloads *downloads() const;
    QString downloadDirectory() const;
    bool acceptDownloads() const;
    SessionStore *sessionStore() const;
    bool startedWithEmptyState() const;
    void reloadSyncedState();

    QString permissionOrigin(const QUrl &url) const override;
    int automaticDownloadDecision(const QString &origin) const override;
    bool rememberAutomaticDownloadDecision(const QString &origin, int decision) override;

    Q_INVOKABLE void activateTab(const QString &tabId);
    // The next or previous stop in the tab list, wrapping at either end. A
    // split is one stop, entered on the half the reader was last in.
    Q_INVOKABLE void stepTab(int delta);
    // Pairs the named tab with the active tab, or pairs a new blank tab with
    // it when none is named: the blank one is on the right and focused, so the
    // next address opened lands in it. Both must be ordinary tabs of the Space
    // on show and in no split yet. Answers whether it acted.
    Q_INVOKABLE bool addSplit(const QString &tabId = {});
    // Ends the split the named tab is in, or the active tab's. Both tabs stay,
    // as two adjacent ordinary rows, and the active tab is shown alone.
    Q_INVOKABLE bool separateSplit(const QString &tabId = {});
    // Moves focus to the tab beside, which makes it the active tab.
    Q_INVOKABLE bool focusSplitPartner();
    Q_INVOKABLE bool tabInSplit(const QString &tabId) const;
    // The ordinary tabs of the Space on show that a split could still take:
    // unpaired, and not the active tab. What the command scope's chooser lists.
    Q_INVOKABLE QStringList splittableTabIds() const;
    Q_INVOKABLE QString createSpace(const QString &name);
    Q_INVOKABLE bool switchSpace(const QString &spaceId);
    // The tabs of every Space but the one on show, in Space order, as the
    // session keeps them, so listing them wakes none of their pages. A Space
    // at rest has none to list, and a Private window has no other Space.
    Q_INVOKABLE QVariantList awaySpaceTabs() const;
    // Switches to the Space and selects its tab, or does neither: a tab the
    // Space does not hold is refused before the Space on show changes.
    Q_INVOKABLE bool activateTabInSpace(const QString &spaceId, const QString &tabId);
    Q_INVOKABLE bool renameSpace(const QString &spaceId, const QString &name);
    // One of spaceColourNames(), whichever other Space has it already.
    Q_INVOKABLE bool setSpaceColour(const QString &spaceId, const QString &colour);
    Q_INVOKABLE bool deleteSpace(const QString &spaceId, const QString &confirmationName);
    // The order Spaces are listed in is the reader's, like the order of tabs
    // within a Space, and it is a property of the Space records rather than of
    // what is open: a move writes the new positions through and leaves the
    // active Space, the active tab, and every Space's tabs where they were. A
    // move that would carry a Space past either end is refused.
    Q_INVOKABLE bool moveSpaceBy(const QString &spaceId, int offset);
    Q_INVOKABLE bool requestTabMoveToSpace(
        const QString &tabId, const QString &destinationSpaceId, bool hasEditedFormState);
    Q_INVOKABLE bool confirmTabMoveToSpace(const QString &tabId, const QString &destinationSpaceId);
    Q_INVOKABLE void openInput(const QString &input, bool inNewTab);
    Q_INVOKABLE void openInputInBackground(const QUrl &url);
    static QUrl agentActivityAddress();
    // The title a tab carries until its page names itself.
    static QString addressTitle(const QUrl &url);
    // Opens the Agent activity page in a new tab of the Space on show and
    // selects it. A Private window has no Agents and opens nothing.
    Q_INVOKABLE bool openAgentActivity();
    Q_INVOKABLE bool retryActiveUrlInsecurely();
    Q_INVOKABLE void closeTab(const QString &tabId);
    Q_INVOKABLE void closeActiveTab();
    // The two closes a row can ask for on its neighbours. Both mean the
    // ordinary tabs and only those: a Pinned tab is the Space's furniture and
    // is never swept away by a command aimed at the list below it.
    Q_INVOKABLE void closeOtherTabs(const QString &tabId);
    Q_INVOKABLE void closeTabsBelow(const QString &tabId);
    Q_INVOKABLE void reopenClosedTab();
    // Puts away every ordinary tab of every Space that has not been on show
    // for longer than the setting allows. Runs at startup, on a Space switch
    // and on a periodic check; asking for it is the same check.
    Q_INVOKABLE void putAwayUnusedTabs();
    // Opens one entry of the put-away list again, as reopening a closed tab
    // does: a new tab, selected, at its address, zoom and muting. The entry
    // leaves the list. Refuses an id the list does not hold.
    Q_INVOKABLE bool reopenPutAwayTab(const QString &id);
    // Refuses any value but the ones putAwayAfterSeconds names.
    Q_INVOKABLE bool setPutAwayAfterSeconds(int seconds);
    Q_INVOKABLE void dismissPutAwayNotice();
    // The time the put-away rule reads, in milliseconds since the epoch, or
    // 0 for the wall clock. Only a test sets it.
    Q_INVOKABLE void setNowForTests(qint64 milliseconds);
    // The row the reader is dragging in the sidebar, or nothing. A tab being
    // dragged is in use and is not put away.
    Q_INVOKABLE void setDraggedTab(const QString &tabId);
    // The tabs an Agent is attached to, which AgentControl keeps current. An
    // Agent works where the reader is not looking, so its tabs are not unused.
    void setAgentTabIds(const QStringList &tabIds);
    // How often a window checks for unused tabs. Only a test shortens it.
    void setPutAwayCheckIntervalForTests(int milliseconds);
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
    Q_INVOKABLE void reportTabPageState(const QString &tabId, const QUrl &url, const QString &title,
        const QUrl &iconUrl, bool loading, bool audible);
    void setTabLoading(const QString &tabId, bool loading);
    void setTabIcon(const QString &tabId, const QUrl &iconUrl);
    // Where the interface draws the favicon the Space on show stored for a
    // page, or its site, from. The address answers without the page loading
    // and without a network request; while the answer is pending, or when
    // nothing is stored, the interface draws the host code. Empty for an
    // address that is not a web page.
    Q_INVOKABLE QUrl storedFavicon(const QUrl &pageUrl) const;
    void setTabAudible(const QString &tabId, bool audible);
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
    // Who a notification is for. A page may notify while the reader is looking
    // at its Space, and otherwise only from a tab that is retained; anything
    // else is a page whose Space was put away and has no business interrupting.
    // The origin names the tab because a notification arrives from a Space's
    // profile rather than from one page.
    Q_INVOKABLE QVariantMap notificationTarget(const QString &spaceId, const QUrl &origin) const;
    Q_INVOKABLE bool activateNotificationTarget(const QString &spaceId, const QString &tabId);
    // Audible autoplay waits for the reader to have dealt with the origin
    // themselves. The memory is the session's and one Space's: another Space
    // is another browsing identity, and nothing here reaches across a restart.
    Q_INVOKABLE void recordOriginInteraction(const QUrl &url);
    Q_INVOKABLE bool originInteracted(const QUrl &url) const override;
    Q_INVOKABLE bool tabSoundSuppressed(const QString &tabId) const;
    // The reader asking a row for its sound. That is the reader dealing with
    // the origin — the same answer as touching the page — so it is recorded as
    // one rather than becoming a muting decision of its own.
    Q_INVOKABLE void grantTabSound(const QString &tabId);
    Q_INVOKABLE void recordVisit(const QUrl &url, const QString &title);
    // The Omnibar's search, answered off the GUI thread. Every keystroke may
    // ask, and only the answer to the latest request, for the Space still on
    // show, reaches historySuggestionsReady. One search runs and one waits, so
    // typing faster than the store answers cannot queue more work.
    Q_INVOKABLE void requestHistorySuggestions(const QString &query, int limit = 8);
    // Abandons what a running or waiting search would have answered: the
    // Omnibar has closed, the Space has changed, or the history it read has
    // been deleted.
    Q_INVOKABLE void cancelHistorySuggestions();
    // Holds every search open for this long. Only a test sets it, to have
    // input arrive while a search is still running.
    Q_INVOKABLE void setHistorySearchDelayForTests(int milliseconds);
    // The Engine suggestion setting this window reads. A window that is
    // never given one never asks.
    void setEngineSuggestions(EngineSuggestions *suggestions);
    // The Omnibar's Engine suggestions for `text`, as Return would search it.
    // Every keystroke may ask; a request goes out once typing pauses, and
    // only the answer to the latest reaches engineSuggestionsReady. Text that
    // has nothing to ask is answered at once with no suggestions.
    Q_INVOKABLE void requestEngineSuggestions(const QString &text);
    // Abandons what was asked and not yet answered: the Omnibar has closed or
    // left the text it asked about.
    Q_INVOKABLE void cancelEngineSuggestions();
    Q_INVOKABLE QVariantList history(const QString &query, int limit = 500) const;
    Q_INVOKABLE bool deleteHistoryVisit(qint64 id);
    Q_INVOKABLE bool deleteHistoryOrigin(const QUrl &url);
    Q_INVOKABLE bool deleteHistorySince(qint64 since);
    Q_INVOKABLE QVariantList searchEngines() const;
    // One configured engine by id, or an empty map.
    Q_INVOKABLE QVariantMap searchEngine(const QString &id) const;
    Q_INVOKABLE QVariantList searchEnginePresets() const;
    Q_INVOKABLE bool addSearchEnginePreset(const QString &id);
    // An empty suggest URL is an engine that offers no Engine suggestions.
    Q_INVOKABLE bool addSearchEngine(const QString &name, const QString &queryUrl,
        const QString &keyword = {}, const QString &suggestUrl = {});
    Q_INVOKABLE bool deleteSearchEngine(const QString &id);
    // The address that searches one engine for `terms`, fully encoded, or
    // empty for an engine that is not configured. Whatever the terms read
    // as, this is a search: an Engine suggestion that looks like an address
    // is still one.
    Q_INVOKABLE QString searchAddress(const QString &engineId, const QString &terms) const;
    Q_INVOKABLE bool setDefaultSearchEngine(const QString &id);
    // What committing `text` would search: `engineId`, `engineName`, the
    // `terms`, and the lowercased `keyword` that chose the engine, empty when
    // the default engine answers for text with no keyword, so a mistyped
    // keyword reads as the search it is. Empty when the text is an address or
    // blank. Terms that are empty mean the engine's front page.
    Q_INVOKABLE QVariantMap searchIntent(const QString &text) const;
    // The engines whose keyword begins with `text`, each as `engineId`,
    // `engineName` and `keyword`. Nothing for blank text, text with a space,
    // or the default engine, which plain text already searches.
    Q_INVOKABLE QVariantList searchKeywordOffers(const QString &text) const;
    // Form history. `spaceId` is the Space of the engine the form was in,
    // which is not always the one on show. Each field is a map of `name` and
    // `value`; a nameless field, a blank value and a value shaped like a card
    // number are never kept.
    Q_INVOKABLE void rememberFormFields(const QString &spaceId, const QVariantList &fields);
    // The values kept for a field's name, the one used last first.
    Q_INVOKABLE QStringList formHistory(const QString &spaceId, const QString &field) const;
    Q_INVOKABLE bool forgetFormEntry(
        const QString &spaceId, const QString &field, const QString &value);
    // Addresses: the reader's own, offered in every Space and never in a
    // Private window, whose store keeps none. Each is a map of `id`, `name`,
    // `street`, `postalCode`, `city`, `country`, `phone` and `email`, in the
    // order they were added.
    Q_INVOKABLE QVariantList addresses() const;
    // Adds the address, or edits the one its `id` names, and answers its id,
    // or nothing when it was not kept: an address needs a name.
    Q_INVOKABLE QString saveAddress(const QVariantMap &address);
    Q_INVOKABLE bool removeAddress(const QString &id);
    // Payment cards: the reader's, kept in the desktop's keyring (ADR 0053),
    // offered in every Space and never in a Private window, which is never
    // given them. A window that is given none has none.
    void setPaymentCards(PaymentCards *cards);
    QString paymentCardsState() const;
    // Each card by everything but its number, as PaymentCards::cards
    // describes; asking reads them from the keyring the first time.
    Q_INVOKABLE QVariantList paymentCards();
    // Asks the keyring again after the reader left it locked.
    Q_INVOKABLE void readPaymentCardsAgain();
    // Adds the card, or edits the one its `id` names, and answers its id, or
    // nothing when it was not kept, as PaymentCards::save describes.
    Q_INVOKABLE QString savePaymentCard(const QVariantMap &card);
    Q_INVOKABLE bool removePaymentCard(const QString &id);
    // The card with its `number`, for the fill the reader picked and nothing
    // else.
    Q_INVOKABLE QVariantMap paymentCardForFill(const QString &id) const;
    Q_INVOKABLE bool paymentCardSaved(const QString &number) const;
    // Whether the text is a card number one could be saved under: 12 to 19
    // digits, spaces and dashes aside, that pass the Luhn check.
    Q_INVOKABLE bool isPaymentCardNumber(const QString &number) const;
    Q_INVOKABLE bool clearBrowsingData(const QStringList &dataTypes, qint64 since,
        bool everySpace = false, const QString &confirmation = {});
    Q_INVOKABLE int permissionDecision(const QUrl &url, const QString &permission);
    Q_INVOKABLE bool setPermissionDecision(
        const QUrl &url, const QString &permission, int decision);
    Q_INVOKABLE int permissionPolicy(const QString &permission) const;
    // What Site information reads and what its reset action does: one origin's
    // decisions inside the Space on show, and nothing beyond it.
    Q_INVOKABLE QVariantList sitePermissions(const QUrl &url) const;
    Q_INVOKABLE bool resetSitePermissions(const QUrl &url);
    // Site information's own answer for one of those decisions: Ask, a standing
    // Allow, or Block, kept for the Space on show. Only a permission Omaweb
    // remembers has one.
    Q_INVOKABLE bool decideSitePermission(const QUrl &url, const QString &permission, int decision);
    // A certificate failure blocks. Whether Omaweb will even offer the reader
    // an exception is this: the engine's own facts about the failure, and
    // whether the address is a Local-development site's own main frame.
    // Nothing here records an answer — there is no remembered exception.
    Q_INVOKABLE bool mayOfferCertificateException(
        const QUrl &url, bool overridable, bool mainFrame, bool fatal) const;
    Q_INVOKABLE bool localDevelopmentSite(const QUrl &url) const;
    // The hosts a Local-development site is recognised by: an IP literal,
    // `localhost` and the `.localhost` and `.test` names. The Omnibar sends a
    // typed one over plain HTTP, and HTTPS-only mode leaves one alone.
    static bool localDevelopmentHost(const QString &host);
    // The origin a site's permissions are kept under: lowercase, the default
    // port left out, the host in its ASCII form. Empty for anything but an
    // `http:` or `https:` address with a host.
    static QString normalizedOrigin(const QUrl &url);
    // The reader chose plain HTTP for this site in HTTPS-only mode, for good,
    // in the active Space. A Private window writes nothing down. Kept with
    // the site's permissions, so resetting them takes it back.
    Q_INVOKABLE bool rememberPlainHttp(const QUrl &url);
    // Whether the reader chose plain HTTP for `origin` in `spaceId`.
    Q_INVOKABLE bool plainHttpRemembered(const QString &spaceId, const QString &origin) const;
    // The reader let one through. Engines remember an accepted certificate for
    // as long as their profile lives and offer no way to take it back, so the
    // grant is recorded here — in memory, for this Space, for this session — to
    // keep the address trigger saying the check was waived for as long as it is.
    Q_INVOKABLE bool recordCertificateException(const QUrl &url);
    Q_INVOKABLE bool certificateExceptionInEffect(const QUrl &url) const;
    Q_INVOKABLE QStringList certificateExceptionOrigins() const;
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
    // where there is nothing there to measure. The engine names the files and
    // directories its site data lives in, because their layout is Chromium's
    // business and not the browser's; Omaweb knows only where it put the
    // profile. Measuring the whole profile instead would report a number the
    // clearing action cannot move.
    Q_INVOKABLE double siteDataBytes(const QString &spaceId, const QStringList &entries) const;
    Q_INVOKABLE bool externalProtocolAllowed(const QUrl &origin, const QString &scheme) const;
    Q_INVOKABLE bool rememberExternalProtocolDecision(const QUrl &origin, const QString &scheme);
    Q_INVOKABLE bool setDownloadDirectory(const QString &path);
    Q_INVOKABLE QString preference(const QString &name, const QString &fallback = {}) const;
    Q_INVOKABLE bool setPreference(const QString &name, const QString &value);

    // Every Known extension Omaweb names, with what the reader decided and what
    // is actually on disk. `enabled` is the reader's answer and `installed` is
    // the package: a reader can enable one before it has been fetched, and what
    // the engine is handed is the pair.
    Q_INVOKABLE QVariantList knownExtensions() const;
    Q_INVOKABLE bool setKnownExtensionEnabled(const QString &key, bool enabled);

    // Fetch this extension's package from the store, whatever is on disk. The
    // reader's own doing: turning a switch on asks for this, and nothing else
    // does.
    Q_INVOKABLE void downloadKnownExtension(const QString &key);
    // Ask the store what version it offers for every enabled extension whose
    // last ask was more than a day ago, and fetch only what has moved. A
    // password manager left to go stale is a real problem, and asking costs a
    // few hundred bytes.
    Q_INVOKABLE void refreshKnownExtensionsIfDue();

    // What the Agent socket reaches (ADR 0051). None of it takes the reader's
    // focus: a tab opened here is never selected, and a Space is never
    // switched to.
    //
    // An Agent Space is one an Agent created. The label, with the name of the
    // connection that created it, is kept in the session store, stays on this
    // machine and never reaches Sync. Taking the Space over removes it and
    // keeps everything else.
    Q_INVOKABLE bool agentSpace(const QString &spaceId) const;
    // Where Omaweb puts something of its own for the reader, such as the notes
    // of an upgrade: the Space on show when it is the reader's own, otherwise
    // the reader's own Space shown most recently, otherwise the first of
    // theirs. Nothing when every Space is an Agent's, or in a Private window.
    Q_INVOKABLE QString readersSpace() const;
    QStringList agentSpaceIds() const;
    // The connection name that created an Agent Space, or nothing.
    Q_INVOKABLE QString agentSpaceCreator(const QString &spaceId) const;
    // A temporary one lasts only as long as the connection that created it:
    // deleteTemporarySpace takes it, with its Engine profile, Browsing data
    // and download records.
    QString createAgentSpace(const QString &name, const QString &creator, bool temporary = false);
    Q_INVOKABLE bool temporarySpace(const QString &spaceId) const;
    // Taking a temporary Space over also makes it permanent.
    Q_INVOKABLE bool takeOverSpace(const QString &spaceId);
    // Refuses a Space that is not temporary, or no longer is.
    bool deleteTemporarySpace(const QString &spaceId);
    // Every temporary Space: as the browser exits, and at start the ones a
    // browser that crashed left behind. Only the browser answering on the
    // Agent socket may do the second, because a second process on the same
    // data would otherwise delete the Spaces of the browser still running.
    void deleteTemporarySpaces();
    // Refuses a Space the reader made or took over, whatever asks.
    bool deleteAgentSpace(const QString &spaceId);
    // A Space grant lets an Agent use one of the reader's Spaces until the
    // reader revokes it. Like the Agent Space label it is kept in the session
    // store, stays on this machine and never reaches Sync. An Agent Space
    // needs none and is refused one.
    Q_INVOKABLE bool spaceGranted(const QString &spaceId) const;
    // In the order they were granted.
    QStringList grantedSpaceIds() const;
    bool grantSpace(const QString &spaceId);
    bool revokeSpaceGrant(const QString &spaceId);
    // A project's Space (`omaweb dev`): the folder it is for, the address its
    // app is served at and its own agent command. Like a grant it is kept in
    // the session store beside the Space records, stays on this machine and
    // never reaches Sync. Deleting the Space takes its project with it.
    std::optional<SpaceProject> spaceProject(const QString &spaceId) const;
    QVariantMap spaceProjectRows() const;
    // The Space whose project directory is `directory` or the nearest folder
    // above it, or nothing.
    QString projectSpaceFor(const QString &directory) const;
    // A new Space of the reader's for the project, named after its folder.
    // Nothing when either could not be kept.
    QString createProjectSpace(const SpaceProject &project);
    // Records or replaces a Space's project. Refused in a Private window.
    bool setSpaceProject(const QString &spaceId, const SpaceProject &project);
    // Clears the Space's project directory, address and agent command, and
    // leaves the Space as it was.
    Q_INVOKABLE bool forgetSpaceProject(const QString &spaceId);
    // The Space waits for its project's address to answer before loading it,
    // so a dev server still starting is never a failed page. It asks the
    // address again until the server answers (see AddressWatch), then loads
    // the address in the Space's blank tab or a new one, and selects it if the
    // Space is on show. Waiting again restarts the wait.
    void awaitAddress(const QString &spaceId, const QUrl &url);
    bool awaitsAddress(const QString &spaceId) const;
    bool activeSpaceAwaitsAddress() const;
    // The reader gave up on it, or went somewhere else in the Space.
    Q_INVOKABLE void stopAwaitingAddress(const QString &spaceId);
    // Tests ask again sooner.
    void setAddressRetryMs(int milliseconds);
    // One Space's tabs: the Space on show from its live model, any other from
    // the store. Empty for a Space this window does not have.
    QVector<TabState> spaceTabs(const QString &spaceId) const;
    // A tab in any Space of this window, or nothing. A Space not on show is
    // read from the store, on the store's thread while this one waits, so a
    // caller that knows where the tab was names that Space first and the
    // others are read only if it is not there.
    std::optional<TabState> findTab(const QString &tabId, const QString &spaceHint = {}) const;
    // What the Omnibar would open for this input, or an invalid address.
    QUrl resolveAddress(const QString &input) const;
    // A new ordinary tab at the end of a Space's list, not selected. Answers
    // its id, or nothing when the Space is not this window's.
    QString openTabInSpace(const QString &spaceId, const QUrl &url);
    // Loads an address in an existing tab without selecting it. A Pinned tab
    // is refused, because its address is the reader's to change. The hint is
    // findTab's.
    bool navigateTab(const QString &tabId, const QUrl &url, const QString &spaceHint = {});
    // Closes an ordinary tab in any Space. A Pinned tab is refused.
    bool closeTabInSpace(const QString &tabId, const QString &spaceHint = {});

signals:
    void paymentCardsChanged();
    void activeSpaceChanged();
    void activeTabChanged();
    void atRestChanged();
    // The split on show changed: a pairing was made or ended, focus moved
    // between the halves, or the active tab entered or left a split.
    void splitChanged();
    void developerToolsChanged();
    void closedTabsChanged();
    void putAwayTabsChanged();
    void putAwayAfterChanged();
    void putAwayNoticeChanged();
    void knownExtensionsChanged();
    // A download ended with nothing written, with a sentence saying why. The
    // switch stays on: the reader asked for the extension, and what failed is
    // the fetching, which is worth another try.
    void knownExtensionFailed(const QString &key, const QString &reason);
    void downloadDirectoryChanged();
    void retainedTabsChanged();
    // The Space being put away, and the tabs inside it that keep running
    // anyway. Named together because the exceptions are only knowable while
    // that Space is still the active one.
    void spaceSuspended(const QString &spaceId, const QStringList &retainedTabIds);
    void spaceRestored(const QString &spaceId);
    void spaceDiscarded(const QString &spaceId);
    void tabMoveConfirmationRequested(const QString &tabId, const QString &destinationSpaceId);
    // Recovery clears core-owned failure state, then asks the active engine to
    // load its page again. Ordinary reloads go straight to the engine host.
    void rendererRecoveryReloadRequested();
    void closeWindowRequested();
    void engineDataClearRequested(
        const QStringList &spaceIds, const QStringList &dataTypes, qint64 since);
    // The reader took an origin's decisions back, so the engine's own record
    // of them goes too: a decision Omaweb cannot reach is one its reset would
    // only appear to undo.
    void engineOriginPermissionsResetRequested(const QString &spaceId, const QUrl &origin);
    void thirdPartyCookieAllowancesChanged();
    // The suggestions for the request the Omnibar is still waiting on. A
    // Private window is answered with none.
    void historySuggestionsReady(const QVariantList &suggestions);
    // The answer to the latest Engine suggestion request: the `engineId`,
    // `engineName` and `siteUrl` of the engine asked, the `terms` it was asked
    // for, and at most four `suggestions`, none of them the terms again.
    // Empty suggestions for anything that was not asked or not answered.
    void engineSuggestionsReady(const QVariantMap &answer);
    // Carried to the search thread. Nothing outside this class connects them.
    void historySearchRequested(
        const QString &spaceId, const QString &text, int limit, quint64 generation);
    void historySearchSpaceForgotten(const QString &spaceId);
    void historySearchDelayRequested(int milliseconds);
    void certificateExceptionsChanged();
    void preferenceChanged(const QString &name);
    void agentSpacesChanged();
    void spaceGrantsChanged();
    void spaceProjectsChanged();
    // A Space started or stopped waiting for its address, or another Space
    // came on show.
    void addressAwaitChanged();
    // The address a Space waited for answered, and this tab is loading it.
    void awaitedAddressLoaded(const QString &spaceId, const QString &tabId);
    // A tab of a Space that is not on show was closed or given a new address
    // behind the page that Space is holding frozen. The page goes, so the tab
    // loads from its saved address when the Space is next shown.
    void awayTabDiscarded(const QString &tabId);

private:
    // The tab the address went to, or nothing.
    QString loadAnsweredAddress(const QString &spaceId, const QUrl &url);
    void rememberReadersSpace();
    // Facts held by pages that survived a Space switch. The session store does
    // not write either one, and a process restart starts them empty again.
    struct LivePageState {
        QUrl iconUrl;
        bool audible = false;
    };
    // One Engine suggestion request: the engine asked and the terms it is
    // asked for, which is what its answer names.
    struct EngineSuggestionRequest {
        QVariantMap engine;
        QString terms;
    };

    void initialize();
    void startHistorySearch(const QString &text, int limit);
    void askEngineForSuggestions();
    void answerEngineSuggestions(
        const EngineSuggestionRequest &request, const QStringList &suggestions);
    void historySearchAnswered(
        const QString &spaceId, const QVariantList &suggestions, quint64 generation);
    void ensureDefaultSpace();
    void ensureActiveTab();
    bool persistTabs();
    void recordTabs();
    void landPendingTabs();
    QUrl storedFaviconIn(const QString &spaceId, const QUrl &pageUrl) const;
    // The icon a tab shows: its page's own, or while it has none, the one its
    // Space stored for the address.
    QUrl iconToShow(const TabState &tab, const QUrl &pageIcon) const;
    // Tabs read back from the store keep what a still-running page showed,
    // and otherwise show their stored icon.
    void showRestoredPages(QVector<TabState> &tabs) const;
    void keepFavicon(const QString &spaceId, const QUrl &pageUrl, const QUrl &iconUrl);
    void schedulePersistTabs();
    // A new tab of the Space on show, selected.
    void appendActiveTab(const QUrl &url, const QString &title);
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
    qint64 now() const;
    // Stamps the tabs on show, and the ones that were until this moment, with
    // the time: a tab is counted as unused from when it left show.
    void noteTabsOnShow();
    void loadPutAwayTabs();
    // Clearing a Space's History takes what it put away over the same range.
    bool forgetPutAwayTabsSince(const QString &spaceId, qint64 since);
    void raisePutAwayNotice();
    // A tab coming back from the closed-tab stack or the put-away list: a new
    // tab of the Space on show, selected.
    void reopenTab(TabState tab);
    // How long a tab may go unshown before it is put away, in milliseconds,
    // or 0 when the reader turned it off.
    qint64 putAwayLimit() const;
    bool tabInUse(const TabState &tab, const QString &spaceActiveTabId, bool audible) const;
    // One Space's part of putAwayUnusedTabs. Answers whether it put any tab
    // away.
    bool putAwayUnusedTabsIn(const QString &spaceId, qint64 limit, qint64 time);
    const RetainedTab *findRetainedTab(const QString &tabId) const;
    QString originInteractionKey(const QUrl &url) const;
    static TabState makeBlankTab(const QString &spaceId);
    // What the tab model holds a split as: two tabs naming each other, side by
    // side with the left one first, both ordinary, one of them focused. Read
    // off every load, since a store is only trusted to hand back what it was
    // given, and put right where it is not.
    static void repairSplits(QVector<TabState> &tabs, const QString &activeTabId);
    void pairTabs(const QString &leftTabId, const QString &rightTabId);
    void unpairTab(const QString &tabId);
    void refreshSplit();
    std::pair<QString, QString> splitOnShowPair() const;
    // The tab a split is entered on: its focused half, or the tab itself when
    // it is in no split.
    QString splitEntryTab(const QString &tabId) const;
    static double steppedZoom(double zoom, int direction);
    static bool isBlank(const QUrl &url);
    bool restingOnBlankTab() const;
    void refreshAtRest();
    void setDeveloperToolsTab(const QString &tabId, const QString &spaceId);
    static QUrl resolveInput(const QString &input);
    QUrl resolveConfiguredInput(const QString &input) const;
    std::optional<QUrl> resolveTypedAddress(const QString &value) const;
    bool loadSearchEngines();
    bool saveSearchEngines(const QVariantList &engines, const QString &defaultEngineId);
    void loadDownloadDirectory();
    bool saveAwayTabs(const QString &spaceId, QVector<TabState> tabs);
    // Reads the Agent Space labels, and which are temporary, from the store.
    void loadAgentSpaces();
    // The colour fewest of the reader's Spaces have, leaving one Space out of
    // the count. Agent Spaces are drawn in no palette colour, so they take up
    // none.
    QString nextSpaceColour(const QString &excludedId = {}) const;
    QString createSpaceRecord(const QString &name, bool agentMade);
    // Puts the list in footer order, the reader's Spaces before the Agent
    // Spaces, and gives a palette name to each Space that holds none, as one
    // stored before Spaces had colours does. Writes back only what changed.
    void settleSpaces();
    // How many Spaces the list holds before the first Agent Space.
    qsizetype readerSpaceCount() const;
    BrowserController(std::shared_ptr<ThreadedSessionStore> store, SpaceStorage storage,
        QString configRoot, QObject *parent);
    // The thread the store takes its calls on, when it has one, is where the
    // history search lives too, so a search never runs ahead of the visits
    // recorded before it.
    BrowserController(std::shared_ptr<SessionStore> store, QThread *storeThread,
        std::optional<SpaceStorage> storage, bool privateBrowsing,
        QSharedPointer<QHash<QString, int>> sessionPermissionDecisions,
        QSharedPointer<SessionSiteState> sessionSiteState, QString configRoot, QObject *parent);

    QString sessionPermissionKey(const QString &origin, const QString &permission) const;

    // The window's session. Which adapter it is answers "does a Private
    // window write this down", so no call site asks.
    std::shared_ptr<SessionStore> m_store;
    // Where this window's Spaces live, kept beside the store because the
    // history search opens the same files from its own thread. Empty in a
    // window that keeps nothing, which is what the profile-path readers answer
    // from rather than testing whether this window is private.
    // Built on the first ask rather than with the controller: a window that
    // never names an extension never builds a network stack for one.
    ExtensionInstaller *extensionInstaller();

    std::optional<SpaceStorage> m_storage;
    std::unique_ptr<ExtensionInstaller> m_extensionInstaller;
    // The search, on the store's thread. Absent in a Private window, which
    // has no history to search.
    HistorySearch *m_historySearch = nullptr;
    QPointer<EngineSuggestions> m_engineSuggestions;
    QPointer<PaymentCards> m_paymentCards;
    // Engine suggestions wait for typing to pause, then ask once. The
    // generation names the request the Omnibar is waiting for, so an answer
    // to text the reader has typed past is dropped when it arrives.
    QTimer m_engineSuggestionPause;
    QPointer<QNetworkReply> m_engineSuggestionReply;
    quint64 m_engineSuggestionGeneration = 0;
    EngineSuggestionRequest m_engineSuggestionRequest;
    // Which request the interface is waiting for. A result carrying an earlier
    // generation belongs to input the reader has already replaced.
    quint64 m_historyGeneration = 0;
    bool m_historySearchRunning = false;
    bool m_historySearchPending = false;
    QString m_pendingHistoryQuery;
    int m_pendingHistoryLimit = 0;
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
    QString m_configRoot;
    QVariantList m_searchEngines;
    QString m_defaultSearchEngineId;
    QString m_errorMessage;
    QVector<TabState> m_closedTabs;
    QVector<PutAwayTab> m_putAwayTabs;
    // The tabs on show when noteTabsOnShow last looked: the active tab and
    // the tab beside it.
    QStringList m_tabsOnShow;
    qint64 m_nowForTests = 0;
    QTimer m_putAwayCheck;
    bool m_putAwayNotice = false;
    QString m_draggedTabId;
    QSet<QString> m_agentTabIds;
    QVector<RetainedTab> m_retainedTabs;
    QHash<QString, LivePageState> m_livePageStates;
    // The name this window's store answers stored favicons under.
    QString m_faviconSource;
    QSet<QString> m_interactedOrigins;
    QString m_downloadDirectory;
    Downloads *m_downloads = nullptr;
    bool m_ready = false;
    bool m_startedWithEmptyState = false;
    bool m_atRest = false;
    // The last answer given for the split on show, so a change is announced
    // once and only when there is one.
    QStringList m_announcedSplit;
    bool m_privateBrowsing = false;
    // Built from the flag above at construction and never from anything else,
    // and with no default: a window is given one kind's table or the other's.
    WindowCapabilities m_capabilities;
    QSharedPointer<QHash<QString, int>> m_sessionPermissionDecisions;
    QSharedPointer<SessionSiteState> m_sessionSiteState;
    // Agent Space id to the connection name that created it.
    QHash<QString, QString> m_agentSpaces;
    QStringList m_spaceGrants;
    QHash<QString, SpaceProject> m_spaceProjects;
    QHash<QString, AddressWatch *> m_addressWatches;
    int m_addressRetryMs = 500;
    QSet<QString> m_temporarySpaceIds;
};

// Makes `BrowserController`'s enums available to QML as `import Omaweb`. The
// type is uncreatable: a window's controller is handed to QML, never built
// there. Call once per process, before loading QML.
void registerBrowserController();

} // namespace omaweb
