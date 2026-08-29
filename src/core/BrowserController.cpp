#include "BrowserController.h"

#include <QUrlQuery>
#include <QUuid>

namespace tanto {

BrowserController::BrowserController(QString dataRoot, QString engineName, QObject *parent)
    : QObject(parent)
    , m_store(std::move(dataRoot))
    , m_engineName(std::move(engineName))
{
    initialize();
}

QAbstractItemModel *BrowserController::spaces()
{
    return &m_spaces;
}

QAbstractItemModel *BrowserController::tabs()
{
    return &m_tabs;
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
    return m_store.engineProfilePath(m_activeSpaceId, m_engineName);
}

bool BrowserController::activeTabPinned() const
{
    const auto *tab = m_tabs.find(m_activeTabId);
    return tab && tab->pinned;
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

bool BrowserController::ready() const
{
    return m_ready;
}

QString BrowserController::errorMessage() const
{
    return m_errorMessage;
}

void BrowserController::activateTab(const QString &tabId)
{
    if (tabId == m_activeTabId || !m_tabs.find(tabId)) {
        return;
    }
    setActiveTab(tabId);
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

    persistTabs();
    emit activeTabChanged();
}

void BrowserController::closeActiveTab()
{
    auto *active = m_tabs.find(m_activeTabId);
    if (!active || active->pinned) {
        return;
    }

    if (m_tabs.rowCount() == 1) {
        active->url = QUrl(QStringLiteral("about:blank"));
        active->title = QStringLiteral("New tab");
        active->loading = false;
        active->rendererFailureReason.clear();
        m_tabs.notifyChanged(active->id, {
            TabListModel::UrlRole,
            TabListModel::TitleRole,
            TabListModel::LoadingRole,
        });
        persistTabs();
        emit activeTabChanged();
        return;
    }

    m_closedTab = {*active, true};
    const auto items = m_tabs.items();
    auto nextId = items.first().id;
    for (qsizetype index = 0; index < items.size(); ++index) {
        if (items.at(index).id != m_activeTabId) {
            continue;
        }
        nextId = items.at(index == 0 ? 1 : index - 1).id;
        break;
    }

    m_tabs.remove(m_activeTabId);
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
    auto *tab = m_tabs.find(m_activeTabId);
    if (!tab) {
        return;
    }
    tab->pinned = !tab->pinned;
    m_tabs.notifyChanged(tab->id, {TabListModel::PinnedRole});
    persistTabs();
    emit activeTabChanged();
}

void BrowserController::updateActiveTab(const QUrl &url, const QString &title)
{
    auto *tab = m_tabs.find(m_activeTabId);
    if (!tab) {
        return;
    }

    const auto normalizedTitle = title.isEmpty()
        ? (url.host().isEmpty() ? QStringLiteral("New tab") : url.host())
        : title;
    if (tab->url == url && tab->title == normalizedTitle) {
        return;
    }

    tab->url = url;
    tab->title = normalizedTitle;
    m_tabs.notifyChanged(tab->id, {TabListModel::UrlRole, TabListModel::TitleRole});
    persistTabs();
    emit activeTabChanged();
}

void BrowserController::setActiveLoading(bool loading)
{
    auto *tab = m_tabs.find(m_activeTabId);
    if (!tab || tab->loading == loading) {
        return;
    }
    tab->loading = loading;
    m_tabs.notifyChanged(tab->id, {TabListModel::LoadingRole});
}

void BrowserController::reportRendererFailure(const QString &reason)
{
    auto *tab = m_tabs.find(m_activeTabId);
    if (!tab) {
        return;
    }
    tab->loading = false;
    tab->rendererFailureReason = reason.isEmpty()
        ? QStringLiteral("The page renderer stopped unexpectedly.")
        : reason;
    m_tabs.notifyChanged(tab->id, {TabListModel::LoadingRole});
    emit activeTabChanged();
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

void BrowserController::initialize()
{
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
        TabState tab;
        tab.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        tab.spaceId = m_activeSpaceId;
        tab.url = QUrl(QStringLiteral("about:blank"));
        tab.title = QStringLiteral("New tab");
        tab.active = true;
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

void BrowserController::persistTabs()
{
    m_store.saveTabs(m_activeSpaceId, m_tabs.items(), m_activeTabId);
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
    persistTabs();
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

} // namespace tanto
