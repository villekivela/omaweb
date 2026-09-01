#include "BrowserController.h"

#include <QRegularExpression>
#include <QUrlQuery>
#include <QUuid>
#include <QStandardPaths>

namespace tanto {
namespace {

constexpr int persistTabsDelayMilliseconds = 400;

} // namespace

BrowserController::BrowserController(QString dataRoot, QString engineName, QObject *parent)
    : BrowserController(std::move(dataRoot), std::move(engineName), false, parent)
{
}

BrowserController::BrowserController(QString dataRoot, QString engineName,
    bool privateBrowsing, QObject *parent)
    : BrowserController(std::move(dataRoot), std::move(engineName), privateBrowsing,
        QSharedPointer<QHash<QString, int>>::create(), parent)
{
}

BrowserController::BrowserController(QString dataRoot, QString engineName,
    bool privateBrowsing, QSharedPointer<QHash<QString, int>> sessionPermissionDecisions,
    QObject *parent)
    : QObject(parent)
    , m_store(std::move(dataRoot))
    , m_engineName(std::move(engineName))
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

bool BrowserController::activeTabPinned() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return tab && tab->pinned;
}

bool BrowserController::activeTabBlank() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return !tab || isBlank(tab->url);
}

bool BrowserController::atRest() const
{
    return m_atRest;
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
    emit spaceSuspended(m_activeSpaceId);
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
    m_closedTab = {};
    ensureActiveTab();
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
    if (m_store.spaceHasSavedContent(spaceId) && confirmationName != target->name) {
        return false;
    }

    const auto deletingActiveSpace = spaceId == m_activeSpaceId;
    if (deletingActiveSpace && !persistTabs()) {
        return false;
    }
    if (deletingActiveSpace) {
        emit spaceSuspended(spaceId);
    }
    if (!m_store.deleteSpace(spaceId, deletingActiveSpace ? replacementId : QString{})) {
        if (deletingActiveSpace) {
            emit spaceRestored(spaceId);
        }
        return false;
    }
    m_spaces.reset(m_store.loadSpaces());
    // Nothing belonging to a deleted Space should outlive it, including the
    // pages a window is still holding open for it.
    emit spaceDiscarded(spaceId);
    if (deletingActiveSpace) {
        m_activeSpaceId = replacementId;
        m_activeSpaceName = replacementName;
        m_closedTab = {};
        ensureActiveTab();
        emit spaceRestored(m_activeSpaceId);
        emit activeSpaceChanged();
        emit activeTabChanged();
    }
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

    m_activeTabId = sourceActiveTabId;
    m_tabs.reset(std::move(sourceTabs));
    emit activeTabChanged();
    return true;
}

void BrowserController::openInput(const QString &input, bool inNewTab)
{
    const auto url = resolveInput(input);
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

    schedulePersistTabs();
    emit activeTabChanged();
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

    if (m_tabs.rowCount() == 1) {
        if (m_privateBrowsing) {
            emit closeWindowRequested();
            return;
        }
        tab->url = QUrl(QStringLiteral("about:blank"));
        tab->title = QStringLiteral("New tab");
        tab->loading = false;
        tab->rendererFailureReason.clear();
        m_tabs.notifyChanged(tab->id, {
            TabListModel::UrlRole,
            TabListModel::TitleRole,
            TabListModel::LoadingRole,
        });
        schedulePersistTabs();
        emit activeTabChanged();
        return;
    }

    m_closedTab = {*tab, true};
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

void BrowserController::reopenClosedTab()
{
    if (!m_closedTab.valid) {
        return;
    }
    if (auto *current = m_tabs.find(m_activeTabId)) {
        current->active = false;
        m_tabs.notifyChanged(current->id, {TabListModel::ActiveRole});
    }
    m_closedTab.tab.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_closedTab.tab.active = true;
    m_tabs.append(m_closedTab.tab);
    m_activeTabId = m_closedTab.tab.id;
    m_closedTab.valid = false;
    persistTabs();
    emit activeTabChanged();
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
    m_tabs.notifyChanged(tab->id, {TabListModel::PinnedRole});
    persistTabs();
    emit activeTabChanged();
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
    m_tabs.notifyChanged(tab->id, changedHost
        ? QList<int>{TabListModel::UrlRole, TabListModel::TitleRole, TabListModel::IconUrlRole}
        : QList<int>{TabListModel::UrlRole, TabListModel::TitleRole});
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
        m_ready = true;
        return;
    }
    if (!m_store.open(&m_errorMessage)) {
        return;
    }
    ensureDefaultSpace();
    ensureActiveTab();
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

QUrl BrowserController::resolveInput(const QString &input)
{
    const auto value = input.trimmed();
    if (value.isEmpty()) {
        return {};
    }

    const auto direct = QUrl::fromUserInput(value);
    const auto looksLikeAddress = value.contains(QStringLiteral("://"))
        || value.contains('.')
        || value.startsWith(QStringLiteral("localhost"))
        || value.startsWith(QStringLiteral("about:"));
    if (looksLikeAddress && direct.isValid()) {
        return direct;
    }

    QUrl search(QStringLiteral("https://duckduckgo.com/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), value);
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

} // namespace tanto
