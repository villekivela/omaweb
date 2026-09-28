#include "AgentControl.h"

#include "BrowserController.h"
#include "PrivacyFile.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QMetaMethod>
#include <QRegularExpression>

#include <algorithm>
#include <utility>

#include <QUuid>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace omaweb {
namespace {

    constexpr QLatin1StringView allowAgentsKey("allow-agents");

    // Where PrivacyFile keeps it. Named here too because the file is watched.
    constexpr auto privacyFileName = "privacy.json";

    const auto defaultAgentSpaceName = QStringLiteral("Agent");

    // A batch is what an Agent means to do before it looks again, which is a
    // form or a few clicks, not a script.
    constexpr qsizetype maximumSteps = 50;
    constexpr int defaultSettleMs = 300;
    constexpr int maximumSettleMs = 10000;
    constexpr int defaultStepTimeoutMs = 10000;
    constexpr int minimumStepTimeoutMs = 100;
    constexpr int maximumStepTimeoutMs = 60000;
    // A page that has not answered by then is not going to, and the Agent is
    // told so rather than left waiting on the socket.
    constexpr int pageAnswerMs = 60000;
    constexpr int fullShotAnswerMs = 120000;
    constexpr int maximumBatchAnswerMs = 15 * 60 * 1000;
    constexpr int idleCheckMs = 30000;
    // Screenshots are the Agent's to read soon after, not an archive.
    constexpr qsizetype maximumShots = 50;
    constexpr qint64 maximumShotAgeSeconds = 24 * 60 * 60;

    const QSet<QString> &stepActions()
    {
        static const QSet<QString> actions {QStringLiteral("click"), QStringLiteral("fill"),
            QStringLiteral("press"), QStringLiteral("select"), QStringLiteral("scroll"),
            QStringLiteral("back"), QStringLiteral("wait")};
        return actions;
    }

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
    m_clock.start();
    m_idleCheck.setInterval(idleCheckMs);
    connect(&m_idleCheck, &QTimer::timeout, this, &AgentControl::detachIdle);
    // Only an explicit `true` lets Agents in.
    m_allowAgents = PrivacyFile::read(m_configRoot, allowAgentsKey).toBool(false);
    if (m_configRoot.isEmpty()) {
        return;
    }
    // The file is watched, so the reader turning Agents off reaches a browser
    // already running. PrivacyFile writes by replacing the file, which a
    // watch on the file alone loses, so the directory is watched as well and
    // the file is watched again whenever it comes back.
    QDir().mkpath(m_configRoot);
    m_watcher.addPath(m_configRoot);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &AgentControl::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &AgentControl::reload);
    reload();
}

bool AgentControl::allowAgents() const { return m_allowAgents; }

void AgentControl::setAllowAgents(bool allowed)
{
    if (allowed == m_allowAgents) {
        return;
    }
    PrivacyFile::write(m_configRoot, allowAgentsKey, allowed);
    apply(allowed);
}

void AgentControl::reload()
{
    const auto path = QDir(m_configRoot).filePath(QLatin1String(privacyFileName));
    if (QFileInfo::exists(path) && !m_watcher.files().contains(path)) {
        m_watcher.addPath(path);
    }
    apply(PrivacyFile::read(m_configRoot, allowAgentsKey).toBool(false));
}

void AgentControl::apply(bool allowed)
{
    if (allowed == m_allowAgents) {
        return;
    }
    m_allowAgents = allowed;
    if (!allowed) {
        for (auto &connection : m_connections) {
            connection.currentTabId.clear();
        }
        if (!m_attached.isEmpty()) {
            m_attached.clear();
            m_idleCheck.stop();
            emit agentTabsChanged();
        }
        // What was already asked of a page is refused rather than answered,
        // and the pages stop, so nothing more is sent to them and nothing
        // they hold comes back.
        const auto pending = std::exchange(m_pendingPages, {});
        if (!pending.isEmpty()) {
            emit pageRequestsCancelled();
        }
        for (const auto &request : pending) {
            request.deadline->stop();
            request.deadline->deleteLater();
            request.reply(refusal(QStringLiteral("allow-agents"),
                QStringLiteral("Allow agents was turned off, so the page was left as it was.")));
        }
    }
    emit allowAgentsChanged();
}

bool AgentControl::gated(const QString &verb)
{
    return verb == u"space new" || verb == u"space delete" || pageVerb(verb);
}

bool AgentControl::pageVerb(const QString &verb)
{
    return verb == u"look" || verb == u"read" || verb == u"do" || verb == u"shot"
        || verb == u"eval";
}

QStringList AgentControl::agentTabIds() const
{
    auto ids = m_attached.keys();
    ids.sort();
    return ids;
}

QVariantMap AgentControl::agentTab(const QString &tabId) const
{
    if (!m_attached.contains(tabId)) {
        return {};
    }
    const auto tab = m_browser->findTab(tabId);
    if (!tab) {
        return {};
    }
    return {
        {QStringLiteral("tabId"), tab->id},
        {QStringLiteral("spaceId"), tab->spaceId},
        {QStringLiteral("url"), tab->url},
        {QStringLiteral("zoom"), tab->zoom},
        {QStringLiteral("muted"), tab->muted},
    };
}

void AgentControl::setShotDirectory(const QString &directory) { m_shotDirectory = directory; }

void AgentControl::setAttachmentIdleMs(int milliseconds)
{
    m_attachmentIdleMs = milliseconds;
    m_idleCheck.setInterval(std::min(idleCheckMs, std::max(milliseconds / 2, 1)));
}

void AgentControl::attach(const QString &tabId)
{
    const auto known = m_attached.contains(tabId);
    m_attached.insert(tabId, m_clock.elapsed());
    if (!m_idleCheck.isActive()) {
        m_idleCheck.start();
    }
    if (!known) {
        emit agentTabsChanged();
    }
}

void AgentControl::detach(const QString &tabId)
{
    if (m_attached.remove(tabId) > 0) {
        if (m_attached.isEmpty()) {
            m_idleCheck.stop();
        }
        emit agentTabsChanged();
    }
}

// A tab the Agent has left alone for a while, or one that has gone, stops
// being an Agent tab. One a page verb is still working in is kept whatever
// its age.
void AgentControl::detachIdle()
{
    const auto now = m_clock.elapsed();
    QSet<QString> working;
    for (const auto &pending : std::as_const(m_pendingPages)) {
        working.insert(pending.tabId);
    }
    QStringList leaving;
    for (auto it = m_attached.cbegin(); it != m_attached.cend(); ++it) {
        if (working.contains(it.key())) {
            continue;
        }
        if (now - it.value() >= m_attachmentIdleMs || !m_browser->findTab(it.key())) {
            leaving.append(it.key());
        }
    }
    for (const auto &tabId : std::as_const(leaving)) {
        m_attached.remove(tabId);
    }
    if (m_attached.isEmpty()) {
        m_idleCheck.stop();
    }
    if (!leaving.isEmpty()) {
        emit agentTabsChanged();
    }
}

QJsonObject AgentControl::gate(const QString &verb) const
{
    static const QSet<QString> verbs {QStringLiteral("spaces"), QStringLiteral("tabs"),
        QStringLiteral("open"), QStringLiteral("close"), QStringLiteral("space new"),
        QStringLiteral("space delete"), QStringLiteral("look"), QStringLiteral("read"),
        QStringLiteral("do"), QStringLiteral("shot"), QStringLiteral("eval")};
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
            pageVerb(verb)
                ? QStringLiteral("Allow agents is off. The reader must turn it on before an Agent "
                                 "can read or drive a page.")
                : QStringLiteral("Allow agents is off. The reader must turn it on before an Agent "
                                 "can use Agent Spaces."));
    }
    return {};
}

// A tab the reader closed since is no longer anyone's current tab, nor an
// Agent tab, and one the reader moved takes the connection's Space with it.
void AgentControl::resolveCurrentTab(Connection &connection) const
{
    if (connection.currentTabId.isEmpty()) {
        return;
    }
    const auto tab = m_browser->findTab(connection.currentTabId, connection.currentSpaceId);
    if (tab) {
        connection.currentSpaceId = tab->spaceId;
    } else {
        connection.currentTabId.clear();
    }
}

void AgentControl::handle(const QJsonObject &request, const Reply &reply, quint64 socketConnection)
{
    const auto verb = request.value(QStringLiteral("verb")).toString();
    if (const auto refused = gate(verb); !refused.isEmpty()) {
        reply(refused);
        return;
    }
    const auto name = request.value(QStringLiteral("name")).toString();
    auto &connection = connectionNamed(name);
    const auto previousTabId = connection.currentTabId;
    resolveCurrentTab(connection);
    if (!previousTabId.isEmpty() && connection.currentTabId.isEmpty()) {
        detach(previousTabId);
    }
    if (pageVerb(verb)) {
        askPage(verb, name, connection, request, reply);
        return;
    }
    reply(answerBrowserCommand(verb, name, connection, request, socketConnection));
}

QJsonObject AgentControl::answer(const QJsonObject &request, quint64 socketConnection)
{
    if (pageVerb(request.value(QStringLiteral("verb")).toString())) {
        return refusal(QStringLiteral("pending"),
            QStringLiteral("A page verb is answered by the page, through the socket."));
    }
    QJsonObject answered;
    handle(
        request, [&answered](const QJsonObject &result) { answered = result; }, socketConnection);
    return answered;
}

QJsonObject AgentControl::answerBrowserCommand(const QString &verb, const QString &name,
    Connection &connection, const QJsonObject &request, quint64 socketConnection)
{
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
        return createSpace(name, connection, request, socketConnection);
    }
    return deleteSpace(name, connection, request);
}

void AgentControl::askPage(const QString &verb, const QString &name, Connection &connection,
    const QJsonObject &request, const Reply &reply)
{
    auto tabId = request.value(QStringLiteral("tab")).toString();
    if (tabId.isEmpty()) {
        tabId = connection.currentTabId;
    }
    if (tabId.isEmpty()) {
        reply(refusal(QStringLiteral("no-current-tab"),
            QStringLiteral(
                "This connection has no current tab. Open one, or name one with --tab.")));
        return;
    }
    const auto tab = m_browser->findTab(
        tabId, tabId == connection.currentTabId ? connection.currentSpaceId : QString {});
    if (!tab) {
        reply(refusal(
            QStringLiteral("not-found"), QStringLiteral("There is no tab \"%1\".").arg(tabId)));
        return;
    }
    if (!mayRead(*tab)) {
        reply(refusal(QStringLiteral("refused"),
            QStringLiteral("An Agent reads and drives only the pages of an Agent Space. Make one "
                           "with `space new` and open the address there.")));
        return;
    }
    QVariantMap arguments;
    if (const auto refused = pageArguments(verb, request, arguments); !refused.isEmpty()) {
        reply(refused);
        return;
    }
    if (!isSignalConnected(QMetaMethod::fromSignal(&AgentControl::pageRequested))) {
        reply(refusal(QStringLiteral("unavailable"),
            QStringLiteral("This browser has no page to answer the verb.")));
        return;
    }

    connection.currentTabId = tabId;
    connection.currentSpaceId = tab->spaceId;
    attach(tabId);

    auto budget = pageAnswerMs;
    if (verb == u"shot" && arguments.value(QStringLiteral("full")).toBool()) {
        budget = fullShotAnswerMs;
    } else if (verb == u"do") {
        const auto steps = arguments.value(QStringLiteral("steps")).toList().size();
        const auto perStep = arguments.value(QStringLiteral("timeout")).toInt()
            + arguments.value(QStringLiteral("settle")).toInt() + 2000;
        budget = static_cast<int>(std::min<qint64>(
            maximumBatchAnswerMs, static_cast<qint64>(steps) * perStep + pageAnswerMs));
    }
    const auto requestId = m_nextPageRequest++;
    auto *deadline = new QTimer(this);
    deadline->setSingleShot(true);
    deadline->setInterval(budget);
    connect(deadline, &QTimer::timeout, this, [this, requestId] {
        const auto pending = m_pendingPages.take(requestId);
        if (!pending.reply) {
            return;
        }
        pending.deadline->deleteLater();
        pending.reply(
            refusal(QStringLiteral("timeout"), QStringLiteral("The page did not answer in time.")));
    });
    m_pendingPages.insert(
        requestId, PendingPage {.reply = reply, .deadline = deadline, .tabId = tabId});
    deadline->start();
    emit pageRequested(requestId,
        QVariantMap {
            {QStringLiteral("verb"), verb},
            {QStringLiteral("tabId"), tabId},
            {QStringLiteral("spaceId"), tab->spaceId},
            {QStringLiteral("url"), tab->url},
            {QStringLiteral("name"), name},
            {QStringLiteral("arguments"), arguments},
        });
}

void AgentControl::answerPage(int requestId, const QVariantMap &answer)
{
    const auto found = m_pendingPages.find(requestId);
    if (found == m_pendingPages.end()) {
        return;
    }
    const auto pending = *found;
    m_pendingPages.erase(found);
    pending.deadline->stop();
    pending.deadline->deleteLater();
    auto result = QJsonObject::fromVariantMap(answer);
    if (!result.value(QStringLiteral("ok")).isBool()) {
        result = refusal(QStringLiteral("failed"), QStringLiteral("The page gave no answer."));
    }
    result.insert(QStringLiteral("tab"), pending.tabId);
    if (m_attached.contains(pending.tabId)) {
        m_attached.insert(pending.tabId, m_clock.elapsed());
    }
    pending.reply(result);
}

// A screenshot is written only in the shots directory, which only the reader
// can enter, under a name that is new there: a page verb never becomes a way
// to write or replace any other file of the reader's. The file is made here,
// closed to everyone else, and the page's picture is written into it. Old
// screenshots go, so the directory does not grow without end on a runtime
// directory that lives in memory.
QString AgentControl::reserveShot(const QString &name, QJsonObject &refused) const
{
    const auto failed = [&refused](const QString &error) {
        refused = refusal(QStringLiteral("failed"), error);
        return QString();
    };
    if (!name.isEmpty()
        && (name.contains(u'/') || name.contains(u'\\') || name == u"." || name == u".."
            || name.startsWith(u'.'))) {
        refused = refusal(QStringLiteral("bad-request"),
            QStringLiteral("Name the screenshot's file alone, such as page.png. Omaweb writes it "
                           "in its own directory for screenshots."));
        return {};
    }
    if (m_shotDirectory.isEmpty() || !QDir().mkpath(m_shotDirectory)
        || ::chmod(QFile::encodeName(m_shotDirectory).constData(), S_IRWXU) != 0) {
        return failed(QStringLiteral("There is nowhere to write the screenshot."));
    }
    pruneShots();
    const QDir directory(m_shotDirectory);
    for (int attempt = 0; attempt < 16; ++attempt) {
        const auto file = !name.isEmpty()
            ? name
            : QStringLiteral("shot-%1-%2.png")
                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")),
                      QUuid::createUuid().toString(QUuid::Id128).left(8));
        const auto path = directory.filePath(file);
        const auto made = ::open(QFile::encodeName(path).constData(),
            O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, S_IRUSR | S_IWUSR);
        if (made >= 0) {
            ::close(made);
            return path;
        }
        if (errno == EEXIST && !name.isEmpty()) {
            refused = refusal(QStringLiteral("bad-request"),
                QStringLiteral("There is a screenshot called %1 already. Name another.").arg(name));
            return {};
        }
        if (errno != EEXIST) {
            return failed(QStringLiteral("The screenshot could not be made."));
        }
    }
    return failed(QStringLiteral("The screenshot could not be made."));
}

void AgentControl::pruneShots() const
{
    auto shots
        = QDir(m_shotDirectory)
              .entryInfoList({QStringLiteral("*.png")}, QDir::Files | QDir::NoSymLinks, QDir::Time);
    const auto oldest = QDateTime::currentDateTime().addSecs(-maximumShotAgeSeconds);
    for (qsizetype index = 0; index < shots.size(); ++index) {
        // Newest first, and one place kept for the screenshot about to be made.
        if (index >= maximumShots - 1 || shots.at(index).lastModified() < oldest) {
            QFile::remove(shots.at(index).filePath());
        }
    }
}

QJsonObject AgentControl::pageArguments(
    const QString &verb, const QJsonObject &request, QVariantMap &out) const
{
    const auto bad
        = [](const QString &error) { return refusal(QStringLiteral("bad-request"), error); };
    if (verb == u"look") {
        out.insert(QStringLiteral("all"), request.value(QStringLiteral("all")).toBool());
        return {};
    }
    if (verb == u"read") {
        out.insert(
            QStringLiteral("selector"), request.value(QStringLiteral("selector")).toString());
        return {};
    }
    if (verb == u"eval") {
        const auto expression = request.value(QStringLiteral("expression")).toString();
        if (expression.trimmed().isEmpty()) {
            return bad(QStringLiteral("Give an expression to evaluate."));
        }
        out.insert(QStringLiteral("expression"), expression);
        return {};
    }
    if (verb == u"shot") {
        out.insert(QStringLiteral("full"), request.value(QStringLiteral("full")).toBool());
        QJsonObject refused;
        const auto destination
            = reserveShot(request.value(QStringLiteral("output")).toString(), refused);
        if (destination.isEmpty()) {
            return refused;
        }
        out.insert(QStringLiteral("destination"), destination);
        return {};
    }

    // `do`
    const auto steps = request.value(QStringLiteral("steps")).toArray();
    if (steps.isEmpty()) {
        return bad(QStringLiteral("Give at least one step to do."));
    }
    if (steps.size() > maximumSteps) {
        return bad(QStringLiteral("A batch holds at most %1 steps.").arg(maximumSteps));
    }
    QVariantList checked;
    for (qsizetype index = 0; index < steps.size(); ++index) {
        const auto step = steps.at(index).toObject();
        const auto action = step.value(QStringLiteral("action")).toString();
        const auto at = [&](const QString &error) {
            return bad(QStringLiteral("Step %1: %2").arg(index + 1).arg(error));
        };
        if (!stepActions().contains(action)) {
            return at(QStringLiteral("there is no step \"%1\".").arg(action));
        }
        const auto target = step.value(QStringLiteral("target")).toString();
        const auto text = step.value(QStringLiteral("text"));
        const auto needsTarget
            = action == u"click" || action == u"fill" || action == u"select" || action == u"scroll";
        if (needsTarget && target.isEmpty()) {
            return at(QStringLiteral("`%1` needs a label.").arg(action));
        }
        if (action == u"fill" && !text.isString()) {
            return at(QStringLiteral("`fill` needs the text to type."));
        }
        if (action == u"select" && text.toString().isEmpty()) {
            return at(QStringLiteral("`select` needs the option to choose."));
        }
        if (action == u"press" && step.value(QStringLiteral("key")).toString().isEmpty()) {
            return at(QStringLiteral("`press` needs a key."));
        }
        if (action == u"wait") {
            const auto forText = !step.value(QStringLiteral("text")).toString().isEmpty();
            const auto forUrl = !step.value(QStringLiteral("url")).toString().isEmpty();
            if (forText == forUrl) {
                return at(QStringLiteral("`wait` waits for text or for an address."));
            }
        }
        checked.append(step.toVariantMap());
    }
    const auto number = [&](const QString &key, int fallback, int lowest, int highest, int &value) {
        const auto given = request.value(key);
        if (given.isUndefined() || given.isNull()) {
            value = fallback;
            return true;
        }
        if (!given.isDouble()) {
            return false;
        }
        value = static_cast<int>(given.toDouble());
        return value >= lowest && value <= highest;
    };
    int settle = 0;
    int timeout = 0;
    if (!number(QStringLiteral("settle"), defaultSettleMs, 0, maximumSettleMs, settle)) {
        return bad(QStringLiteral("--settle is milliseconds from 0 to %1.").arg(maximumSettleMs));
    }
    if (!number(QStringLiteral("timeout"), defaultStepTimeoutMs, minimumStepTimeoutMs,
            maximumStepTimeoutMs, timeout)) {
        return bad(QStringLiteral("--timeout is milliseconds from %1 to %2.")
                .arg(minimumStepTimeoutMs)
                .arg(maximumStepTimeoutMs));
    }
    out.insert(QStringLiteral("steps"), checked);
    out.insert(QStringLiteral("settle"), settle);
    out.insert(QStringLiteral("timeout"), timeout);
    return {};
}

AgentControl::Connection &AgentControl::connectionNamed(const QString &name)
{
    auto found = m_connections.find(name);
    if (found == m_connections.end()) {
        // A name costs nothing to invent, so the states they pick are
        // bounded, and the one used longest ago makes room.
        if (m_connections.size() >= maximumConnections) {
            auto oldest = m_connections.begin();
            for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
                if (it->lastUsed < oldest->lastUsed) {
                    oldest = it;
                }
            }
            m_connections.erase(oldest);
        }
        found = m_connections.insert(name, {});
    }
    found->lastUsed = ++m_requests;
    return *found;
}

// Until Space grants land, the only tabs an Agent may drive are the ones an
// Agent opened this run and those of an Agent Space.
bool AgentControl::mayDrive(const TabState &tab) const
{
    if (tab.pinned) {
        return false;
    }
    return m_openedTabIds.contains(tab.id) || mayRead(tab);
}

// A page verb reads the page and acts in it as the Space's own identity, with
// its cookies and its logins. Until Space grants land, that is only an Agent
// Space's, whoever opened the tab: a tab an Agent opened in one of the
// reader's Spaces is the reader's page.
bool AgentControl::mayRead(const TabState &tab) const
{
    return !tab.pinned && m_allowAgents && m_browser->agentSpace(tab.spaceId);
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
        return noSpace(named);
    }
    QJsonArray tabs;
    for (const auto &tab : m_browser->spaceTabs(spaceId)) {
        tabs.append(describeTab(tab, connection));
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
        auto tab = m_browser->findTab(tabId, connection.currentSpaceId);
        if (!tab) {
            return refusal(
                QStringLiteral("not-found"), QStringLiteral("There is no tab \"%1\".").arg(tabId));
        }
        if (tab->pinned) {
            return refusal(QStringLiteral("refused"),
                QStringLiteral("A Pinned tab's address is the reader's to change."));
        }
        if (!mayDrive(*tab)) {
            return refusal(QStringLiteral("refused"),
                QStringLiteral("An Agent loads an address only in a tab an Agent opened or one of "
                               "an Agent Space. Open a new tab instead."));
        }
        if (!m_browser->navigateTab(tabId, url, tab->spaceId)) {
            return refusal(
                QStringLiteral("failed"), QStringLiteral("The tab could not be loaded."));
        }
        connection.currentTabId = tabId;
        connection.currentSpaceId = tab->spaceId;
        // The page an Agent loads is one it means to read, so it starts
        // loading behind the page on show rather than at the first look.
        if (mayRead(*tab)) {
            attach(tabId);
        }
        tab->url = url;
        tab->title = url.host().isEmpty() ? url.toDisplayString() : url.host();
        return success({{QStringLiteral("tab"), describeTab(*tab, connection)}});
    }

    const auto spaceId = spaceName.isEmpty() ? defaultSpace(connection) : findSpace(spaceName);
    if (spaceId.isEmpty()) {
        return noSpace(spaceName);
    }
    const auto tabId = m_browser->openTabInSpace(spaceId, url);
    if (tabId.isEmpty()) {
        return refusal(QStringLiteral("failed"), QStringLiteral("The tab could not be opened."));
    }
    m_openedTabIds.insert(tabId);
    connection.currentTabId = tabId;
    connection.currentSpaceId = spaceId;
    if (m_allowAgents && m_browser->agentSpace(spaceId)) {
        attach(tabId);
    }
    const auto tab = m_browser->findTab(tabId, spaceId);
    return success({{QStringLiteral("tab"), tab ? describeTab(*tab, connection) : QJsonObject {}}});
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
    const auto tab = m_browser->findTab(tabId, connection.currentSpaceId);
    if (!tab) {
        return refusal(
            QStringLiteral("not-found"), QStringLiteral("There is no tab \"%1\".").arg(tabId));
    }
    if (!mayDrive(*tab)) {
        return refusal(QStringLiteral("refused"),
            QStringLiteral("An Agent closes only the tabs an Agent opened."));
    }
    if (!m_browser->closeTabInSpace(tabId, tab->spaceId)) {
        return refusal(QStringLiteral("failed"), QStringLiteral("The tab could not be closed."));
    }
    m_openedTabIds.remove(tabId);
    detach(tabId);
    for (auto &other : m_connections) {
        if (other.currentTabId == tabId) {
            other.currentTabId.clear();
        }
    }
    return success({{QStringLiteral("closed"), tabId}});
}

QJsonObject AgentControl::createSpace(const QString &creator, Connection &connection,
    const QJsonObject &request, quint64 socketConnection)
{
    auto name = request.value(QStringLiteral("space")).toString().trimmed();
    if (name.isEmpty()) {
        name = defaultAgentSpaceName;
    }
    const auto temporary = request.value(QStringLiteral("temporary")).toBool();
    if (temporary && socketConnection == 0) {
        return refusal(QStringLiteral("bad-request"),
            QStringLiteral("A temporary Space lasts as long as its connection, and this request "
                           "came over none."));
    }
    const auto spaceId = m_browser->createAgentSpace(name, creator, temporary);
    if (spaceId.isEmpty()) {
        return refusal(QStringLiteral("failed"), QStringLiteral("The Space could not be created."));
    }
    if (temporary) {
        m_temporarySpaces[socketConnection].append(spaceId);
    }
    connection.currentSpaceId = spaceId;
    connection.currentTabId.clear();
    return success({{QStringLiteral("space"),
        QJsonObject {
            {QStringLiteral("id"), spaceId},
            {QStringLiteral("name"), name},
            {QStringLiteral("onShow"), false},
            {QStringLiteral("agent"), true},
            {QStringLiteral("temporary"), temporary},
        }}});
}

void AgentControl::connectionClosed(quint64 connection)
{
    for (const auto &spaceId : m_temporarySpaces.take(connection)) {
        m_browser->deleteTemporarySpace(spaceId);
    }
}

// The name is not an identity, so this keeps one Agent from sweeping away
// another's work by mistake rather than keeping out anyone who means to.
QJsonObject AgentControl::deleteSpace(
    const QString &requester, Connection &connection, const QJsonObject &request)
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
    if (m_browser->agentSpaceCreator(spaceId) != requester) {
        return refusal(QStringLiteral("refused"),
            QStringLiteral("An Agent deletes only an Agent Space it created."));
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
    if (connection.currentSpaceId.isEmpty()) {
        return m_browser->activeSpaceId();
    }
    // The connection's Space went, a temporary one with its connection or any
    // Space the reader deleted. Its next page is not the reader's to receive.
    return findSpace(connection.currentSpaceId);
}

QJsonObject AgentControl::noSpace(const QString &named)
{
    return refusal(QStringLiteral("not-found"),
        named.isEmpty()
            ? QStringLiteral("The Space this connection was using is gone. Name one with --space.")
            : QStringLiteral("There is no Space \"%1\".").arg(named));
}

QJsonObject AgentControl::describeTab(const TabState &tab, const Connection &connection) const
{
    return {
        {QStringLiteral("id"), tab.id},
        {QStringLiteral("space"), tab.spaceId},
        {QStringLiteral("url"), tab.url.toString()},
        {QStringLiteral("title"), tab.title},
        {QStringLiteral("pinned"), tab.pinned},
        {QStringLiteral("current"), tab.id == connection.currentTabId},
    };
}

} // namespace omaweb
