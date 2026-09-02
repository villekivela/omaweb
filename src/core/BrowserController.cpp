#include "BrowserController.h"

#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUrlQuery>
#include <QUuid>
#include <QStandardPaths>

#include <iterator>

namespace omaweb {
namespace {

constexpr int persistTabsDelayMilliseconds = 400;
// How many closes a Space remembers. Deep enough that a reader who shut a row
// of tabs can walk all of them back, bounded so the store does not grow into a
// second history of everywhere they have been.
constexpr qsizetype retainedClosedTabs = 25;
// Zoom factors arrive back from an engine as the doubles it rounded them to, so
// a rung is recognised by nearness rather than by equality.
constexpr double zoomTolerance = 0.001;

QVariantList predefinedSearchEngines()
{
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("duckduckgo")},
            {QStringLiteral("name"), QStringLiteral("DuckDuckGo")},
            {QStringLiteral("queryUrl"), QStringLiteral("https://duckduckgo.com/?q={query}")},
            {QStringLiteral("keyword"), QStringLiteral("d")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("google")},
            {QStringLiteral("name"), QStringLiteral("Google")},
            {QStringLiteral("queryUrl"), QStringLiteral("https://www.google.com/search?q={query}")},
            {QStringLiteral("keyword"), QStringLiteral("g")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("bing")},
            {QStringLiteral("name"), QStringLiteral("Bing")},
            {QStringLiteral("queryUrl"), QStringLiteral("https://www.bing.com/search?q={query}")},
            {QStringLiteral("keyword"), QStringLiteral("b")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("brave")},
            {QStringLiteral("name"), QStringLiteral("Brave Search")},
            {QStringLiteral("queryUrl"), QStringLiteral("https://search.brave.com/search?q={query}")},
            {QStringLiteral("keyword"), QStringLiteral("br")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("kagi")},
            {QStringLiteral("name"), QStringLiteral("Kagi")},
            {QStringLiteral("queryUrl"), QStringLiteral("https://kagi.com/search?q={query}")},
            {QStringLiteral("keyword"), QStringLiteral("k")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("ecosia")},
            {QStringLiteral("name"), QStringLiteral("Ecosia")},
            {QStringLiteral("queryUrl"), QStringLiteral("https://www.ecosia.org/search?q={query}")},
            {QStringLiteral("keyword"), QStringLiteral("e")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("startpage")},
            {QStringLiteral("name"), QStringLiteral("Startpage")},
            {QStringLiteral("queryUrl"),
                QStringLiteral("https://www.startpage.com/sp/search?query={query}")},
            {QStringLiteral("keyword"), QStringLiteral("sp")}},
    };
}

} // namespace

BrowserController::BrowserController(QString dataRoot, QString engineName, QObject *parent)
    : BrowserController(dataRoot, std::move(engineName), false,
        QSharedPointer<QHash<QString, int>>::create(), {}, parent)
{
}

BrowserController::BrowserController(QString dataRoot, QString engineName, QString configRoot,
    QObject *parent)
    : BrowserController(std::move(dataRoot), std::move(engineName), false,
        QSharedPointer<QHash<QString, int>>::create(), std::move(configRoot), parent)
{
}

BrowserController::BrowserController(QString dataRoot, QString engineName,
    bool privateBrowsing, QObject *parent)
    : BrowserController(dataRoot, std::move(engineName), privateBrowsing,
        QSharedPointer<QHash<QString, int>>::create(), {}, parent)
{
}

BrowserController::BrowserController(QString dataRoot, QString engineName,
    bool privateBrowsing, QSharedPointer<QHash<QString, int>> sessionPermissionDecisions,
    QObject *parent)
    : BrowserController(dataRoot, std::move(engineName), privateBrowsing,
        std::move(sessionPermissionDecisions), {}, parent)
{
}

BrowserController::BrowserController(QString dataRoot, QString engineName,
    bool privateBrowsing, QSharedPointer<QHash<QString, int>> sessionPermissionDecisions,
    QString configRoot, QObject *parent)
    : QObject(parent)
    , m_store(std::move(dataRoot))
    , m_engineName(std::move(engineName))
    , m_configRoot(std::move(configRoot))
    , m_privateBrowsing(privateBrowsing)
    , m_sessionPermissionDecisions(std::move(sessionPermissionDecisions))
{
    m_pinnedTabs.setSourceModel(&m_tabs);
    m_pinnedTabs.setFilterRole(TabListModel::PinnedRole);
    m_pinnedTabs.setFilterRegularExpression(QRegularExpression(QStringLiteral("^true$")));
    m_unpinnedTabs.setSourceModel(&m_tabs);
    m_unpinnedTabs.setFilterRole(TabListModel::PinnedRole);
    m_unpinnedTabs.setFilterRegularExpression(QRegularExpression(QStringLiteral("^false$")));
    m_persistTabsTimer.setSingleShot(true);
    m_persistTabsTimer.setInterval(persistTabsDelayMilliseconds);
    connect(&m_persistTabsTimer, &QTimer::timeout, this, [this] { persistTabs(); });
    // Every route to a blank tab changes the tab model: opening the first
    // address, closing the last page, switching Space, restoring a session.
    // Watching the model is what keeps the answer from depending on a caller
    // remembering to announce it.
    connect(&m_tabs, &QAbstractItemModel::rowsInserted, this, [this] { refreshAtRest(); });
    connect(&m_tabs, &QAbstractItemModel::rowsRemoved, this, [this] { refreshAtRest(); });
    connect(&m_tabs, &QAbstractItemModel::dataChanged, this, [this] { refreshAtRest(); });
    connect(&m_tabs, &QAbstractItemModel::modelReset, this, [this] { refreshAtRest(); });
    initialize();
}

// A quit while a coalesced write is still pending would drop the last address
// or title a page reported.
BrowserController::~BrowserController()
{
    if (m_persistTabsTimer.isActive()) {
        persistTabs();
    }
}

QAbstractItemModel *BrowserController::spaces()
{
    return &m_spaces;
}

QAbstractItemModel *BrowserController::tabs()
{
    return &m_tabs;
}

// The sidebar renders pinned and ordinary tabs as two lists rather than hiding
// rows: a positioner does not reliably re-place children whose visibility
// changes, so filtering belongs in the model.
QAbstractItemModel *BrowserController::pinnedTabs()
{
    return &m_pinnedTabs;
}

QAbstractItemModel *BrowserController::unpinnedTabs()
{
    return &m_unpinnedTabs;
}

QString BrowserController::activeSpaceId() const
{
    return m_activeSpaceId;
}

QString BrowserController::activeSpaceName() const
{
    return m_activeSpaceName;
}

QString BrowserController::activeTabId() const
{
    return m_activeTabId;
}

QUrl BrowserController::activeUrl() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return tab ? tab->url : QUrl(QStringLiteral("about:blank"));
}

QString BrowserController::activeTitle() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return tab && !tab->title.isEmpty() ? tab->title : QStringLiteral("New tab");
}

QString BrowserController::activeProfilePath() const
{
    if (m_privateBrowsing) {
        return {};
    }
    return m_store.engineProfilePath(m_activeSpaceId, m_engineName);
}

QString BrowserController::profilePathForSpace(const QString &spaceId) const
{
    if (m_privateBrowsing) {
        return {};
    }
    for (const auto &space : m_spaces.items()) {
        if (space.id == spaceId) {
            return m_store.engineProfilePath(spaceId, m_engineName);
        }
    }
    return {};
}

bool BrowserController::activeTabPinned() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return tab && tab->pinned;
}

double BrowserController::activeTabZoom() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return tab ? tab->zoom : 1.0;
}

bool BrowserController::activeTabBlank() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return !tab || isBlank(tab->url);
}

bool BrowserController::activeTabKeepActive() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return tab && tab->pinned && tab->keepActive;
}

int BrowserController::closedTabCount() const
{
    return static_cast<int>(m_closedTabs.size());
}

// The one place the retained tabs are spoken of in strings: QML reads a list
// of maps, and everything inside the core reads the type.
QVariantList BrowserController::retainedTabs() const
{
    QVariantList tabs;
    tabs.reserve(m_retainedTabs.size());
    for (const auto &retained : m_retainedTabs) {
        tabs.append(retained.toVariantMap());
    }
    return tabs;
}

bool BrowserController::atRest() const
{
    return m_atRest;
}

QString BrowserController::developerToolsTabId() const
{
    return m_developerToolsTabId;
}

bool BrowserController::activeTabInspected() const
{
    return !m_developerToolsTabId.isEmpty() && m_developerToolsTabId == m_activeTabId;
}

bool BrowserController::activeRendererFailed() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return tab && !tab->rendererFailureReason.isEmpty();
}

QString BrowserController::activeRendererFailureReason() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return tab ? tab->rendererFailureReason : QString{};
}

bool BrowserController::privateBrowsing() const
{
    return m_privateBrowsing;
}

bool BrowserController::ready() const
{
    return m_ready;
}

QString BrowserController::errorMessage() const
{
    return m_errorMessage;
}

QString BrowserController::downloadDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

bool BrowserController::acceptDownloads() const
{
    return true;
}

void BrowserController::activateTab(const QString &tabId)
{
    if (tabId == m_activeTabId || !m_tabs.find(tabId)) {
        return;
    }
    setActiveTab(tabId);
}

QString BrowserController::createSpace(const QString &name)
{
    if (m_privateBrowsing) {
        return {};
    }
    const auto normalizedName = name.trimmed();
    if (normalizedName.isEmpty()) {
        return {};
    }

    SpaceState space;
    space.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    space.name = normalizedName;
    space.color = QStringLiteral("#7c6cff");
    if (!m_store.saveSpace(space)) {
        return {};
    }

    auto spaces = m_spaces.items();
    spaces.append(space);
    m_spaces.reset(std::move(spaces));
    return space.id;
}

bool BrowserController::switchSpace(const QString &spaceId)
{
    if (m_privateBrowsing) {
        return false;
    }
    if (spaceId == m_activeSpaceId) {
        return true;
    }

    const SpaceState *destination = nullptr;
    for (const auto &space : m_spaces.items()) {
        if (space.id == spaceId) {
            destination = &space;
            break;
        }
    }
    if (!destination) {
        return false;
    }

    if (!persistTabs()) {
        return false;
    }
    emit spaceSuspended(m_activeSpaceId, retainedTabIds());
    if (!m_store.setActiveSpace(spaceId)) {
        emit spaceRestored(m_activeSpaceId);
        return false;
    }
    m_activeSpaceId = destination->id;
    m_activeSpaceName = destination->name;
    auto spaces = m_spaces.items();
    for (auto &space : spaces) {
        space.active = space.id == spaceId;
    }
    m_spaces.reset(std::move(spaces));
    ensureActiveTab();
    // Each Space takes back its own closes, and what is being retained changes
    // the moment the Space on show does.
    loadClosedTabs();
    refreshRetainedTabs();
    emit spaceRestored(m_activeSpaceId);
    emit activeSpaceChanged();
    emit activeTabChanged();
    return true;
}

bool BrowserController::renameSpace(const QString &spaceId, const QString &name)
{
    if (m_privateBrowsing) {
        return false;
    }
    const auto normalizedName = name.trimmed();
    if (normalizedName.isEmpty()) {
        return false;
    }

    auto spaces = m_spaces.items();
    for (auto &space : spaces) {
        if (space.id != spaceId) {
            continue;
        }
        space.name = normalizedName;
        if (!m_store.saveSpace(space)) {
            return false;
        }
        if (space.active) {
            m_activeSpaceName = normalizedName;
        }
        m_spaces.reset(std::move(spaces));
        if (spaceId == m_activeSpaceId) {
            emit activeSpaceChanged();
        }
        return true;
    }
    return false;
}

bool BrowserController::deleteSpace(const QString &spaceId, const QString &confirmationName)
{
    if (m_privateBrowsing) {
        return false;
    }
    if (m_spaces.items().size() <= 1) {
        return false;
    }

    const SpaceState *target = nullptr;
    QString replacementId;
    QString replacementName;
    for (const auto &space : m_spaces.items()) {
        if (space.id == spaceId) {
            target = &space;
        } else if (replacementId.isEmpty()) {
            replacementId = space.id;
            replacementName = space.name;
        }
    }
    if (!target) {
        return false;
    }
    if (confirmationName != target->name) {
        return false;
    }

    const auto deletingActiveSpace = spaceId == m_activeSpaceId;
    if (deletingActiveSpace && !persistTabs()) {
        return false;
    }
    if (deletingActiveSpace) {
        // Nothing in a Space that is being deleted is worth keeping running.
        emit spaceSuspended(spaceId, {});
    }
    if (!m_store.deleteSpace(spaceId, deletingActiveSpace ? replacementId : QString{})) {
        if (deletingActiveSpace) {
            emit spaceRestored(spaceId);
        }
        return false;
    }
    m_spaces.reset(m_store.loadSpaces());
    // Nothing belonging to a deleted Space should outlive it, including the
    // pages a window is still holding open for it and an inspector attached to
    // one of them — which, while another Space is active, is not in the tab
    // model to be noticed missing.
    if (spaceId == m_developerToolsSpaceId) {
        closeDeveloperTools();
    }
    emit spaceDiscarded(spaceId);
    if (deletingActiveSpace) {
        m_activeSpaceId = replacementId;
        m_activeSpaceName = replacementName;
        ensureActiveTab();
        loadClosedTabs();
        emit spaceRestored(m_activeSpaceId);
        emit activeSpaceChanged();
        emit activeTabChanged();
    }
    // Last, so the Space now on show is the one excluded: a deletion that
    // replaced the active Space must not leave that Space's own Keep active
    // tabs listed as though something were holding them for it.
    refreshRetainedTabs();
    return true;
}

bool BrowserController::requestTabMoveToSpace(const QString &tabId,
    const QString &destinationSpaceId, bool hasEditedFormState)
{
    if (m_privateBrowsing) {
        return false;
    }
    if (!m_tabs.find(tabId) || destinationSpaceId == m_activeSpaceId) {
        return false;
    }
    for (const auto &space : m_spaces.items()) {
        if (space.id == destinationSpaceId) {
            if (hasEditedFormState) {
                emit tabMoveConfirmationRequested(tabId, destinationSpaceId);
                return true;
            }
            return confirmTabMoveToSpace(tabId, destinationSpaceId);
        }
    }
    return false;
}

bool BrowserController::confirmTabMoveToSpace(const QString &tabId,
    const QString &destinationSpaceId)
{
    if (m_privateBrowsing) {
        return false;
    }
    const auto *sourceTab = m_tabs.find(tabId);
    if (!sourceTab || destinationSpaceId == m_activeSpaceId) {
        return false;
    }
    bool destinationExists = false;
    for (const auto &space : m_spaces.items()) {
        destinationExists = destinationExists || space.id == destinationSpaceId;
    }
    if (!destinationExists) {
        return false;
    }

    auto sourceTabs = m_tabs.items();
    TabState movedTab = *sourceTab;
    sourceTabs.removeIf([&tabId](const TabState &tab) { return tab.id == tabId; });
    QString sourceActiveTabId = m_activeTabId;
    if (sourceTabs.isEmpty()) {
        auto blankTab = makeBlankTab(m_activeSpaceId);
        sourceActiveTabId = blankTab.id;
        sourceTabs.append(blankTab);
    } else if (tabId == m_activeTabId) {
        sourceActiveTabId = sourceTabs.first().id;
    }

    auto destinationTabs = m_store.loadTabs(destinationSpaceId);
    QString destinationActiveTabId;
    for (const auto &tab : destinationTabs) {
        if (tab.active) {
            destinationActiveTabId = tab.id;
            break;
        }
    }
    movedTab.spaceId = destinationSpaceId;
    movedTab.active = destinationTabs.isEmpty();
    movedTab.loading = false;
    movedTab.rendererFailureReason.clear();
    destinationTabs.append(movedTab);
    if (destinationActiveTabId.isEmpty()) {
        destinationActiveTabId = movedTab.id;
    }

    if (!m_store.saveSpaceMove(m_activeSpaceId, sourceTabs, sourceActiveTabId,
            destinationSpaceId, destinationTabs, destinationActiveTabId)) {
        return false;
    }

    // The tab reloads under the destination Space's identity and its engine
    // profile, so nothing the inspector was attached to survives the move.
    if (tabId == m_developerToolsTabId) {
        closeDeveloperTools();
    }
    m_activeTabId = sourceActiveTabId;
    m_tabs.reset(std::move(sourceTabs));
    refreshRetainedTabs();
    emit activeTabChanged();
    return true;
}

void BrowserController::openInput(const QString &input, bool inNewTab)
{
    const auto url = resolveConfiguredInput(input);
    if (!url.isValid()) {
        return;
    }

    if (inNewTab) {
        if (auto *current = m_tabs.find(m_activeTabId)) {
            current->active = false;
            m_tabs.notifyChanged(current->id, {TabListModel::ActiveRole});
        }

        TabState tab;
        tab.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        tab.spaceId = m_activeSpaceId;
        tab.url = url;
        tab.title = url.host().isEmpty() ? QStringLiteral("New tab") : url.host();
        tab.active = true;
        m_tabs.append(tab);
        m_activeTabId = tab.id;
    } else if (auto *tab = m_tabs.find(m_activeTabId)) {
        tab->url = url;
        tab->title = url.host().isEmpty() ? url.toDisplayString() : url.host();
        tab->rendererFailureReason.clear();
        m_tabs.notifyChanged(tab->id, {TabListModel::UrlRole, TabListModel::TitleRole});
    }

    refreshSoundSuppression();
    schedulePersistTabs();
    emit activeTabChanged();
}

bool BrowserController::retryActiveUrlInsecurely()
{
    auto url = activeUrl();
    if (url.scheme() != QStringLiteral("https")) {
        return false;
    }
    url.setScheme(QStringLiteral("http"));
    openInput(url.toString(), false);
    return true;
}

void BrowserController::openInputInBackground(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty()) {
        return;
    }
    TabState tab;
    tab.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tab.spaceId = m_activeSpaceId;
    tab.url = url;
    tab.title = url.host().isEmpty() ? url.toDisplayString() : url.host();
    tab.active = false;
    m_tabs.append(tab);
    refreshSoundSuppression();
    schedulePersistTabs();
}

void BrowserController::closeActiveTab()
{
    closeTab(m_activeTabId);
}

void BrowserController::closeTab(const QString &tabId)
{
    auto *tab = m_tabs.find(tabId);
    if (!tab || tab->pinned) {
        return;
    }
    // Past this point the tab goes, is emptied, or takes its Private window
    // with it, and in every case the page the inspector was attached to is no
    // longer there.
    if (tabId == m_developerToolsTabId) {
        closeDeveloperTools();
    }

    if (m_tabs.rowCount() == 1) {
        if (m_privateBrowsing) {
            emit closeWindowRequested();
            return;
        }
        // The last tab is emptied rather than removed, but the page in it was
        // still closed and the reader can still ask for it back.
        rememberClosedTab(*tab);
        tab->url = QUrl(QStringLiteral("about:blank"));
        tab->title = QStringLiteral("New tab");
        tab->loading = false;
        tab->muted = false;
        tab->zoom = 1.0;
        tab->rendererFailureReason.clear();
        m_tabs.notifyChanged(tab->id, {
            TabListModel::UrlRole,
            TabListModel::TitleRole,
            TabListModel::LoadingRole,
            TabListModel::MutedRole,
            TabListModel::ZoomRole,
        });
        refreshSoundSuppression();
        schedulePersistTabs();
        emit activeTabChanged();
        return;
    }

    rememberClosedTab(*tab);
    if (tabId != m_activeTabId) {
        m_tabs.remove(tabId);
        schedulePersistTabs();
        return;
    }

    const auto items = m_tabs.items();
    auto nextId = items.first().id;
    for (qsizetype index = 0; index < items.size(); ++index) {
        if (items.at(index).id != m_activeTabId) {
            continue;
        }
        nextId = items.at(index == 0 ? 1 : index - 1).id;
        break;
    }

    m_tabs.remove(tabId);
    setActiveTab(nextId);
}

void BrowserController::closeOtherTabs(const QString &tabId)
{
    if (!m_tabs.find(tabId)) {
        return;
    }
    QStringList doomed;
    for (const auto &tab : m_tabs.items()) {
        if (!tab.pinned && tab.id != tabId) {
            doomed.append(tab.id);
        }
    }
    for (const auto &id : doomed) {
        closeTab(id);
    }
}

void BrowserController::closeTabsBelow(const QString &tabId)
{
    const auto *anchor = m_tabs.find(tabId);
    // A command about the rows below this one is a command about the ordinary
    // list, and a pin is not in it.
    if (!anchor || anchor->pinned) {
        return;
    }
    QStringList doomed;
    bool below = false;
    for (const auto &tab : m_tabs.items()) {
        if (tab.id == tabId) {
            below = true;
            continue;
        }
        if (below && !tab.pinned) {
            doomed.append(tab.id);
        }
    }
    for (const auto &id : doomed) {
        closeTab(id);
    }
}

// What a Space remembers about a tab it lost is what the session kept about it:
// the address, the title, the pinning, the zoom and the muting. Not the page —
// a reopened tab loads the address again rather than resuming a document.
void BrowserController::rememberClosedTab(const TabState &tab)
{
    if (isBlank(tab.url)) {
        return;
    }
    TabState closed;
    // A record of its own, not the tab's. The last tab in a Space is emptied
    // rather than removed and keeps its id, so a Space whose last page is
    // closed twice would otherwise hold two records under one id — and the
    // store, which keys them, would refuse the whole stack.
    closed.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    closed.spaceId = tab.spaceId;
    closed.url = tab.url;
    closed.title = tab.title;
    closed.pinned = tab.pinned;
    closed.muted = tab.muted;
    closed.zoom = tab.zoom;
    closed.keepActive = tab.keepActive;
    m_closedTabs.prepend(closed);
    while (m_closedTabs.size() > retainedClosedTabs) {
        m_closedTabs.removeLast();
    }
    persistClosedTabs();
    emit closedTabsChanged();
}

void BrowserController::loadClosedTabs()
{
    m_closedTabs = m_privateBrowsing ? QVector<TabState>{}
        : m_store.loadClosedTabs(m_activeSpaceId);
    while (m_closedTabs.size() > retainedClosedTabs) {
        m_closedTabs.removeLast();
    }
    emit closedTabsChanged();
}

// A Private session keeps the same stack for as long as it lasts and leaves
// nothing behind, so nothing about it reaches a store.
void BrowserController::persistClosedTabs()
{
    if (m_privateBrowsing) {
        return;
    }
    m_store.saveClosedTabs(m_activeSpaceId, m_closedTabs);
}

// Newest first, so repeated asking walks back through the closes in the order
// they happened. A tab comes back as it was left — its address, title, pinning,
// zoom and muting — as a new tab, because the page it held is gone.
void BrowserController::reopenClosedTab()
{
    if (m_closedTabs.isEmpty()) {
        return;
    }
    auto tab = m_closedTabs.takeFirst();
    persistClosedTabs();
    emit closedTabsChanged();

    tab.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tab.spaceId = m_activeSpaceId;
    tab.active = true;
    // A Private window has no Pinned section to come back into, and Keep
    // active is a Pinned tab's setting.
    if (m_privateBrowsing) {
        tab.pinned = false;
        tab.keepActive = false;
    }
    if (!tab.pinned) {
        tab.keepActive = false;
    }

    if (auto *current = m_tabs.find(m_activeTabId)) {
        current->active = false;
        m_tabs.notifyChanged(current->id, {TabListModel::ActiveRole});
    }

    // A tab that was pinned comes back into the Pinned section, at its end,
    // rather than joining the ordinary rows and losing what the reader had
    // arranged about it.
    if (tab.pinned) {
        m_tabs.insert(tab, pinnedTabCount());
    } else {
        m_tabs.append(tab);
    }
    m_activeTabId = tab.id;
    refreshSoundSuppression();
    persistTabs();
    emit activeTabChanged();
}

QString BrowserController::duplicateTab(const QString &tabId)
{
    const auto *source = m_tabs.find(tabId);
    if (!source || isBlank(source->url)) {
        return {};
    }

    TabState tab;
    tab.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tab.spaceId = m_activeSpaceId;
    tab.url = source->url;
    tab.title = source->title;
    tab.active = true;
    // A pin is the Space's furniture rather than something a copy inherits, and
    // zoom and muting are decisions about the tab the reader made, not about
    // the address. So the duplicate is an ordinary tab at 100 percent, unmuted.
    const auto destination = source->pinned
        ? m_tabs.items().size()
        : tabRow(tabId) + 1;

    if (auto *current = m_tabs.find(m_activeTabId)) {
        current->active = false;
        m_tabs.notifyChanged(current->id, {TabListModel::ActiveRole});
    }
    m_tabs.insert(tab, destination);
    m_activeTabId = tab.id;
    refreshSoundSuppression();
    persistTabs();
    emit activeTabChanged();
    return tab.id;
}

qsizetype BrowserController::pinnedTabCount() const
{
    qsizetype count = 0;
    for (const auto &tab : m_tabs.items()) {
        if (tab.pinned) {
            ++count;
        }
    }
    return count;
}

qsizetype BrowserController::tabRow(const QString &tabId) const
{
    const auto &items = m_tabs.items();
    for (qsizetype row = 0; row < items.size(); ++row) {
        if (items.at(row).id == tabId) {
            return row;
        }
    }
    return -1;
}

int BrowserController::tabSectionIndex(const QString &tabId) const
{
    const auto *tab = m_tabs.find(tabId);
    if (!tab) {
        return -1;
    }
    return static_cast<int>(tabRow(tabId) - (tab->pinned ? 0 : pinnedTabCount()));
}

int BrowserController::tabSectionCount(const QString &tabId) const
{
    const auto *tab = m_tabs.find(tabId);
    if (!tab) {
        return 0;
    }
    const auto pinned = pinnedTabCount();
    return static_cast<int>(tab->pinned ? pinned : m_tabs.items().size() - pinned);
}

bool BrowserController::moveTab(const QString &tabId, int destinationIndex)
{
    const auto *tab = m_tabs.find(tabId);
    if (!tab) {
        return false;
    }
    const auto pinned = pinnedTabCount();
    const auto sectionStart = tab->pinned ? qsizetype{0} : pinned;
    const auto sectionCount = tabSectionCount(tabId);
    if (destinationIndex < 0 || destinationIndex >= sectionCount) {
        return false;
    }
    if (!m_tabs.move(tabId, sectionStart + destinationIndex)) {
        return false;
    }
    persistTabs();
    return true;
}

bool BrowserController::moveTabBy(const QString &tabId, int offset)
{
    const auto index = tabSectionIndex(tabId);
    if (index < 0 || offset == 0) {
        return false;
    }
    const auto destination = index + offset;
    if (destination < 0 || destination >= tabSectionCount(tabId)) {
        return false;
    }
    return moveTab(tabId, destination);
}

void BrowserController::toggleActivePinned()
{
    if (m_privateBrowsing) {
        return;
    }
    auto *tab = m_tabs.find(m_activeTabId);
    if (!tab) {
        return;
    }

    qsizetype pinnedCount = 0;
    for (const auto &item : m_tabs.items()) {
        if (item.pinned) {
            ++pinnedCount;
        }
    }
    const bool pinned = !tab->pinned;
    const auto destinationRow = pinned ? pinnedCount : pinnedCount - 1;
    m_tabs.move(m_activeTabId, destinationRow);
    tab = m_tabs.find(m_activeTabId);
    tab->pinned = pinned;
    // Keep active is a Pinned tab's setting, so unpinning gives it up rather
    // than keeping it in reserve for the next time the tab is pinned.
    const auto lostKeepActive = !pinned && tab->keepActive;
    if (lostKeepActive) {
        tab->keepActive = false;
    }
    m_tabs.notifyChanged(tab->id, lostKeepActive
        ? QList<int>{TabListModel::PinnedRole, TabListModel::KeepActiveRole}
        : QList<int>{TabListModel::PinnedRole});
    persistTabs();
    emit activeTabChanged();
}

bool BrowserController::setTabKeepActive(const QString &tabId, bool keepActive)
{
    if (m_privateBrowsing) {
        return false;
    }
    auto *tab = m_tabs.find(tabId);
    if (!tab || !tab->pinned) {
        return false;
    }
    if (tab->keepActive == keepActive) {
        return true;
    }
    tab->keepActive = keepActive;
    m_tabs.notifyChanged(tab->id, {TabListModel::KeepActiveRole});
    persistTabs();
    if (tabId == m_activeTabId) {
        emit activeTabChanged();
    }
    return true;
}

bool BrowserController::releaseRetainedTab(const QString &tabId)
{
    if (m_privateBrowsing) {
        return false;
    }
    // A retained tab of the Space on show is simply one of its tabs.
    if (m_tabs.find(tabId)) {
        return setTabKeepActive(tabId, false);
    }
    if (const auto *retained = findRetainedTab(tabId)) {
        const auto spaceId = retained->spaceId;
        auto tabs = m_store.loadTabs(spaceId);
        QString activeTabId;
        bool found = false;
        for (auto &tab : tabs) {
            if (tab.active) {
                activeTabId = tab.id;
            }
            if (tab.id != tabId) {
                continue;
            }
            tab.keepActive = false;
            found = true;
        }
        if (!found || !m_store.saveTabs(spaceId, tabs, activeTabId)) {
            return false;
        }
        refreshRetainedTabs();
        return true;
    }
    return false;
}

bool BrowserController::tabPinned(const QString &tabId) const
{
    const auto *tab = m_tabs.find(tabId);
    return tab && tab->pinned;
}

bool BrowserController::tabKeepActive(const QString &tabId) const
{
    const auto *tab = m_tabs.find(tabId);
    return tab && tab->pinned && tab->keepActive;
}

bool BrowserController::toggleActiveKeepActive()
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return tab && setTabKeepActive(m_activeTabId, !tab->keepActive);
}

// The two exceptions to suspension: the reader's standing request, and the
// inspector's. An inspected tab that stopped would leave the frontend attached
// to a page that cannot answer.
bool BrowserController::retains(const TabState &tab, const QString &developerToolsTabId)
{
    // A page with no address is not running, so there is nothing about it to
    // keep running.
    if (isBlank(tab.url)) {
        return false;
    }
    return (tab.pinned && tab.keepActive) || tab.id == developerToolsTabId;
}

QStringList BrowserController::retainedTabIds() const
{
    QStringList ids;
    for (const auto &tab : m_tabs.items()) {
        if (retains(tab, m_developerToolsTabId)) {
            ids.append(tab.id);
        }
    }
    return ids;
}

// Read from the stores of the Spaces that are not on show, because that is the
// only place their tabs are: the tab model holds one Space at a time. The
// active Space contributes nothing — its pages are live because the reader is
// looking at them, not because anything is being retained for them.
void BrowserController::refreshRetainedTabs()
{
    QVector<RetainedTab> retained;
    if (!m_privateBrowsing) {
        for (const auto &space : m_spaces.items()) {
            if (space.id == m_activeSpaceId) {
                continue;
            }
            for (const auto &tab : m_store.loadTabs(space.id)) {
                if (!retains(tab, m_developerToolsTabId)) {
                    continue;
                }
                retained.append(RetainedTab{
                    .tabId = tab.id,
                    .spaceId = space.id,
                    .spaceName = space.name,
                    .title = tab.title,
                    .url = tab.url,
                    .zoom = tab.zoom,
                    .muted = tab.muted,
                    .inspected = tab.id == m_developerToolsTabId,
                });
            }
        }
    }
    if (retained == m_retainedTabs) {
        return;
    }
    m_retainedTabs = std::move(retained);
    emit retainedTabsChanged();
}

const RetainedTab *BrowserController::findRetainedTab(const QString &tabId) const
{
    for (const auto &retained : m_retainedTabs) {
        if (retained.tabId == tabId) {
            return &retained;
        }
    }
    return nullptr;
}

QVariantMap BrowserController::notificationTarget(const QString &spaceId,
    const QUrl &origin) const
{
    const auto wanted = normalizedOrigin(origin);
    if (wanted.isEmpty()) {
        return {};
    }

    QString spaceName;
    for (const auto &space : m_spaces.items()) {
        if (space.id == spaceId) {
            spaceName = space.name;
            break;
        }
    }

    const auto answer = [&](const QString &tabId, const QString &title) {
        return QVariantMap{
            {QStringLiteral("tabId"), tabId},
            {QStringLiteral("spaceId"), spaceId},
            {QStringLiteral("spaceName"), m_privateBrowsing
                ? QStringLiteral("Private") : spaceName},
            {QStringLiteral("origin"), wanted},
            {QStringLiteral("title"), title},
        };
    };

    // The Space the reader is looking at: any of its pages may say something.
    if (spaceId == m_activeSpaceId) {
        for (const auto &tab : m_tabs.items()) {
            if (normalizedOrigin(tab.url) == wanted) {
                return answer(tab.id, tab.title);
            }
        }
        return {};
    }

    // Any other Space is put away, and only a tab that was kept running has a
    // page left to speak for.
    for (const auto &retained : m_retainedTabs) {
        if (retained.spaceId == spaceId && normalizedOrigin(retained.url) == wanted) {
            return answer(retained.tabId, retained.title);
        }
    }
    return {};
}

// Activating a notification is asking to be taken to the page that sent it,
// which may mean changing Space first.
bool BrowserController::activateNotificationTarget(const QString &spaceId,
    const QString &tabId)
{
    if (!spaceId.isEmpty() && spaceId != m_activeSpaceId && !switchSpace(spaceId)) {
        return false;
    }
    if (!m_tabs.find(tabId)) {
        return false;
    }
    activateTab(tabId);
    return true;
}

QString BrowserController::originInteractionKey(const QUrl &url) const
{
    const auto origin = normalizedOrigin(url);
    return origin.isEmpty() ? QString{} : m_activeSpaceId + QLatin1Char('|') + origin;
}

void BrowserController::recordOriginInteraction(const QUrl &url)
{
    const auto key = originInteractionKey(url);
    if (key.isEmpty() || m_interactedOrigins.contains(key)) {
        return;
    }
    m_interactedOrigins.insert(key);
    // An origin the reader has dealt with has earned its sound in every tab
    // showing it, not only in the one they touched.
    refreshSoundSuppression();
}

// Playback itself is allowed: a page that starts a silent video interrupts
// nobody, and refusing it costs the reader pages that work everywhere else.
// What waits is the sound. A tab whose origin the reader has not dealt with is
// held silent, so a page that starts making noise on its own is not heard until
// they have answered for the site — by touching the page, or by asking the row
// for the sound.
bool BrowserController::suppressesSound(const TabState &tab) const
{
    return !isBlank(tab.url) && !originInteracted(tab.url);
}

void BrowserController::refreshSoundSuppression()
{
    for (const auto &item : m_tabs.items()) {
        auto *tab = m_tabs.find(item.id);
        const auto suppressed = suppressesSound(*tab);
        if (tab->soundSuppressed == suppressed) {
            continue;
        }
        tab->soundSuppressed = suppressed;
        m_tabs.notifyChanged(tab->id, {TabListModel::SoundSuppressedRole});
    }
}

bool BrowserController::tabSoundSuppressed(const QString &tabId) const
{
    const auto *tab = m_tabs.find(tabId);
    return tab && tab->soundSuppressed;
}

void BrowserController::grantTabSound(const QString &tabId)
{
    const auto *tab = m_tabs.find(tabId);
    if (!tab || !tab->soundSuppressed) {
        return;
    }
    recordOriginInteraction(tab->url);
}

bool BrowserController::originInteracted(const QUrl &url) const
{
    const auto key = originInteractionKey(url);
    return !key.isEmpty() && m_interactedOrigins.contains(key);
}

void BrowserController::updateTab(const QString &tabId, const QUrl &url, const QString &title)
{
    auto *tab = m_tabs.find(tabId);
    if (!tab) {
        return;
    }

    const auto normalizedTitle = title.isEmpty()
        ? (url.host().isEmpty() ? QStringLiteral("New tab") : url.host())
        : title;
    if (tab->url == url && tab->title == normalizedTitle) {
        return;
    }

    // Artwork from the previous site would mislabel the tab until the new page
    // reports its own, so a move to another host drops it.
    const auto changedHost = tab->url.host() != url.host();
    tab->url = url;
    tab->title = normalizedTitle;
    if (changedHost) {
        tab->iconUrl.clear();
    }
    // A tab that has lost its address has lost its page, and the engine that
    // drew it goes with it.
    if (isBlank(url) && tabId == m_developerToolsTabId) {
        closeDeveloperTools();
    }
    m_tabs.notifyChanged(tab->id, changedHost
        ? QList<int>{TabListModel::UrlRole, TabListModel::TitleRole, TabListModel::IconUrlRole}
        : QList<int>{TabListModel::UrlRole, TabListModel::TitleRole});
    refreshSoundSuppression();
    schedulePersistTabs();
    if (tabId == m_activeTabId) {
        emit activeTabChanged();
    }
}

// Site artwork belongs to the loaded page, not to the saved session: a
// restored tab shows its lettered tile until the page hands one back.
void BrowserController::setTabIcon(const QString &tabId, const QUrl &iconUrl)
{
    auto *tab = m_tabs.find(tabId);
    if (!tab || tab->iconUrl == iconUrl) {
        return;
    }
    tab->iconUrl = iconUrl;
    m_tabs.notifyChanged(tab->id, {TabListModel::IconUrlRole});
}

void BrowserController::setTabLoading(const QString &tabId, bool loading)
{
    auto *tab = m_tabs.find(tabId);
    if (!tab || tab->loading == loading) {
        return;
    }
    tab->loading = loading;
    m_tabs.notifyChanged(tab->id, {TabListModel::LoadingRole});
}

// Whether a page is making sound is the page's to say, and it says it only
// while it has a renderer: a tab that loses its engine falls silent here too,
// or the row would keep offering to mute a page that is no longer loaded.
void BrowserController::setTabAudible(const QString &tabId, bool audible)
{
    auto *tab = m_tabs.find(tabId);
    if (!tab || tab->audible == audible) {
        return;
    }
    tab->audible = audible;
    m_tabs.notifyChanged(tab->id, {TabListModel::AudibleRole});
}

// Muting is a standing decision about the tab rather than about the page in
// it: it survives navigation within the tab, because the reader silenced this
// tab and not one particular document. A Space switch reloads its tabs from
// the store, which knows nothing of either, so a page that outlived the
// switch says both again on the way back.
void BrowserController::setTabMuted(const QString &tabId, bool muted)
{
    auto *tab = m_tabs.find(tabId);
    if (!tab || tab->muted == muted) {
        return;
    }
    tab->muted = muted;
    m_tabs.notifyChanged(tab->id, {TabListModel::MutedRole});
}

void BrowserController::toggleTabMuted(const QString &tabId)
{
    if (auto *tab = m_tabs.find(tabId)) {
        setTabMuted(tabId, !tab->muted);
    }
}

// Zoom is the reader's decision about this tab, so it survives navigation
// within the tab and is written to the session with the rest of the tab.
void BrowserController::setTabZoom(const QString &tabId, double zoom)
{
    auto *tab = m_tabs.find(tabId);
    if (!tab || qFuzzyCompare(tab->zoom, zoom)) {
        return;
    }
    tab->zoom = zoom;
    m_tabs.notifyChanged(tab->id, {TabListModel::ZoomRole});
    schedulePersistTabs();
    if (tabId == m_activeTabId) {
        emit activeTabChanged();
    }
}

// The ladder every browser zooms along. Stepping rather than multiplying keeps
// the sizes the same in both directions and keeps the ends where they are: no
// amount of asking for less makes a page smaller than a quarter.
double BrowserController::steppedZoom(double zoom, int direction)
{
    static constexpr double ladder[] = {
        0.25, 0.33, 0.5, 0.67, 0.75, 0.8, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0,
    };
    static constexpr int rungs = static_cast<int>(std::size(ladder));
    if (direction == 0) {
        return zoom;
    }
    if (direction > 0) {
        for (const auto rung : ladder) {
            if (rung > zoom + zoomTolerance) {
                return rung;
            }
        }
        return ladder[rungs - 1];
    }
    for (int index = rungs - 1; index >= 0; --index) {
        if (ladder[index] < zoom - zoomTolerance) {
            return ladder[index];
        }
    }
    return ladder[0];
}

void BrowserController::stepActiveZoom(int direction)
{
    setTabZoom(m_activeTabId, steppedZoom(activeTabZoom(), direction));
}

void BrowserController::resetActiveZoom()
{
    setTabZoom(m_activeTabId, 1.0);
}

void BrowserController::reportTabRendererFailure(const QString &tabId, const QString &reason)
{
    auto *tab = m_tabs.find(tabId);
    if (!tab) {
        return;
    }
    tab->loading = false;
    tab->rendererFailureReason = reason.isEmpty()
        ? QStringLiteral("The page renderer stopped unexpectedly.")
        : reason;
    m_tabs.notifyChanged(tab->id, {TabListModel::LoadingRole});
    if (tabId == m_activeTabId) {
        emit activeTabChanged();
    }
}

void BrowserController::recoverActiveTab()
{
    auto *tab = m_tabs.find(m_activeTabId);
    if (!tab || tab->rendererFailureReason.isEmpty()) {
        return;
    }
    tab->rendererFailureReason.clear();
    emit activeTabChanged();
    emit reloadRequested();
}

// A blank tab has no page and no engine to inspect, so there is nothing for an
// inspector to attach to.
void BrowserController::openDeveloperTools()
{
    if (m_activeTabId.isEmpty() || activeTabBlank()) {
        return;
    }
    setDeveloperToolsTab(m_activeTabId, m_activeSpaceId);
}

void BrowserController::toggleDeveloperTools()
{
    if (activeTabInspected()) {
        closeDeveloperTools();
        return;
    }
    openDeveloperTools();
}

void BrowserController::closeDeveloperTools()
{
    setDeveloperToolsTab({}, {});
}

void BrowserController::setDeveloperToolsTab(const QString &tabId, const QString &spaceId)
{
    const auto movedTabs = m_developerToolsTabId != tabId;
    m_developerToolsTabId = tabId;
    m_developerToolsSpaceId = spaceId;
    if (!movedTabs) {
        return;
    }
    // An inspected tab keeps running while another Space is active, so the
    // retained set moves with the attachment.
    refreshRetainedTabs();
    emit developerToolsChanged();
    // Which tab is inspected is also a fact about the tab on show, and the
    // interface reads it there to decide whether to dock the inspector.
    emit activeTabChanged();
}

void BrowserController::requestBack()
{
    emit backRequested();
}

void BrowserController::requestForward()
{
    emit forwardRequested();
}

void BrowserController::requestReload()
{
    emit reloadRequested();
}

void BrowserController::requestReloadBypassingCache()
{
    emit reloadBypassingCacheRequested();
}

void BrowserController::requestStopLoading()
{
    emit stopLoadingRequested();
}

void BrowserController::recordVisit(const QUrl &url, const QString &title)
{
    if (m_privateBrowsing || url.scheme() == QStringLiteral("about")
        || normalizedOrigin(url).isEmpty()) {
        return;
    }
    m_store.recordVisit(m_activeSpaceId, url,
        title.isEmpty() ? url.host() : title);
}

QVariantList BrowserController::historySuggestions(const QString &query, int limit) const
{
    if (m_privateBrowsing || limit <= 0) {
        return {};
    }
    return m_store.historySuggestions(m_activeSpaceId, query.trimmed(), limit);
}

QVariantList BrowserController::history(const QString &query, int limit) const
{
    if (m_privateBrowsing || limit <= 0) {
        return {};
    }
    return m_store.history(m_activeSpaceId, query.trimmed(), limit);
}

bool BrowserController::deleteHistoryVisit(qint64 id)
{
    return !m_privateBrowsing && id > 0 && m_store.deleteHistoryVisit(m_activeSpaceId, id);
}

bool BrowserController::deleteHistoryOrigin(const QUrl &url)
{
    const auto origin = normalizedOrigin(url);
    return !m_privateBrowsing && !origin.isEmpty()
        && m_store.deleteHistoryOrigin(m_activeSpaceId, origin);
}

bool BrowserController::deleteHistorySince(qint64 since)
{
    return !m_privateBrowsing && m_store.deleteHistorySince(m_activeSpaceId, since);
}

QVariantList BrowserController::searchEngines() const
{
    QVariantList engines;
    for (const auto &value : m_searchEngines) {
        auto engine = value.toMap();
        engine.insert(QStringLiteral("default"),
            engine.value(QStringLiteral("id")).toString() == m_defaultSearchEngineId);
        engines.append(engine);
    }
    return engines;
}

QVariantList BrowserController::searchEnginePresets() const
{
    return predefinedSearchEngines();
}

bool BrowserController::addSearchEnginePreset(const QString &id)
{
    for (const auto &configured : m_searchEngines) {
        if (configured.toMap().value(QStringLiteral("id")).toString() == id) {
            return false;
        }
    }
    for (const auto &preset : predefinedSearchEngines()) {
        if (preset.toMap().value(QStringLiteral("id")).toString() != id) {
            continue;
        }
        auto engines = m_searchEngines;
        engines.append(preset);
        return saveSearchEngines(engines, m_defaultSearchEngineId);
    }
    return false;
}

bool BrowserController::addSearchEngine(const QString &name, const QString &queryUrl,
    const QString &keyword)
{
    const auto normalizedName = name.trimmed();
    auto id = normalizedName.toLower();
    id.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    id.remove(QRegularExpression(QStringLiteral("^-|-$")));
    if (id.isEmpty()) {
        id = QStringLiteral("search");
    }
    const auto baseId = id;
    int suffix = 2;
    QSet<QString> ids;
    for (const auto &value : m_searchEngines) {
        ids.insert(value.toMap().value(QStringLiteral("id")).toString());
    }
    while (ids.contains(id)) {
        id = QStringLiteral("%1-%2").arg(baseId).arg(suffix++);
    }
    auto engines = m_searchEngines;
    engines.append(QVariantMap{{QStringLiteral("id"), id},
        {QStringLiteral("name"), normalizedName},
        {QStringLiteral("queryUrl"), queryUrl.trimmed()},
        {QStringLiteral("keyword"), keyword.trimmed()}});
    return saveSearchEngines(engines, id);
}

bool BrowserController::deleteSearchEngine(const QString &id)
{
    if (m_privateBrowsing || m_searchEngines.size() <= 1) {
        return false;
    }
    QVariantList engines;
    for (const auto &value : m_searchEngines) {
        if (value.toMap().value(QStringLiteral("id")).toString() != id) {
            engines.append(value);
        }
    }
    if (engines.size() == m_searchEngines.size()) {
        return false;
    }
    const auto defaultId = id == m_defaultSearchEngineId
        ? engines.first().toMap().value(QStringLiteral("id")).toString()
        : m_defaultSearchEngineId;
    return saveSearchEngines(engines, defaultId);
}

bool BrowserController::setDefaultSearchEngine(const QString &id)
{
    return saveSearchEngines(m_searchEngines, id);
}

bool BrowserController::saveSearchEngines(const QVariantList &engines,
    const QString &defaultEngineId)
{
    if (m_privateBrowsing || engines.isEmpty()) {
        return false;
    }
    QJsonArray jsonEngines;
    QSet<QString> ids;
    QSet<QString> keywords;
    bool foundDefault = false;
    for (const auto &value : engines) {
        const auto engine = value.toMap();
        const auto id = engine.value(QStringLiteral("id")).toString().trimmed();
        const auto name = engine.value(QStringLiteral("name")).toString().trimmed();
        const auto queryUrl = engine.value(QStringLiteral("queryUrl")).toString().trimmed();
        const auto keyword = engine.value(QStringLiteral("keyword")).toString().trimmed();
        if (id.isEmpty() || name.isEmpty() || !queryUrl.contains(QStringLiteral("{query}"))
            || !QUrl(queryUrl).isValid() || ids.contains(id)
            || (!keyword.isEmpty() && keywords.contains(keyword))) {
            return false;
        }
        ids.insert(id);
        if (!keyword.isEmpty()) {
            keywords.insert(keyword);
        }
        foundDefault = foundDefault || id == defaultEngineId;
        jsonEngines.append(QJsonObject{{QStringLiteral("id"), id},
            {QStringLiteral("name"), name}, {QStringLiteral("queryUrl"), queryUrl},
            {QStringLiteral("keyword"), keyword}});
    }
    if (!foundDefault || !QDir().mkpath(m_configRoot)) {
        return false;
    }
    QSaveFile file(QDir(m_configRoot).filePath(QStringLiteral("search-engines.json")));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
        {QStringLiteral("default"), defaultEngineId},
        {QStringLiteral("engines"), jsonEngines}}).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return false;
    }
    m_searchEngines = engines;
    m_defaultSearchEngineId = defaultEngineId;
    return true;
}

bool BrowserController::clearBrowsingData(const QStringList &dataTypes, qint64 since,
    bool everySpace, const QString &confirmation)
{
    static const QSet<QString> allowedTypes{QStringLiteral("cookies"),
        QStringLiteral("storage"), QStringLiteral("cache"),
        QStringLiteral("permissions"), QStringLiteral("history")};
    if (m_privateBrowsing || dataTypes.isEmpty() || since < 0
        || (everySpace && confirmation != QStringLiteral("CLEAR ALL"))) {
        return false;
    }
    for (const auto &dataType : dataTypes) {
        if (!allowedTypes.contains(dataType)) {
            return false;
        }
    }
    QStringList spaceIds{m_activeSpaceId};
    if (everySpace) {
        spaceIds.clear();
        for (const auto &space : m_store.loadSpaces()) {
            spaceIds.append(space.id);
        }
    }
    bool cleared = true;
    for (const auto &spaceId : spaceIds) {
        if (dataTypes.contains(QStringLiteral("history"))) {
            cleared = m_store.deleteHistorySince(spaceId, since) && cleared;
        }
        if (dataTypes.contains(QStringLiteral("permissions"))) {
            cleared = m_store.clearPermissionsSince(spaceId, since) && cleared;
            const auto prefix = spaceId + QChar(0x1f);
            m_sessionPermissionDecisions->removeIf(
                [&prefix](auto it) { return it.key().startsWith(prefix); });
        }
    }
    if (!cleared) {
        return false;
    }
    emit engineDataClearRequested(spaceIds, dataTypes, since);
    return true;
}

int BrowserController::permissionDecision(const QUrl &url, const QString &permission)
{
    const auto origin = normalizedOrigin(url);
    const auto normalizedPermission = permission.trimmed().toLower();
    if (origin.isEmpty() || normalizedPermission.isEmpty()) {
        return Ask;
    }
    const auto key = sessionPermissionKey(origin, normalizedPermission);
    const auto sessionDecision = m_sessionPermissionDecisions->take(key);
    if (sessionDecision != Ask) {
        return sessionDecision;
    }
    if (m_privateBrowsing) {
        return Ask;
    }
    return m_store.permissionDecision(m_activeSpaceId, origin, normalizedPermission);
}

bool BrowserController::setPermissionDecision(const QUrl &url, const QString &permission,
    int decision)
{
    const auto origin = normalizedOrigin(url);
    const auto normalizedPermission = permission.trimmed().toLower();
    if (origin.isEmpty() || normalizedPermission.isEmpty()
        || decision < AllowOnce || decision > Block) {
        return false;
    }
    if (decision == AllowOnce || m_privateBrowsing) {
        m_sessionPermissionDecisions->insert(
            sessionPermissionKey(origin, normalizedPermission), decision);
        return true;
    }
    return m_store.savePermissionDecision(
        m_activeSpaceId, origin, normalizedPermission, decision);
}

bool BrowserController::externalProtocolAllowed(const QUrl &url, const QString &scheme) const
{
    const auto origin = normalizedOrigin(url);
    const auto normalizedScheme = scheme.trimmed().toLower();
    if (origin.isEmpty() || normalizedScheme.isEmpty()) {
        return false;
    }
    const auto permission = QStringLiteral("external-protocol/%1").arg(normalizedScheme);
    const auto sessionDecision = m_sessionPermissionDecisions->value(
        sessionPermissionKey(origin, permission), Ask);
    if (sessionDecision == AllowPersistently) {
        return true;
    }
    return !m_privateBrowsing
        && m_store.permissionDecision(m_activeSpaceId, origin, permission)
            == AllowPersistently;
}

bool BrowserController::rememberExternalProtocolDecision(const QUrl &url,
    const QString &scheme)
{
    const auto origin = normalizedOrigin(url);
    const auto normalizedScheme = scheme.trimmed().toLower();
    if (origin.isEmpty() || normalizedScheme.isEmpty()) {
        return false;
    }
    const auto permission = QStringLiteral("external-protocol/%1").arg(normalizedScheme);
    if (m_privateBrowsing) {
        m_sessionPermissionDecisions->insert(
            sessionPermissionKey(origin, permission), AllowPersistently);
        return true;
    }
    return m_store.savePermissionDecision(
        m_activeSpaceId, origin, permission, AllowPersistently);
}

QString BrowserController::recordDownload(const QString &runtimeId, const QUrl &url,
    const QString &path, const QString &state, qint64 receivedBytes, qint64 totalBytes)
{
    if (m_privateBrowsing || runtimeId.isEmpty() || !url.isValid()) {
        return {};
    }
    const auto recordId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return m_store.recordDownload(recordId, url, path, state, receivedBytes, totalBytes)
        ? recordId : QString{};
}

bool BrowserController::updateDownload(const QString &id, const QString &state,
    qint64 receivedBytes, qint64 totalBytes, const QString &error)
{
    if (m_privateBrowsing) {
        return false;
    }
    return m_store.updateDownload(id, state, receivedBytes, totalBytes, error);
}

QVariantList BrowserController::downloadHistory() const
{
    if (m_privateBrowsing) {
        return {};
    }
    return m_store.downloadHistory();
}

// A Private window has no store to read or write, so it browses on the
// defaults and leaves nothing of itself behind.
QString BrowserController::preference(const QString &name, const QString &fallback) const
{
    if (m_privateBrowsing || !m_ready) {
        return fallback;
    }
    return m_store.preference(name, fallback);
}

bool BrowserController::setPreference(const QString &name, const QString &value)
{
    if (m_privateBrowsing || !m_ready) {
        return false;
    }
    return m_store.savePreference(name, value);
}

void BrowserController::initialize()
{
    if (m_privateBrowsing) {
        auto tab = makeBlankTab({});
        m_activeTabId = tab.id;
        m_tabs.reset({tab});
        loadSearchEngines();
        m_ready = true;
        return;
    }
    if (!m_store.open(&m_errorMessage)) {
        return;
    }
    ensureDefaultSpace();
    ensureActiveTab();
    loadClosedTabs();
    // A Pinned tab marked Keep active is running before its Space is ever
    // selected, so what the session restores is known from the first moment.
    refreshRetainedTabs();
    loadSearchEngines();
    m_ready = true;
}

void BrowserController::ensureDefaultSpace()
{
    auto spaces = m_store.loadSpaces();
    if (spaces.isEmpty()) {
        SpaceState personal;
        personal.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        personal.name = QStringLiteral("Personal");
        personal.color = QStringLiteral("#7c6cff");
        personal.active = true;
        m_store.saveSpace(personal);
        spaces.append(personal);
    }

    auto active = spaces.cbegin();
    for (auto it = spaces.cbegin(); it != spaces.cend(); ++it) {
        if (it->active) {
            active = it;
            break;
        }
    }
    m_activeSpaceId = active->id;
    m_activeSpaceName = active->name;
    m_spaces.reset(std::move(spaces));
}

void BrowserController::ensureActiveTab()
{
    auto tabs = m_store.loadTabs(m_activeSpaceId);
    if (tabs.isEmpty()) {
        auto tab = makeBlankTab(m_activeSpaceId);
        tabs.append(tab);
        m_store.saveTab(tab, 0);
    }

    auto active = tabs.cbegin();
    for (auto it = tabs.cbegin(); it != tabs.cend(); ++it) {
        if (it->active) {
            active = it;
            break;
        }
    }
    m_activeTabId = active->id;
    m_tabs.reset(std::move(tabs));
    refreshSoundSuppression();
    persistTabs();
}

bool BrowserController::persistTabs()
{
    m_persistTabsTimer.stop();
    if (m_privateBrowsing) {
        return true;
    }
    return m_store.saveTabs(m_activeSpaceId, m_tabs.items(), m_activeTabId);
}

// A loading page reports a new address and then several titles in quick
// succession, and each report used to rewrite the whole Space's tab table.
// The session only has to survive a quit, so the writes are coalesced; the
// paths that must know the write succeeded still call persistTabs directly.
void BrowserController::schedulePersistTabs()
{
    if (m_privateBrowsing) {
        return;
    }
    m_persistTabsTimer.start();
}

void BrowserController::setActiveTab(const QString &tabId)
{
    for (const auto &item : m_tabs.items()) {
        if (auto *tab = m_tabs.find(item.id)) {
            const auto shouldBeActive = tab->id == tabId;
            if (tab->active != shouldBeActive) {
                tab->active = shouldBeActive;
                m_tabs.notifyChanged(tab->id, {TabListModel::ActiveRole});
            }
        }
    }
    m_activeTabId = tabId;
    schedulePersistTabs();
    emit activeTabChanged();
}

bool BrowserController::loadSearchEngines()
{
    auto duckDuckGo = predefinedSearchEngines().first().toMap();
    duckDuckGo.insert(QStringLiteral("keyword"), QString{});
    const auto path = QDir(m_configRoot).filePath(QStringLiteral("search-engines.json"));
    if (m_configRoot.isEmpty()) {
        m_searchEngines = {duckDuckGo};
        m_defaultSearchEngineId = QStringLiteral("duckduckgo");
        return true;
    }
    QFile file(path);
    if (!file.exists()) {
        if (m_privateBrowsing) {
            m_searchEngines = {duckDuckGo};
            m_defaultSearchEngineId = QStringLiteral("duckduckgo");
            return true;
        }
        return saveSearchEngines({duckDuckGo}, QStringLiteral("duckduckgo"));
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_searchEngines = {duckDuckGo};
        m_defaultSearchEngineId = QStringLiteral("duckduckgo");
        return false;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    const auto object = document.object();
    QVariantList engines;
    QSet<QString> ids;
    QSet<QString> keywords;
    bool valid = error.error == QJsonParseError::NoError;
    for (const auto value : object.value(QStringLiteral("engines")).toArray()) {
        const auto engine = value.toObject().toVariantMap();
        const auto id = engine.value(QStringLiteral("id")).toString().trimmed();
        const auto name = engine.value(QStringLiteral("name")).toString().trimmed();
        const auto queryUrl = engine.value(QStringLiteral("queryUrl")).toString().trimmed();
        const auto keyword = engine.value(QStringLiteral("keyword")).toString().trimmed();
        valid = valid && !id.isEmpty() && !name.isEmpty()
            && queryUrl.contains(QStringLiteral("{query}")) && QUrl(queryUrl).isValid()
            && !ids.contains(id) && (keyword.isEmpty() || !keywords.contains(keyword));
        ids.insert(id);
        if (!keyword.isEmpty()) {
            keywords.insert(keyword);
        }
        engines.append(engine);
    }
    const auto defaultId = object.value(QStringLiteral("default")).toString();
    if (!valid || engines.isEmpty() || !ids.contains(defaultId)) {
        m_searchEngines = {duckDuckGo};
        m_defaultSearchEngineId = QStringLiteral("duckduckgo");
        return false;
    }
    m_searchEngines = engines;
    m_defaultSearchEngineId = defaultId;
    if (object.value(QStringLiteral("version")).toInt() != 1) {
        return saveSearchEngines(engines, defaultId);
    }
    return true;
}

QUrl BrowserController::resolveConfiguredInput(const QString &input) const
{
    const auto value = input.trimmed();
    if (value.isEmpty()) {
        return {};
    }

    static const QRegularExpression explicitScheme(
        QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
    static const QRegularExpression hostWithPort(QStringLiteral("^[^/\\s:]+:[0-9]+(?:/|$)"));
    if (explicitScheme.match(value).hasMatch() && !hostWithPort.match(value).hasMatch()) {
        return QUrl(value);
    }

    const auto authority = value.split(QRegularExpression(QStringLiteral("[/?#]"))).first();
    auto host = authority;
    if (host.startsWith('[') && host.contains(']')) {
        host = host.mid(1, host.indexOf(']') - 1);
    } else if (host.count(':') == 1) {
        host = host.section(':', 0, 0);
    }
    QHostAddress literal;
    const auto explicitPort = authority.contains(QRegularExpression(
        QStringLiteral("(?:\\]|[^:]):[0-9]+$")));
    const auto localAddress = host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0
        || host.endsWith(QStringLiteral(".localhost"), Qt::CaseInsensitive)
        || host.endsWith(QStringLiteral(".test"), Qt::CaseInsensitive)
        || literal.setAddress(host);
    const auto publicAddress = !value.contains(QRegularExpression(QStringLiteral("\\s")))
        && host.contains('.') && !host.startsWith('.') && !host.endsWith('.');
    if (localAddress || publicAddress || explicitPort) {
        const auto insecureLocal = localAddress || (explicitPort && !publicAddress);
        auto address = value;
        if (literal.protocol() == QAbstractSocket::IPv6Protocol && !authority.startsWith('[')) {
            address = QStringLiteral("[%1]%2").arg(authority, value.mid(authority.size()));
        }
        return QUrl((insecureLocal ? QStringLiteral("http://") : QStringLiteral("https://"))
            + address);
    }

    auto terms = value;
    QVariantMap selected;
    const auto firstSpace = value.indexOf(' ');
    if (firstSpace > 0) {
        const auto keyword = value.left(firstSpace);
        for (const auto &candidate : m_searchEngines) {
            if (candidate.toMap().value(QStringLiteral("keyword")).toString() == keyword) {
                selected = candidate.toMap();
                terms = value.mid(firstSpace + 1).trimmed();
                break;
            }
        }
    }
    if (selected.isEmpty()) {
        for (const auto &candidate : m_searchEngines) {
            if (candidate.toMap().value(QStringLiteral("id")).toString()
                == m_defaultSearchEngineId) {
                selected = candidate.toMap();
                break;
            }
        }
    }
    if (selected.isEmpty()) {
        return resolveInput(value);
    }
    auto queryUrl = selected.value(QStringLiteral("queryUrl")).toString().toUtf8();
    queryUrl.replace("{query}", QUrl::toPercentEncoding(terms));
    return QUrl::fromEncoded(queryUrl);
}

QUrl BrowserController::resolveInput(const QString &input)
{
    QUrl search(QStringLiteral("https://duckduckgo.com/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), input.trimmed());
    search.setQuery(query);
    return search;
}

QString BrowserController::normalizedOrigin(const QUrl &url)
{
    const auto scheme = url.scheme().toLower();
    if ((scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
        || url.host().isEmpty()) {
        return {};
    }
    QUrl origin;
    origin.setScheme(scheme);
    origin.setHost(url.host().toLower());
    const auto port = url.port(-1);
    const auto defaultPort = scheme == QStringLiteral("https") ? 443 : 80;
    if (port != -1 && port != defaultPort) {
        origin.setPort(port);
    }
    return origin.toString(QUrl::FullyEncoded);
}

QString BrowserController::sessionPermissionKey(const QString &origin,
    const QString &permission) const
{
    return m_activeSpaceId + QChar(0x1f) + origin + QChar(0x1f) + permission;
}

bool BrowserController::isBlank(const QUrl &url)
{
    return url.isEmpty() || url == QUrl(QStringLiteral("about:blank"));
}

// Pinned tabs are the Space's own furniture and say nothing about whether the
// reader has opened anything, so rest is decided on the ordinary tabs alone.
bool BrowserController::restingOnBlankTab() const
{
    const TabState *ordinary = nullptr;
    for (const auto &tab : m_tabs.items()) {
        if (tab.pinned) {
            continue;
        }
        if (ordinary) {
            return false;
        }
        ordinary = &tab;
    }
    return ordinary && isBlank(ordinary->url);
}

void BrowserController::refreshAtRest()
{
    const auto resting = restingOnBlankTab();
    if (resting == m_atRest) {
        return;
    }
    m_atRest = resting;
    emit atRestChanged();
}

TabState BrowserController::makeBlankTab(const QString &spaceId)
{
    TabState tab;
    tab.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tab.spaceId = spaceId;
    tab.url = QUrl(QStringLiteral("about:blank"));
    tab.title = QStringLiteral("New tab");
    tab.active = true;
    return tab;
}

} // namespace omaweb
