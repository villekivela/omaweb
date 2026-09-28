#include "AgentControl.h"

#include "BrowserController.h"
#include "PrivacyFile.h"

#include <QAbstractItemModel>
#include <QJsonArray>
#include <QRegularExpression>

#include <utility>

namespace omaweb {
namespace {

    constexpr QLatin1StringView allowAgentsKey("allow-agents");

    const auto defaultAgentSpaceName = QStringLiteral("Agent");

    QJsonObject success(QJsonObject fields = {})
    {
        fields.insert(QStringLiteral("ok"), true);
        return fields;
    }

    QJsonObject refusal(const QString &code, const QString &error)
    {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("code"), code},
            {QStringLiteral("error"), error},
        };
    }

    // An address a browser command may load. `javascript:` runs in whatever
    // page the tab holds and `data:` is a document the Agent wrote, and both
    // are acting inside a page rather than opening one.
    //
    // Asked of the input as well as of what it resolves to, because input that
    // is not an address becomes a search, and a search for `javascript:...`
    // would otherwise be let through as one.
    bool openable(const QString &input, const QUrl &url)
    {
        static const QSet<QString> schemes {QStringLiteral("http"), QStringLiteral("https"),
            QStringLiteral("file"), QStringLiteral("about")};
        // `localhost:3000` names a port, and `example.com:8080` a host.
        static const QRegularExpression namedScheme(
            QStringLiteral("^([A-Za-z][A-Za-z0-9+-]*):(?!\\d)"));
        const auto named = namedScheme.match(input.trimmed());
        if (named.hasMatch() && !schemes.contains(named.captured(1).toLower())) {
            return false;
        }
        return url.isValid() && schemes.contains(url.scheme());
    }

} // namespace

AgentControl::AgentControl(BrowserController *browser, QString configRoot, QObject *parent)
    : QObject(parent)
    , m_browser(browser)
    , m_configRoot(std::move(configRoot))
{
    // Only an explicit `true` lets Agents in.
    m_allowAgents = PrivacyFile::read(m_configRoot, allowAgentsKey).toBool(false);
}

bool AgentControl::allowAgents() const { return m_allowAgents; }

void AgentControl::setAllowAgents(bool allowed)
{
    if (allowed == m_allowAgents) {
        return;
    }
    m_allowAgents = allowed;
    PrivacyFile::write(m_configRoot, allowAgentsKey, allowed);
    if (!allowed) {
        for (auto &connection : m_connections) {
            connection.currentTabId.clear();
        }
    }
    emit allowAgentsChanged();
}

bool AgentControl::gated(const QString &verb)
{
    return verb == u"space new" || verb == u"space delete";
}

QJsonObject AgentControl::answer(const QJsonObject &request)
{
    const auto verb = request.value(QStringLiteral("verb")).toString();
    static const QSet<QString> verbs {QStringLiteral("spaces"), QStringLiteral("tabs"),
        QStringLiteral("open"), QStringLiteral("close"), QStringLiteral("space new"),
        QStringLiteral("space delete")};
    if (!verbs.contains(verb)) {
        return refusal(
            QStringLiteral("bad-request"), QStringLiteral("Omaweb has no verb \"%1\".").arg(verb));
    }
    if (!m_browser || m_browser->privateBrowsing()) {
        return refusal(
            QStringLiteral("private"), QStringLiteral("A Private window is never reachable."));
    }
    if (gated(verb) && !m_allowAgents) {
        return refusal(QStringLiteral("allow-agents"),
            QStringLiteral("Allow agents is off. The reader must turn it on before an Agent "
                           "can use Agent Spaces."));
    }

    auto &connection = m_connections[request.value(QStringLiteral("name")).toString()];
    // A tab the reader closed since is no longer anyone's current tab.
    if (!connection.currentTabId.isEmpty() && !m_browser->findTab(connection.currentTabId)) {
        connection.currentTabId.clear();
    }
    if (verb == u"spaces") {
        return listSpaces();
    }
    if (verb == u"tabs") {
        return listTabs(connection, request);
    }
    if (verb == u"open") {
        return open(connection, request);
    }
    if (verb == u"close") {
        return close(connection, request);
    }
    if (verb == u"space new") {
        return createSpace(connection, request);
    }
    return deleteSpace(connection, request);
}

QJsonObject AgentControl::listSpaces() const
{
    QJsonArray spaces;
    const auto *model = m_browser->spaces();
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto index = model->index(row, 0);
        const auto id = index.data(SpaceListModel::IdRole).toString();
        spaces.append(QJsonObject {
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), index.data(SpaceListModel::NameRole).toString()},
            {QStringLiteral("onShow"), id == m_browser->activeSpaceId()},
            {QStringLiteral("agent"), m_browser->agentSpace(id)},
        });
    }
    return success({{QStringLiteral("spaces"), spaces}});
}

QJsonObject AgentControl::listTabs(Connection &connection, const QJsonObject &request) const
{
    const auto named = request.value(QStringLiteral("space")).toString();
    const auto spaceId = named.isEmpty() ? defaultSpace(connection) : findSpace(named);
    if (spaceId.isEmpty()) {
        return refusal(
            QStringLiteral("not-found"), QStringLiteral("There is no Space \"%1\".").arg(named));
    }
    QJsonArray tabs;
    for (const auto &tab : m_browser->spaceTabs(spaceId)) {
        tabs.append(describeTab(tab.id, connection));
    }
    return success({{QStringLiteral("space"), spaceId}, {QStringLiteral("tabs"), tabs}});
}

// Opening an address is a browser command, as the desktop's own handover is.
// It never selects the tab, so the page in front of the reader stays there.
QJsonObject AgentControl::open(Connection &connection, const QJsonObject &request)
{
    const auto input = request.value(QStringLiteral("url")).toString();
    const auto url = m_browser->resolveAddress(input);
    if (!url.isValid()) {
        return refusal(QStringLiteral("bad-request"), QStringLiteral("Give an address to open."));
    }
    if (!openable(input, url)) {
        return refusal(QStringLiteral("refused"),
            QStringLiteral("Omaweb opens only http, https, file and about addresses."));
    }

    const auto tabName = request.value(QStringLiteral("tab")).toString();
    const auto spaceName = request.value(QStringLiteral("space")).toString();
    if (!tabName.isEmpty() && !spaceName.isEmpty()) {
        return refusal(QStringLiteral("bad-request"),
            QStringLiteral("Name a tab or a Space to open in, not both."));
    }
    // A named tab, or the current one, moves to the address. Otherwise the
    // address gets a tab of its own.
    const auto newTab = !spaceName.isEmpty() || request.value(QStringLiteral("new")).toBool()
        || (tabName.isEmpty() && connection.currentTabId.isEmpty());
    if (!newTab) {
        const auto tabId = tabName.isEmpty() ? connection.currentTabId : tabName;
        const auto tab = m_browser->findTab(tabId);
        if (!tab) {
            return refusal(
                QStringLiteral("not-found"), QStringLiteral("There is no tab \"%1\".").arg(tabId));
        }
        if (tab->pinned) {
            return refusal(QStringLiteral("refused"),
                QStringLiteral("A Pinned tab's address is the reader's to change."));
        }
        if (!m_browser->navigateTab(tabId, url)) {
            return refusal(
                QStringLiteral("failed"), QStringLiteral("The tab could not be loaded."));
        }
        connection.currentTabId = tabId;
        connection.currentSpaceId = tab->spaceId;
        return success({{QStringLiteral("tab"), describeTab(tabId, connection)}});
    }

    const auto spaceId = spaceName.isEmpty() ? defaultSpace(connection) : findSpace(spaceName);
    if (spaceId.isEmpty()) {
        return refusal(QStringLiteral("not-found"),
            QStringLiteral("There is no Space \"%1\".").arg(spaceName));
    }
    const auto tabId = m_browser->openTabInSpace(spaceId, url);
    if (tabId.isEmpty()) {
        return refusal(QStringLiteral("failed"), QStringLiteral("The tab could not be opened."));
    }
    m_openedTabIds.insert(tabId);
    connection.currentTabId = tabId;
    connection.currentSpaceId = spaceId;
    return success({{QStringLiteral("tab"), describeTab(tabId, connection)}});
}

// An Agent closes a tab an Agent opened, or any ordinary tab of an Agent
// Space. Every other tab is the reader's.
QJsonObject AgentControl::close(Connection &connection, const QJsonObject &request)
{
    auto tabId = request.value(QStringLiteral("tab")).toString();
    if (tabId.isEmpty()) {
        tabId = connection.currentTabId;
    }
    if (tabId.isEmpty()) {
        return refusal(QStringLiteral("no-current-tab"),
            QStringLiteral("This connection has no current tab. Name one with --tab."));
    }
    const auto tab = m_browser->findTab(tabId);
    if (!tab) {
        return refusal(
            QStringLiteral("not-found"), QStringLiteral("There is no tab \"%1\".").arg(tabId));
    }
    const auto agentsTab
        = m_openedTabIds.contains(tabId) || (m_allowAgents && m_browser->agentSpace(tab->spaceId));
    if (tab->pinned || !agentsTab) {
        return refusal(QStringLiteral("refused"),
            QStringLiteral("An Agent closes only the tabs an Agent opened."));
    }
    if (!m_browser->closeTabInSpace(tabId)) {
        return refusal(QStringLiteral("failed"), QStringLiteral("The tab could not be closed."));
    }
    m_openedTabIds.remove(tabId);
    for (auto &other : m_connections) {
        if (other.currentTabId == tabId) {
            other.currentTabId.clear();
        }
    }
    return success({{QStringLiteral("closed"), tabId}});
}

QJsonObject AgentControl::createSpace(Connection &connection, const QJsonObject &request)
{
    auto name = request.value(QStringLiteral("space")).toString().trimmed();
    if (name.isEmpty()) {
        name = defaultAgentSpaceName;
    }
    const auto spaceId = m_browser->createAgentSpace(name);
    if (spaceId.isEmpty()) {
        return refusal(QStringLiteral("failed"), QStringLiteral("The Space could not be created."));
    }
    connection.currentSpaceId = spaceId;
    connection.currentTabId.clear();
    return success({{QStringLiteral("space"),
        QJsonObject {
            {QStringLiteral("id"), spaceId},
            {QStringLiteral("name"), name},
            {QStringLiteral("onShow"), false},
            {QStringLiteral("agent"), true},
        }}});
}

QJsonObject AgentControl::deleteSpace(Connection &connection, const QJsonObject &request)
{
    const auto named = request.value(QStringLiteral("space")).toString();
    const auto spaceId = findSpace(named);
    if (spaceId.isEmpty()) {
        return refusal(
            QStringLiteral("not-found"), QStringLiteral("There is no Space \"%1\".").arg(named));
    }
    if (!m_browser->agentSpace(spaceId)) {
        return refusal(
            QStringLiteral("refused"), QStringLiteral("An Agent deletes only an Agent Space."));
    }
    if (!m_browser->deleteAgentSpace(spaceId)) {
        return refusal(QStringLiteral("failed"), QStringLiteral("The Space could not be deleted."));
    }
    if (connection.currentSpaceId == spaceId) {
        connection.currentSpaceId.clear();
    }
    return success({{QStringLiteral("deleted"), spaceId}});
}

QString AgentControl::findSpace(const QString &idOrName) const
{
    if (idOrName.isEmpty()) {
        return {};
    }
    const auto *model = m_browser->spaces();
    QString byName;
    int named = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto index = model->index(row, 0);
        const auto id = index.data(SpaceListModel::IdRole).toString();
        if (id == idOrName) {
            return id;
        }
        if (index.data(SpaceListModel::NameRole).toString() == idOrName) {
            byName = id;
            ++named;
        }
    }
    return named == 1 ? byName : QString {};
}

QString AgentControl::defaultSpace(const Connection &connection) const
{
    if (const auto tab = m_browser->findTab(connection.currentTabId)) {
        return tab->spaceId;
    }
    if (!findSpace(connection.currentSpaceId).isEmpty()) {
        return connection.currentSpaceId;
    }
    return m_browser->activeSpaceId();
}

QJsonObject AgentControl::describeTab(const QString &tabId, const Connection &connection) const
{
    const auto tab = m_browser->findTab(tabId);
    if (!tab) {
        return {};
    }
    return {
        {QStringLiteral("id"), tab->id},
        {QStringLiteral("space"), tab->spaceId},
        {QStringLiteral("url"), tab->url.toString()},
        {QStringLiteral("title"), tab->title},
        {QStringLiteral("pinned"), tab->pinned},
        {QStringLiteral("current"), tab->id == connection.currentTabId},
    };
}

} // namespace omaweb
