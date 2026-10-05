#include "AgentControl.h"

#include "AgentActivityLog.h"
#include "AgentProtocol.h"
#include "BrowserController.h"
#include "PrivacyFile.h"
#include "SpaceProject.h"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QMetaMethod>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>
#include <memory>
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

    constexpr QLatin1StringView agentCommandKey("agent-command");
    const auto defaultAgentCommand = QStringLiteral("claude");

    // What the reader's agent is first told: the tab is the reader's own, so
    // it is used as it is rather than the Agent Space the skill would make,
    // and then the reader's words exactly as they were typed.
    QString agentPrompt(const QString &tabId, const QString &words)
    {
        auto prompt = QStringLiteral(
            "Use the omaweb skill to work on the reader's Omaweb tab %1 with the omaweb CLI. "
            "Pass --tab %1 to every page command, and make no Agent Space for it.")
                          .arg(tabId);
        if (!words.isEmpty()) {
            prompt += QStringLiteral("\n\n") + words;
        }
        return prompt;
    }

    // An agent command with nothing in it is the default.
    QString agentCommandOrDefault(const QString &command)
    {
        const auto trimmed = command.trimmed();
        return trimmed.isEmpty() ? defaultAgentCommand : trimmed;
    }

    QString storedAgentCommand(const QString &configRoot)
    {
        return agentCommandOrDefault(PrivacyFile::read(configRoot, agentCommandKey).toString());
    }

    // A program named by its path, if it can be run, or the first one of the
    // name on the search path. Empty when there is none.
    QString runnableProgram(const QString &program)
    {
        if (!program.contains(u'/')) {
            return QStandardPaths::findExecutable(program);
        }
        const QFileInfo file(program);
        return file.isFile() && file.isExecutable() ? file.absoluteFilePath() : QString();
    }

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
    // A form takes a few files, not a directory's worth.
    constexpr qsizetype maximumUploadFiles = 16;
    constexpr qsizetype maximumDirectoryNameLength = 64;

    // A connection's name as a directory name: nothing that leaves the
    // directory it is put in, and nothing hidden.
    QString directoryName(const QString &name)
    {
        QString cleaned;
        for (const auto character : name) {
            const auto safe = character.isLetterOrNumber() || character == u'-' || character == u'_'
                || character == u'.' || character == u' ';
            cleaned.append(safe ? character : u'-');
        }
        while (cleaned.startsWith(u'.') || cleaned.startsWith(u' ')) {
            cleaned.remove(0, 1);
        }
        cleaned = cleaned.left(maximumDirectoryNameLength).trimmed();
        return cleaned.isEmpty() ? QStringLiteral("agent") : cleaned;
    }

    bool isWindowId(const QString &id) { return id.startsWith(u"window-"); }

    // A target is an address or a name, and a log line has no use for more.
    constexpr qsizetype maximumAddressLength = 2048;
    constexpr qsizetype maximumTargetLength = 200;

    const QSet<QString> &stepActions()
    {
        static const QSet<QString> actions {QStringLiteral("click"), QStringLiteral("fill"),
            QStringLiteral("press"), QStringLiteral("select"), QStringLiteral("scroll"),
            QStringLiteral("back"), QStringLiteral("wait"), QStringLiteral("dialog"),
            QStringLiteral("upload")};
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

    QString quoted(const QString &text) { return QStringLiteral("\"%1\"").arg(text); }

    // What an address keeps of itself as an origin: its scheme, host and port.
    constexpr auto appOrigin
        = QUrl::RemoveUserInfo | QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment;

    // One step of a `do`, as the reader is told it was done: what the page
    // named the element it reached, and the label only where it named none.
    QString describeStep(const QVariantMap &step, const QVariantMap &result)
    {
        const auto action = step.value(QStringLiteral("action")).toString();
        const auto target = step.value(QStringLiteral("target")).toString();
        const auto text = step.value(QStringLiteral("text")).toString();
        auto name = result.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("label %1").arg(target);
        } else {
            name = quoted(name);
        }
        if (action == u"click") {
            return QStringLiteral("clicked %1").arg(name);
        }
        if (action == u"fill") {
            return QStringLiteral("typed in %1").arg(name);
        }
        if (action == u"select") {
            return QStringLiteral("chose %1 in %2").arg(quoted(text), name);
        }
        if (action == u"scroll") {
            if (target == u"up" || target == u"down") {
                return QStringLiteral("scrolled %1").arg(target);
            }
            if (target == u"top" || target == u"bottom") {
                return QStringLiteral("scrolled to the %1").arg(target);
            }
            return QStringLiteral("scrolled to %1").arg(name);
        }
        if (action == u"press") {
            return QStringLiteral("pressed %1").arg(step.value(QStringLiteral("key")).toString());
        }
        if (action == u"back") {
            return QStringLiteral("went back");
        }
        if (action == u"wait") {
            return text.isEmpty() ? QStringLiteral("waited for the address")
                                  : QStringLiteral("waited for %1").arg(quoted(text));
        }
        return {};
    }

    // A page verb's answer as the Agent's last act, or nothing when the verb
    // did nothing to tell: it failed, or a batch failed at its first step.
    QString describeAct(const QString &verb, const QVariantList &steps, const QJsonObject &answer)
    {
        if (verb == u"do") {
            const auto results = answer.value(QStringLiteral("steps")).toArray();
            for (auto index = std::min(results.size(), steps.size()) - 1; index >= 0; --index) {
                const auto result = results.at(index).toObject().toVariantMap();
                if (result.value(QStringLiteral("ok")).toBool()) {
                    return describeStep(steps.at(index).toMap(), result);
                }
            }
            return {};
        }
        if (!answer.value(QStringLiteral("ok")).toBool()) {
            return {};
        }
        if (verb == u"look") {
            return QStringLiteral("looked at the page");
        }
        if (verb == u"read") {
            return QStringLiteral("read the page");
        }
        if (verb == u"eval") {
            return QStringLiteral("ran a script");
        }
        if (verb == u"shot") {
            return QStringLiteral("took a screenshot");
        }
        return {};
    }

    // What a tab an Agent loads is called until its page names itself.
    QString hostOrAddress(const QUrl &url)
    {
        return url.host().isEmpty() ? url.toDisplayString() : url.host();
    }

    QString describeOpened(const QUrl &url)
    {
        return QStringLiteral("opened %1").arg(hostOrAddress(url));
    }

    // An address as the activity log keeps it: without its query, fragment or
    // credentials, which is where a form's values and a site's tokens go.
    QString activityAddress(const QUrl &url)
    {
        return url.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment)
            .toString()
            .left(maximumAddressLength);
    }

    // Whether a verb is about one tab's page: a page verb, or `console`, which
    // the core answers from what that page has logged.
    bool readsTab(const QString &verb)
    {
        return AgentControl::pageVerb(verb) || verb == u"console";
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
    if (m_browser) {
        connect(m_browser, &BrowserController::spaceGrantsChanged, this,
            &AgentControl::grantedSpacesChanged);
        // A Space the reader renames is listed and asked about by its new
        // name, and one they delete is no longer asked about.
        const auto *spaces = m_browser->spaces();
        for (const auto renamed :
            {&AgentControl::grantedSpacesChanged, &AgentControl::grantRequestChanged}) {
            connect(spaces, &QAbstractItemModel::dataChanged, this, renamed);
            connect(spaces, &QAbstractItemModel::modelReset, this, renamed);
        }
        connect(spaces, &QAbstractItemModel::rowsRemoved, this, &AgentControl::dropGoneGrants);
        connect(spaces, &QAbstractItemModel::modelReset, this, &AgentControl::dropGoneGrants);
        // An Agent tab is in use wherever its Space is, so the browser never
        // puts one away as unused while an Agent is attached.
        connect(this, &AgentControl::agentTabsChanged, m_browser,
            [this] { m_browser->setAgentTabIds(agentTabIds()); });
    }
    // Only an explicit `true` lets Agents in.
    m_allowAgents = PrivacyFile::read(m_configRoot, allowAgentsKey).toBool(false);
    m_agentCommand = storedAgentCommand(m_configRoot);
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

QString AgentControl::agentCommand() const { return m_agentCommand; }

void AgentControl::setAgentCommand(const QString &command)
{
    const auto chosen = agentCommandOrDefault(command);
    if (chosen == m_agentCommand) {
        return;
    }
    // Only a command of the reader's own is written down, so the default can
    // change in a later release for a reader who never chose one.
    PrivacyFile::write(m_configRoot, agentCommandKey,
        chosen == defaultAgentCommand ? QJsonValue() : QJsonValue(chosen));
    m_agentCommand = chosen;
    emit agentCommandChanged();
}

QVariantMap AgentControl::askAgent(const QString &tabId, const QString &words)
{
    const auto failed = [](const QString &code, const QString &program = {}) {
        return QVariantMap {{QStringLiteral("ok"), false}, {QStringLiteral("code"), code},
            {QStringLiteral("program"), program}};
    };
    if (!m_allowAgents) {
        return failed(QStringLiteral("allow-agents"));
    }
    if (!m_browser || m_browser->privateBrowsing()) {
        return failed(QStringLiteral("private"));
    }
    const auto tab = m_browser->findTab(tabId);
    if (!tab) {
        return failed(QStringLiteral("no-tab"));
    }
    // A project's Space runs the project's own agent command, or the global
    // one, with `{dir}` naming the project directory. Anywhere else the
    // command is the reader's as they wrote it.
    const auto project = m_browser->spaceProject(tab->spaceId);
    const auto command
        = project && !project->agentCommand.isEmpty() ? project->agentCommand : m_agentCommand;
    auto arguments = project ? projectAgentArguments(command, project->directory)
                             : QProcess::splitCommand(command);
    if (arguments.isEmpty() || runnableProgram(arguments.constFirst()).isEmpty()) {
        return failed(
            QStringLiteral("no-agent"), arguments.isEmpty() ? command : arguments.constFirst());
    }
    const auto terminal = runnableProgram(m_terminalProgram);
    if (terminal.isEmpty()) {
        return failed(QStringLiteral("no-terminal"), m_terminalProgram);
    }
    arguments.append(agentPrompt(tabId, words));
    // The agent works for the reader, so it starts where their terminal
    // would, not wherever the browser was started from: in the project
    // directory, which is where an agent finds the project's CLAUDE.md, or
    // else at home. A folder recorded inside a container may not be here.
    auto directory = QDir::homePath();
    if (project && QFileInfo(project->directory).isDir()) {
        directory = project->directory;
        arguments.prepend(QStringLiteral("--dir=") + directory);
    }
    if (!QProcess::startDetached(terminal, arguments, directory)) {
        return failed(QStringLiteral("not-started"), m_terminalProgram);
    }
    return {{QStringLiteral("ok"), true}};
}

void AgentControl::setTerminalProgram(const QString &program) { m_terminalProgram = program; }

void AgentControl::reload()
{
    const auto path = QDir(m_configRoot).filePath(QLatin1String(privacyFileName));
    if (QFileInfo::exists(path) && !m_watcher.files().contains(path)) {
        m_watcher.addPath(path);
    }
    apply(PrivacyFile::read(m_configRoot, allowAgentsKey).toBool(false));
    const auto command = storedAgentCommand(m_configRoot);
    if (command != m_agentCommand) {
        m_agentCommand = command;
        emit agentCommandChanged();
    }
}

void AgentControl::apply(bool allowed)
{
    if (allowed == m_allowAgents) {
        return;
    }
    m_allowAgents = allowed;
    if (!allowed) {
        while (!m_pendingGrants.isEmpty()) {
            finishGrant(m_pendingGrants.constFirst().spaceId, GrantAnswer::Withdrawn);
        }
        for (auto &connection : m_connections) {
            connection.currentTabId.clear();
        }
        m_console.forgetAll();
        m_tabConnections.clear();
        m_newWindows.clear();
        if (!m_windows.isEmpty()) {
            m_windows.clear();
            emit agentWindowsChanged();
        }
        if (!m_attached.isEmpty()) {
            m_attached.clear();
            m_idleCheck.stop();
            emit agentTabsChanged();
            emit agentActivityChanged();
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
            removeUntakenShot(request);
            request.reply(refusal(QStringLiteral("allow-agents"),
                QStringLiteral("Allow agents was turned off, so the page was left as it was.")));
        }
    }
    emit allowAgentsChanged();
}

bool AgentControl::gated(const QString &verb)
{
    return verb == u"space new" || verb == u"space delete" || readsTab(verb);
}

bool AgentControl::pageVerb(const QString &verb)
{
    return verb == u"look" || verb == u"read" || verb == u"do" || verb == u"shot"
        || verb == u"eval";
}

// In the order `BrowserCommands.qml` describes them, which is the order
// `commands` lists them in. A command added there is not public until it is
// added here, so one that reads a page is never let out by default.
const QStringList &AgentControl::publicCommands()
{
    static const QStringList commands {
        QStringLiteral("back"),
        QStringLiteral("forward"),
        QStringLiteral("reload"),
        QStringLiteral("reload-bypassing-cache"),
        QStringLiteral("stop-loading"),
        QStringLiteral("open-address"),
        QStringLiteral("command-scope"),
        QStringLiteral("new-tab"),
        QStringLiteral("close-tab"),
        QStringLiteral("reopen-tab"),
        QStringLiteral("next-tab"),
        QStringLiteral("previous-tab"),
        QStringLiteral("jump-back"),
        QStringLiteral("jump-forward"),
        QStringLiteral("select-tab"),
        QStringLiteral("pin-tab"),
        QStringLiteral("keep-tab-active"),
        QStringLiteral("extension-popup"),
        QStringLiteral("glance-to-tab"),
        QStringLiteral("duplicate-tab"),
        QStringLiteral("move-tab-up"),
        QStringLiteral("move-tab-down"),
        QStringLiteral("close-other-tabs"),
        QStringLiteral("close-tabs-below"),
        QStringLiteral("tab-menu"),
        QStringLiteral("move-tab"),
        QStringLiteral("add-split"),
        QStringLiteral("separate-split"),
        QStringLiteral("focus-split-partner"),
        QStringLiteral("next-space"),
        QStringLiteral("select-space"),
        QStringLiteral("new-space"),
        QStringLiteral("toggle-sidebar"),
        QStringLiteral("widen-sidebar"),
        QStringLiteral("narrow-sidebar"),
        QStringLiteral("reset-sidebar"),
        QStringLiteral("focus-sidebar"),
        QStringLiteral("focus-page"),
        QStringLiteral("move-focus-left"),
        QStringLiteral("move-focus-down"),
        QStringLiteral("move-focus-up"),
        QStringLiteral("move-focus-right"),
        QStringLiteral("copy-address"),
        QStringLiteral("site-information"),
        QStringLiteral("find"),
        QStringLiteral("find-next"),
        QStringLiteral("find-previous"),
        QStringLiteral("zoom-in"),
        QStringLiteral("zoom-out"),
        QStringLiteral("zoom-reset"),
        QStringLiteral("print"),
        QStringLiteral("fullscreen"),
        QStringLiteral("developer-tools"),
        QStringLiteral("inspect-element"),
        QStringLiteral("open-page-context-menu"),
        QStringLiteral("open-file"),
        QStringLiteral("shortcuts"),
        QStringLiteral("history"),
        QStringLiteral("settings"),
        QStringLiteral("downloads"),
        QStringLiteral("minimize-window"),
    };
    return commands;
}

bool AgentControl::commandTakesPosition(const QString &command)
{
    return command == u"select-tab" || command == u"select-space";
}

QVariantMap AgentControl::grantRequest() const
{
    if (m_pendingGrants.isEmpty()) {
        return {};
    }
    const auto &pending = m_pendingGrants.constFirst();
    return {
        {QStringLiteral("spaceId"), pending.spaceId},
        {QStringLiteral("spaceName"), spaceName(pending.spaceId)},
        {QStringLiteral("name"), pending.name},
    };
}

void AgentControl::answerGrant(const QString &spaceId, bool allowed)
{
    const auto asked = std::any_of(m_pendingGrants.cbegin(), m_pendingGrants.cend(),
        [&spaceId](const PendingGrant &pending) { return pending.spaceId == spaceId; });
    if (!asked) {
        return;
    }
    if (!allowed) {
        finishGrant(spaceId, GrantAnswer::Denied);
        return;
    }
    finishGrant(spaceId,
        m_allowAgents && m_browser->grantSpace(spaceId) ? GrantAnswer::Allowed
                                                        : GrantAnswer::Failed);
}

QVariantList AgentControl::grantedSpaces() const
{
    QVariantList spaces;
    if (!m_browser) {
        return spaces;
    }
    for (const auto &spaceId : m_browser->grantedSpaceIds()) {
        spaces.append(QVariantMap {
            {QStringLiteral("spaceId"), spaceId},
            {QStringLiteral("spaceName"), spaceName(spaceId)},
        });
    }
    return spaces;
}

// The Space's pages are the reader's again from this moment, so nothing an
// Agent started there is let finish, and no Agent tab of it stays rendered.
bool AgentControl::revokeGrant(const QString &spaceId)
{
    if (!m_browser || !m_browser->revokeSpaceGrant(spaceId)) {
        return false;
    }
    const auto name = spaceName(spaceId);
    QSet<QString> inSpace;
    for (const auto &tab : m_browser->spaceTabs(spaceId)) {
        inSpace.insert(tab.id);
    }
    const auto reached = [this, &inSpace](const QString &target) {
        return !target.isEmpty() && inSpace.contains(openerOf(target));
    };

    QSet<QString> users;
    for (auto it = m_tabConnections.cbegin(); it != m_tabConnections.cend(); ++it) {
        if (inSpace.contains(it.key())) {
            users.insert(it.value());
        }
    }
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (users.contains(it.key()) || reached(it->currentTabId)) {
            it->currentTabId.clear();
            it->revokedSpaceName = name;
        }
    }

    QStringList cancelled;
    QList<Reply> refused;
    for (auto it = m_pendingPages.begin(); it != m_pendingPages.end();) {
        if (!reached(it->tabId)) {
            ++it;
            continue;
        }
        it->deadline->stop();
        it->deadline->deleteLater();
        removeUntakenShot(*it);
        cancelled.append(it->tabId);
        refused.append(it->reply);
        it = m_pendingPages.erase(it);
    }

    QStringList leaving;
    for (auto it = m_attached.cbegin(); it != m_attached.cend(); ++it) {
        if (inSpace.contains(it.key())) {
            leaving.append(it.key());
        }
    }
    for (const auto &tabId : std::as_const(leaving)) {
        detach(tabId);
    }
    if (!cancelled.isEmpty()) {
        emit pageRequestsCancelledIn(cancelled);
    }
    for (const auto &reply : std::as_const(refused)) {
        reply(refusal(QStringLiteral("revoked"),
            QStringLiteral("The reader revoked the grant to Space \"%1\", so the page was left "
                           "as it was.")
                .arg(name)));
    }
    return true;
}

void AgentControl::setGrantAnswerMs(int milliseconds) { m_grantAnswerMs = milliseconds; }

QString AgentControl::spaceNeedingGrant(
    const QString &verb, const Connection &connection, const QJsonObject &request) const
{
    if (!m_allowAgents) {
        return {};
    }
    QString tabId;
    if (verb == u"open") {
        // A tab an Agent opened is its to load whatever the Space.
        tabId = tabToLoad(connection, request);
        if (m_openedTabIds.contains(tabId)) {
            return {};
        }
    } else if (pageVerb(verb) || verb == u"console") {
        tabId = openerOf(targetId(connection, request));
    }
    if (tabId.isEmpty()) {
        return {};
    }
    const auto tab = m_browser->findTab(
        tabId, tabId == connection.currentTabId ? connection.currentSpaceId : QString {});
    // A Pinned tab's address is refused whatever the reader would answer.
    if (!tab || usableSpace(tab->spaceId) || (verb == u"open" && tab->pinned)) {
        return {};
    }
    return tab->spaceId;
}

QString AgentControl::tabToLoad(const Connection &connection, const QJsonObject &request)
{
    const auto tabName = request.value(QStringLiteral("tab")).toString();
    // An Auxiliary window is the page's to navigate, not the Agent's.
    const auto newTab = !request.value(QStringLiteral("space")).toString().isEmpty()
        || request.value(QStringLiteral("new")).toBool()
        || (tabName.isEmpty()
            && (connection.currentTabId.isEmpty() || isWindowId(connection.currentTabId)));
    if (newTab) {
        return {};
    }
    return tabName.isEmpty() ? connection.currentTabId : tabName;
}

void AgentControl::dropGoneGrants()
{
    QStringList gone;
    for (const auto &pending : std::as_const(m_pendingGrants)) {
        if (findSpace(pending.spaceId) != pending.spaceId) {
            gone.append(pending.spaceId);
        }
    }
    for (const auto &spaceId : std::as_const(gone)) {
        finishGrant(spaceId, GrantAnswer::Gone);
    }
}

// Everyone who asks for a Space while it is being asked for waits on the one
// prompt, which names the connection that asked first. Each Space's wait is
// its own and starts when it is first asked for, so one waiting behind
// another's prompt is not kept longer than any other.
void AgentControl::askGrant(
    const QString &spaceId, const QString &name, const std::function<void(GrantAnswer)> &waiter)
{
    for (auto &pending : m_pendingGrants) {
        if (pending.spaceId == spaceId) {
            pending.waiters.append(waiter);
            return;
        }
    }
    auto *deadline = new QTimer(this);
    deadline->setSingleShot(true);
    deadline->setInterval(m_grantAnswerMs);
    connect(deadline, &QTimer::timeout, this,
        [this, spaceId] { finishGrant(spaceId, GrantAnswer::Undecided); });
    m_pendingGrants.append(
        PendingGrant {.spaceId = spaceId, .name = name, .deadline = deadline, .waiters = {waiter}});
    deadline->start();
    if (m_pendingGrants.size() == 1) {
        emit grantRequestChanged();
    }
}

void AgentControl::finishGrant(const QString &spaceId, GrantAnswer answer)
{
    const auto found = std::find_if(m_pendingGrants.begin(), m_pendingGrants.end(),
        [&spaceId](const PendingGrant &pending) { return pending.spaceId == spaceId; });
    if (found == m_pendingGrants.end()) {
        return;
    }
    const auto shown = found == m_pendingGrants.begin();
    const auto pending = *found;
    m_pendingGrants.erase(found);
    pending.deadline->stop();
    pending.deadline->deleteLater();
    // The prompt changes before anyone is answered, since an answer can ask
    // for another Space.
    if (shown) {
        emit grantRequestChanged();
    }
    for (const auto &waiter : pending.waiters) {
        waiter(answer);
    }
}

QStringList AgentControl::agentTabIds() const
{
    auto ids = m_attached.keys();
    ids.sort();
    return ids;
}

QVariantMap AgentControl::agentActivity() const
{
    QHash<QString, int> working;
    for (const auto &pending : std::as_const(m_pendingPages)) {
        working[openerOf(pending.tabId)] += 1;
    }
    QVariantMap activity;
    for (auto it = m_attached.cbegin(); it != m_attached.cend(); ++it) {
        activity.insert(it.key(),
            QVariantMap {
                {QStringLiteral("spaceId"), it->spaceId},
                {QStringLiteral("name"), it->name},
                {QStringLiteral("act"), it->act},
                {QStringLiteral("busy"), working.value(it.key()) > 0},
            });
    }
    return activity;
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
        {QStringLiteral("downloadDirectory"), downloadDirectoryFor(m_tabConnections.value(tabId))},
    };
}

QStringList AgentControl::agentWindowIds() const
{
    auto ids = m_windows.keys();
    ids.sort();
    return ids;
}

QString AgentControl::attachWindow(const QString &openerTabId)
{
    if (!m_allowAgents || !m_attached.contains(openerTabId)) {
        return {};
    }
    const auto id = QStringLiteral("window-%1").arg(m_nextWindow++);
    m_windows.insert(id,
        AgentWindow {
            .openerTabId = openerTabId, .connection = m_tabConnections.value(openerTabId)});
    m_newWindows[openerTabId].append(id);
    emit agentWindowsChanged();
    return id;
}

QVariantMap AgentControl::agentWindow(const QString &windowId) const
{
    const auto found = m_windows.constFind(windowId);
    if (found == m_windows.cend()) {
        return {};
    }
    const auto opener = m_browser->findTab(found->openerTabId);
    return {
        {QStringLiteral("windowId"), windowId},
        {QStringLiteral("openerTabId"), found->openerTabId},
        {QStringLiteral("spaceId"), opener ? opener->spaceId : QString {}},
        {QStringLiteral("connection"), found->connection},
        {QStringLiteral("downloadDirectory"), downloadDirectoryFor(found->connection)},
    };
}

void AgentControl::windowClosed(const QString &windowId)
{
    if (forgetWindow(windowId)) {
        emit agentWindowsChanged();
    }
}

bool AgentControl::forgetWindow(const QString &windowId)
{
    const auto window = m_windows.take(windowId);
    if (window.openerTabId.isEmpty()) {
        return false;
    }
    m_console.forget(windowId);
    m_newWindows[window.openerTabId].removeAll(windowId);
    for (auto &connection : m_connections) {
        if (connection.currentTabId == windowId) {
            connection.currentTabId.clear();
        }
    }
    return true;
}

// A window is the Agent's only while its opener is an Agent tab. Once no
// Agent holds the opener, nobody is left to answer what the window asks, so
// it goes back to the reader with it.
void AgentControl::releaseWindowsOf(const QString &openerTabId)
{
    QStringList released;
    for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it) {
        if (it->openerTabId == openerTabId) {
            released.append(it.key());
        }
    }
    for (const auto &windowId : std::as_const(released)) {
        forgetWindow(windowId);
    }
    if (!released.isEmpty()) {
        emit agentWindowsChanged();
    }
}

QString AgentControl::openerOf(const QString &target) const
{
    return isWindowId(target) ? m_windows.value(target).openerTabId : target;
}

QJsonObject AgentControl::noWindow(const QString &windowId)
{
    return refusal(QStringLiteral("not-found"),
        QStringLiteral("There is no window \"%1\". It has closed.").arg(windowId));
}

QString AgentControl::downloadDirectoryFor(const QString &name) const
{
    const auto root = m_browser->downloadDirectory();
    if (root.isEmpty()) {
        return {};
    }
    return QDir(root).filePath(QStringLiteral("Agents/") + directoryName(name));
}

void AgentControl::setShotDirectory(const QString &directory) { m_shotDirectory = directory; }

void AgentControl::setAttachmentIdleMs(int milliseconds)
{
    m_attachmentIdleMs = milliseconds;
    m_idleCheck.setInterval(std::min(idleCheckMs, std::max(milliseconds / 2, 1)));
}

void AgentControl::attach(
    const QString &tabId, const QString &spaceId, const QString &name, const QString &act)
{
    const auto known = m_attached.contains(tabId);
    auto &attachment = m_attached[tabId];
    attachment.lastUsed = m_clock.elapsed();
    attachment.spaceId = spaceId;
    if (!name.isEmpty()) {
        attachment.name = name;
    }
    if (!act.isEmpty()) {
        attachment.act = act;
    }
    // Another connection using the tab takes its downloads with it, which the
    // page hears as a change to the Agent tabs.
    const auto handedOver = !name.isEmpty() && m_tabConnections.value(tabId) != name;
    if (!name.isEmpty()) {
        m_tabConnections.insert(tabId, name);
    }
    if (!m_idleCheck.isActive()) {
        m_idleCheck.start();
    }
    if (!known || handedOver) {
        emit agentTabsChanged();
    }
    emit agentActivityChanged();
}

void AgentControl::detach(const QString &tabId)
{
    m_console.forget(tabId);
    m_tabConnections.remove(tabId);
    m_newWindows.remove(tabId);
    releaseWindowsOf(tabId);
    if (m_attached.remove(tabId) > 0) {
        if (m_attached.isEmpty()) {
            m_idleCheck.stop();
        }
        emit agentTabsChanged();
        emit agentActivityChanged();
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
        // A verb in one of a tab's windows is work in the tab.
        working.insert(openerOf(pending.tabId));
    }
    QStringList leaving;
    for (auto it = m_attached.cbegin(); it != m_attached.cend(); ++it) {
        if (working.contains(it.key())) {
            continue;
        }
        if (now - it->lastUsed >= m_attachmentIdleMs || !m_browser->findTab(it.key())) {
            leaving.append(it.key());
        }
    }
    for (const auto &tabId : std::as_const(leaving)) {
        m_attached.remove(tabId);
        m_tabConnections.remove(tabId);
        m_newWindows.remove(tabId);
        m_console.forget(tabId);
        releaseWindowsOf(tabId);
    }
    if (m_attached.isEmpty()) {
        m_idleCheck.stop();
    }
    if (!leaving.isEmpty()) {
        emit agentTabsChanged();
        emit agentActivityChanged();
    }
}

QJsonObject AgentControl::gate(const QString &verb) const
{
    static const QSet<QString> verbs {QStringLiteral("spaces"), QStringLiteral("tabs"),
        QStringLiteral("open"), QStringLiteral("close"), QStringLiteral("space new"),
        QStringLiteral("space delete"), QStringLiteral("look"), QStringLiteral("read"),
        QStringLiteral("do"), QStringLiteral("shot"), QStringLiteral("eval"),
        QStringLiteral("console"), QStringLiteral("commands"), QStringLiteral("run"),
        QStringLiteral("space"), QStringLiteral("focus"), QStringLiteral("dev")};
    if (!verbs.contains(verb)) {
        return refusal(
            QStringLiteral("unknown-verb"), QStringLiteral("Omaweb has no verb \"%1\".").arg(verb));
    }
    if (!m_browser || m_browser->privateBrowsing()) {
        return refusal(
            QStringLiteral("private"), QStringLiteral("A Private window is never reachable."));
    }
    if (gated(verb) && !m_allowAgents) {
        return refusal(QStringLiteral("allow-agents"),
            readsTab(verb)
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
    if (isWindowId(connection.currentTabId)) {
        if (!m_windows.contains(connection.currentTabId)) {
            connection.currentTabId.clear();
        }
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
    const auto name = request.value(QStringLiteral("name")).toString();
    // Which protocol this speaks reaches no page and no Space, so it is
    // answered before anything is gated, and is not activity.
    if (verb == u"hello") {
        reply(agentHelloAnswer(QCoreApplication::applicationVersion()));
        return;
    }
    if (const auto refused = gate(verb); !refused.isEmpty()) {
        // What is not a verb, and anything asked of a Private window, did
        // nothing to write down.
        const auto code = refused.value(QStringLiteral("code")).toString();
        if (code != u"bad-request" && code != u"unknown-verb" && code != u"private") {
            logActivity(verb, name, request, refused, {});
        }
        reply(refused);
        return;
    }
    auto &connection = connectionNamed(name);
    if (const auto revoked = std::exchange(connection.revokedSpaceName, {}); !revoked.isEmpty()) {
        const auto refused = refusal(QStringLiteral("revoked"),
            QStringLiteral("The reader revoked the grant to Space \"%1\", and this connection "
                           "was taken out of it. Using it again asks the reader again.")
                .arg(revoked));
        logActivity(verb, name, request, refused, {});
        reply(refused);
        return;
    }
    const auto previousTabId = connection.currentTabId;
    resolveCurrentTab(connection);
    if (!previousTabId.isEmpty() && connection.currentTabId.isEmpty()) {
        detach(previousTabId);
    }
    const auto scope = activityScope(verb, request, connection);
    const Reply logged = [this, verb, name, request, scope, reply](const QJsonObject &answer) {
        logActivity(verb, name, request, answer, scope);
        reply(answer);
    };
    // An upload is refused before the reader is asked, since no answer of
    // theirs would let it through.
    if (verb == u"do") {
        if (const auto refused = refuseUpload(connection, request); !refused.isEmpty()) {
            logged(refused);
            return;
        }
    }
    if (const auto spaceId = spaceNeedingGrant(verb, connection, request); !spaceId.isEmpty()) {
        const auto space = spaceName(spaceId);
        const auto denied = refusal(QStringLiteral("denied"),
            QStringLiteral("The reader denied the use of Space \"%1\". Make an Agent Space with "
                           "`space new` and work there.")
                .arg(space));
        if (connection.deniedSpaceIds.contains(spaceId)) {
            logged(denied);
            return;
        }
        askGrant(spaceId, name,
            [this, request, reply, logged, socketConnection, space, spaceId, name, denied](
                GrantAnswer answer) {
                switch (answer) {
                case GrantAnswer::Allowed:
                    handle(request, reply, socketConnection);
                    return;
                case GrantAnswer::Denied:
                    if (const auto found = m_connections.find(name); found != m_connections.end()) {
                        found->deniedSpaceIds.insert(spaceId);
                    }
                    logged(denied);
                    return;
                case GrantAnswer::Undecided:
                    logged(refusal(QStringLiteral("undecided"),
                        QStringLiteral("The reader has not decided whether an Agent may use Space "
                                       "\"%1\". Asking again shows the prompt again.")
                            .arg(space)));
                    return;
                case GrantAnswer::Withdrawn:
                    logged(refusal(QStringLiteral("allow-agents"),
                        QStringLiteral(
                            "Allow agents was turned off, so the page was left as it was.")));
                    return;
                case GrantAnswer::Failed:
                    logged(refusal(QStringLiteral("failed"),
                        QStringLiteral("The grant to Space \"%1\" could not be kept.").arg(space)));
                    return;
                case GrantAnswer::Gone:
                    logged(refusal(QStringLiteral("not-found"),
                        QStringLiteral("Space \"%1\" was deleted while the reader was asked.")
                            .arg(space)));
                    return;
                }
            });
        return;
    }
    if (pageVerb(verb)) {
        askPage(verb, name, connection, request, logged);
        return;
    }
    if (verb == u"console") {
        logged(readConsole(connection, request));
        return;
    }
    logged(answerBrowserCommand(verb, name, connection, request, socketConnection));
}

void AgentControl::setActivityLog(AgentActivityLog *log) { m_activity = log; }

QString AgentControl::activityTarget(const QString &verb, const QJsonObject &request) const
{
    const auto text = [&request](const QString &key) {
        return request.value(key).toString().left(maximumTargetLength);
    };
    if (verb == u"open") {
        // What the input became, not the input: words become a search, and
        // an address that is refused may be script.
        const auto input = request.value(QStringLiteral("url")).toString();
        const auto url = m_browser->resolveAddress(input);
        return openable(input, url) ? activityAddress(url) : QString {};
    }
    if (verb == u"tabs" || verb == u"space" || verb == u"space new" || verb == u"space delete") {
        return text(QStringLiteral("space"));
    }
    if (verb == u"run") {
        const auto argument = request.value(QStringLiteral("argument"));
        return argument.isDouble()
            ? QStringLiteral("%1 %2").arg(text(QStringLiteral("command"))).arg(argument.toInt())
            : text(QStringLiteral("command"));
    }
    if (verb == u"shot" && request.value(QStringLiteral("full")).toBool()) {
        return QStringLiteral("full page");
    }
    if (verb != u"do") {
        return {};
    }
    // A step is its action and the hint label it acted on. What it filled,
    // the option it chose, the key it pressed, the text or address it waited
    // for, the text a dialog was answered with and the files an upload gave
    // are left out, as is a target that is not a label: any of them can be
    // what the reader typed.
    static const QRegularExpression hintLabel(QStringLiteral("^[0-9]{1,9}$"));
    QStringList steps;
    for (const auto &value : request.value(QStringLiteral("steps")).toArray()) {
        const auto step = value.toObject();
        const auto action = step.value(QStringLiteral("action")).toString();
        if (!stepActions().contains(action)) {
            continue;
        }
        // A dialog's answer is accept or dismiss, never the text it gave.
        if (action == u"dialog") {
            const auto answer = step.value(QStringLiteral("answer")).toString();
            steps.append(answer == u"accept" || answer == u"dismiss"
                    ? QStringLiteral("dialog %1").arg(answer)
                    : action);
            continue;
        }
        const auto label = step.value(QStringLiteral("target")).toString();
        steps.append(action == u"press" || action == u"wait" || !hintLabel.match(label).hasMatch()
                ? action
                : QStringLiteral("%1 %2").arg(action, label));
    }
    return steps.join(QStringLiteral(", "));
}

// Taken before the verb runs, because a Space it deletes or a tab it closes
// is gone by the time it has answered.
AgentControl::ActivityScope AgentControl::activityScope(
    const QString &verb, const QJsonObject &request, const Connection &connection) const
{
    ActivityScope scope;
    QString tabId;
    if (verb == u"focus") {
        tabId = request.value(QStringLiteral("target")).toString();
    } else if (readsTab(verb) || verb == u"close"
        || (verb == u"open" && request.value(QStringLiteral("space")).toString().isEmpty()
            && !request.value(QStringLiteral("new")).toBool())) {
        tabId = request.value(QStringLiteral("tab")).toString();
        if (tabId.isEmpty()) {
            tabId = connection.currentTabId;
        }
    }
    if (!tabId.isEmpty()) {
        scope.tab = m_browser->findTab(
            tabId, tabId == connection.currentTabId ? connection.currentSpaceId : QString {});
    }
    const auto named = request.value(QStringLiteral("space")).toString();
    // `tabs --all` is about no one Space.
    const auto listsOneSpace = verb == u"tabs" && !request.value(QStringLiteral("all")).toBool();
    scope.spaceId = named.isEmpty() ? (listsOneSpace ? defaultSpace(connection) : QString {})
                                    : findSpace(named);
    scope.spaceName = spaceName(scope.spaceId);
    return scope;
}

void AgentControl::logActivity(const QString &verb, const QString &name, const QJsonObject &request,
    const QJsonObject &answer, const ActivityScope &scope)
{
    if (!m_activity) {
        return;
    }
    const auto ok = answer.value(QStringLiteral("ok")).toBool();
    AgentActivityLog::Entry entry;
    entry.agent = name;
    entry.verb = verb;
    entry.target = activityTarget(verb, request);
    entry.outcome = ok ? QStringLiteral("ok") : answer.value(QStringLiteral("code")).toString();
    const auto idOf = [](const QJsonValue &value) {
        return value.isObject() ? value.toObject().value(QStringLiteral("id")).toString()
                                : value.toString();
    };
    // The tab the verb acted on, at the address it had then. A tab the answer
    // names instead is one the verb opened.
    const auto answeredTab = ok ? idOf(answer.value(QStringLiteral("tab"))) : QString {};
    auto tab = scope.tab;
    if (!answeredTab.isEmpty() && (!tab || tab->id != answeredTab)) {
        tab = m_browser->findTab(answeredTab);
    }
    if (tab) {
        entry.tabId = tab->id;
        entry.address = activityAddress(tab->url);
        entry.spaceId = tab->spaceId;
    }
    if (entry.spaceId.isEmpty() && ok) {
        entry.spaceId = idOf(answer.value(QStringLiteral("space")));
    }
    if (entry.spaceId.isEmpty()) {
        entry.spaceId = scope.spaceId;
    }
    entry.space = spaceName(entry.spaceId);
    if (entry.space.isEmpty() && entry.spaceId == scope.spaceId) {
        entry.space = scope.spaceName;
    }
    m_activity->record(std::move(entry));
}

QString AgentControl::spaceName(const QString &spaceId) const
{
    if (spaceId.isEmpty()) {
        return {};
    }
    const auto *model = m_browser->spaces();
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto index = model->index(row, 0);
        if (index.data(SpaceListModel::IdRole).toString() == spaceId) {
            return index.data(SpaceListModel::NameRole).toString();
        }
    }
    return {};
}

QJsonObject AgentControl::answer(const QJsonObject &request, quint64 socketConnection)
{
    if (pageVerb(request.value(QStringLiteral("verb")).toString())) {
        return refusal(QStringLiteral("pending"),
            QStringLiteral("A page verb is answered by the page, through the socket."));
    }
    // A request the reader has to answer first is answered later, when this
    // call has returned, so the late answer goes nowhere.
    const auto answered = std::make_shared<std::optional<QJsonObject>>();
    handle(
        request,
        [answered](const QJsonObject &result) {
            if (!*answered) {
                *answered = result;
            }
        },
        socketConnection);
    if (!*answered) {
        *answered = QJsonObject {};
        return refusal(QStringLiteral("pending"),
            QStringLiteral("The reader is being asked whether an Agent may use the Space, and "
                           "the answer comes through the socket."));
    }
    return **answered;
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
        return open(name, connection, request);
    }
    if (verb == u"close") {
        return close(connection, request);
    }
    if (verb == u"space new") {
        return createSpace(name, connection, request, socketConnection);
    }
    if (verb == u"space delete") {
        return deleteSpace(name, connection, request);
    }
    if (verb == u"space") {
        return switchToSpace(request);
    }
    if (verb == u"focus") {
        return focusTab(request);
    }
    if (verb == u"dev") {
        return openProject(request);
    }
    return askWindow(verb, request);
}

// The command registry lives in the window, so `commands` and `run` are
// answered there. What is public is decided here first, so a command outside
// the list never reaches the window whatever the window would make of it.
QJsonObject AgentControl::askWindow(const QString &verb, const QJsonObject &request)
{
    QVariantMap asked {
        {QStringLiteral("verb"), verb},
        {QStringLiteral("commands"), publicCommands()},
    };
    if (verb == u"run") {
        const auto command = request.value(QStringLiteral("command")).toString();
        if (command.isEmpty()) {
            return refusal(QStringLiteral("bad-request"), QStringLiteral("Name a command to run."));
        }
        if (!publicCommands().contains(command)) {
            return refusal(QStringLiteral("refused"),
                QStringLiteral("\"%1\" is not a command Omaweb runs from outside its window. "
                               "`omaweb commands` lists those it does.")
                    .arg(command));
        }
        const auto given = request.value(QStringLiteral("argument"));
        auto position = -1;
        if (commandTakesPosition(command)) {
            const auto number = given.toDouble(0);
            if (!given.isDouble() || number < 1 || number != static_cast<double>(given.toInt())) {
                return refusal(QStringLiteral("bad-request"),
                    QStringLiteral("%1 takes a position, 1 for the first.").arg(command));
            }
            position = given.toInt() - 1;
        } else if (!given.isUndefined() && !given.isNull()) {
            return refusal(QStringLiteral("bad-request"),
                QStringLiteral("%1 takes no argument.").arg(command));
        }
        asked.insert(QStringLiteral("command"), command);
        asked.insert(QStringLiteral("argument"), position);
    }
    if (!isSignalConnected(QMetaMethod::fromSignal(&AgentControl::commandRequested))) {
        return refusal(QStringLiteral("unavailable"),
            QStringLiteral("This browser has no window to run the command in."));
    }
    m_commandRequest = m_nextCommandRequest++;
    m_commandAnswer.reset();
    emit commandRequested(m_commandRequest, asked);
    m_commandRequest = 0;
    const auto answered = std::exchange(m_commandAnswer, std::nullopt);
    if (!answered || !answered->value(QStringLiteral("ok")).isBool()) {
        return refusal(QStringLiteral("failed"), QStringLiteral("The window gave no answer."));
    }
    return *answered;
}

void AgentControl::answerCommand(int requestId, const QVariantMap &answer)
{
    if (requestId != m_commandRequest || m_commandAnswer) {
        return;
    }
    m_commandAnswer = QJsonObject::fromVariantMap(answer);
}

// Switching Space is what a keybind for a Space does, so it is open like the
// sidebar is. An Agent Space is one of the reader's Spaces to look at too.
QJsonObject AgentControl::switchToSpace(const QJsonObject &request)
{
    const auto named = request.value(QStringLiteral("space")).toString();
    if (named.isEmpty()) {
        return refusal(QStringLiteral("bad-request"), QStringLiteral("Name a Space to switch to."));
    }
    const auto spaceId = findSpace(named);
    if (spaceId.isEmpty()) {
        return noSpace(named);
    }
    if (!m_browser->switchSpace(spaceId)) {
        return refusal(
            QStringLiteral("failed"), QStringLiteral("Omaweb could not switch to the Space."));
    }
    return success({{QStringLiteral("space"), spaceId}});
}

// A tab by its id, or else the first whose address holds the text, looked for
// in the Space on show before the others in the sidebar's order. Selecting it
// switches to its Space, as choosing it in the Omnibar does. A Space not on
// show is read from its store, so each is read once, for both.
QJsonObject AgentControl::focusTab(const QJsonObject &request)
{
    const auto target = request.value(QStringLiteral("target")).toString();
    if (target.isEmpty()) {
        return refusal(QStringLiteral("bad-request"),
            QStringLiteral("Name a tab, or a part of its address, to select."));
    }
    QStringList spaceIds {m_browser->activeSpaceId()};
    const auto *model = m_browser->spaces();
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto id = model->index(row, 0).data(SpaceListModel::IdRole).toString();
        if (!spaceIds.contains(id)) {
            spaceIds.append(id);
        }
    }
    std::optional<TabState> tab;
    std::optional<TabState> byAddress;
    for (const auto &spaceId : std::as_const(spaceIds)) {
        for (const auto &each : m_browser->spaceTabs(spaceId)) {
            if (each.id == target) {
                tab = each;
                break;
            }
            if (!byAddress && each.url.toString().contains(target, Qt::CaseInsensitive)) {
                byAddress = each;
            }
        }
        if (tab) {
            break;
        }
    }
    if (!tab) {
        tab = byAddress;
    }
    if (!tab) {
        return refusal(QStringLiteral("not-found"),
            QStringLiteral("No tab is \"%1\" or has it in its address.").arg(target));
    }
    // Through the Space's store, as the Omnibar does: the tab is recorded as the one the Space is
    // left on before it comes on show, so a tab unused for long is not put away on the way in.
    if (!m_browser->activateTabInSpace(tab->spaceId, tab->id)) {
        return refusal(
            QStringLiteral("failed"), QStringLiteral("Omaweb could not switch to the tab."));
    }
    // Only when asked: no verb takes the reader's focus unprompted (ADR 0051), and an Agent that
    // selects a tab is not the reader asking to see it.
    if (request.value(QStringLiteral("raise")).toBool()) {
        emit windowRequested();
    }
    return success({{QStringLiteral("tab"), tab->id}, {QStringLiteral("space"), tab->spaceId}});
}

// `omaweb dev`, from the folder the CLI ran in. Any process can send it, so it
// is a browser command and reaches no page: it makes or finds the reader's own
// Space, records where its project is, and brings it forward. It never grants
// the Space and never makes its tabs an Agent's.
QJsonObject AgentControl::openProject(const QJsonObject &request)
{
    const auto sentDirectory = request.value(QStringLiteral("directory")).toString();
    if (sentDirectory.isEmpty() || QDir::isRelativePath(sentDirectory)) {
        return refusal(QStringLiteral("bad-request"),
            QStringLiteral("`dev` names the folder it was run in by its whole path."));
    }
    const auto directory = QDir::cleanPath(sentDirectory);
    const auto typedAddress = request.value(QStringLiteral("address")).toString().trimmed();
    QString spaceId;
    SpaceProject project {.directory = directory, .address = {}, .agentCommand = {}};
    if (!typedAddress.isEmpty()) {
        // Given an address, the folder is the project, even below another
        // project's: a monorepo's apps are a Space each.
        const auto url = m_browser->resolveAddress(typedAddress);
        const auto web = url.scheme() == u"http" || url.scheme() == u"https";
        // Words become a search, which is no project's address.
        if (!openable(typedAddress, url) || !web
            || !typedAddress.contains(url.host(), Qt::CaseInsensitive)) {
            return refusal(QStringLiteral("bad-request"),
                QStringLiteral("\"%1\" is not the address of a web app, such as localhost:5173.")
                    .arg(typedAddress));
        }
        const auto nearest = m_browser->projectSpaceFor(directory);
        if (const auto existing = m_browser->spaceProject(nearest);
            existing && QDir::cleanPath(existing->directory) == directory) {
            spaceId = nearest;
            project = *existing;
        }
        project.address = url.toString();
    } else {
        spaceId = m_browser->projectSpaceFor(directory);
        const auto existing = m_browser->spaceProject(spaceId);
        if (!existing || existing->address.isEmpty()) {
            return refusal(QStringLiteral("no-address"),
                QStringLiteral("No project in %1 or a folder above it has an address to open. "
                               "Give it once, such as `omaweb dev localhost:5173`.")
                    .arg(directory));
        }
        project = *existing;
    }
    if (const auto agent = request.value(QStringLiteral("agent")); agent.isString()) {
        project.agentCommand = agent.toString().trimmed();
    }
    if (spaceId.isEmpty()) {
        spaceId = m_browser->createProjectSpace(project);
    } else if (!m_browser->setSpaceProject(spaceId, project)) {
        spaceId.clear();
    }
    if (spaceId.isEmpty()) {
        return refusal(
            QStringLiteral("failed"), QStringLiteral("Omaweb could not keep the project's Space."));
    }
    if (!m_browser->switchSpace(spaceId)) {
        return refusal(
            QStringLiteral("failed"), QStringLiteral("Omaweb could not switch to the Space."));
    }
    // The reader ran it to see the app, as `focus --raise` brings a tab
    // forward.
    emit windowRequested();
    QJsonObject answer {{QStringLiteral("space"), spaceId},
        {QStringLiteral("spaceName"), spaceName(spaceId)},
        {QStringLiteral("directory"), project.directory},
        {QStringLiteral("address"), project.address}};
    // A tab already on the app is the app, at whatever page the reader left it.
    const auto origin = QUrl(project.address).adjusted(appOrigin);
    for (const auto &tab : m_browser->spaceTabs(spaceId)) {
        if (tab.url.adjusted(appOrigin) == origin
            && m_browser->activateTabInSpace(spaceId, tab.id)) {
            // A wait an earlier `dev` began would open the app a second time.
            m_browser->stopAwaitingAddress(spaceId);
            answer.insert(QStringLiteral("tab"), tab.id);
            return success(answer);
        }
    }
    // The CLI returns now. The road drives until the server answers, which
    // may be long after: Omaweb does not start it.
    m_browser->awaitAddress(spaceId, QUrl(project.address));
    return success(answer);
}

QString AgentControl::targetId(const Connection &connection, const QJsonObject &request) const
{
    const auto named = request.value(QStringLiteral("tab")).toString();
    return named.isEmpty() ? connection.currentTabId : named;
}

// The tab a page verb or `console` is about: the one `--tab` names or the
// connection's current one, and only a page an Agent may read. An Auxiliary
// window an Agent tab opened is read as its opener is.
QJsonObject AgentControl::pageTab(const Connection &connection, const QJsonObject &request,
    std::optional<TabState> &tab, QString &target) const
{
    target = targetId(connection, request);
    if (target.isEmpty()) {
        return refusal(QStringLiteral("no-current-tab"),
            QStringLiteral(
                "This connection has no current tab. Open one, or name one with --tab."));
    }
    const auto tabId = openerOf(target);
    if (tabId.isEmpty()) {
        return noWindow(target);
    }
    tab = m_browser->findTab(
        tabId, tabId == connection.currentTabId ? connection.currentSpaceId : QString {});
    if (!tab) {
        return refusal(
            QStringLiteral("not-found"), QStringLiteral("There is no tab \"%1\".").arg(tabId));
    }
    // The reader's account of what Agents did is not an Agent's to read, even
    // in an Agent Space. No engine holds it, so nothing would answer anyway.
    if (tab->url == BrowserController::agentActivityAddress()) {
        return refusal(QStringLiteral("refused"),
            QStringLiteral("The Agent activity page is the reader's. An Agent cannot read it."));
    }
    if (!mayRead(*tab)) {
        return refusal(QStringLiteral("refused"),
            QStringLiteral("An Agent reads and drives only the pages of an Agent Space, or of a "
                           "Space the reader granted. Make one with `space new` and open the "
                           "address there."));
    }
    return {};
}

// Answered from what the tab's page has already logged, so it needs no page
// to ask. Reading a tab's console makes it the connection's, and an Agent tab
// from here on, so what its page says next is kept.
QJsonObject AgentControl::readConsole(Connection &connection, const QJsonObject &request)
{
    auto threshold = AgentConsole::Info;
    if (!AgentConsole::parseThreshold(
            request.value(QStringLiteral("level")).toString(), &threshold)) {
        return refusal(
            QStringLiteral("bad-request"), QStringLiteral("--level is error, warning or all."));
    }
    const auto sinceValue = request.value(QStringLiteral("since"));
    if (!sinceValue.isUndefined() && (!sinceValue.isDouble() || sinceValue.toDouble() < 0)) {
        return refusal(QStringLiteral("bad-request"),
            QStringLiteral("--since is the cursor a `console` answered."));
    }
    std::optional<TabState> tab;
    QString target;
    if (const auto refused = pageTab(connection, request, tab, target); !refused.isEmpty()) {
        return refused;
    }
    connection.currentTabId = target;
    connection.currentSpaceId = tab->spaceId;
    attach(tab->id, tab->spaceId, request.value(QStringLiteral("name")).toString(),
        QStringLiteral("read the console"));

    const auto reading
        = m_console.read(target, threshold, static_cast<quint64>(sinceValue.toDouble(0)));
    QJsonArray messages;
    for (const auto &message : reading.messages) {
        messages.append(QJsonObject {
            {QStringLiteral("cursor"), static_cast<double>(message.cursor)},
            {QStringLiteral("level"), AgentConsole::levelName(message.level)},
            {QStringLiteral("message"), message.text},
            {QStringLiteral("source"), message.source},
            {QStringLiteral("line"), message.line},
        });
    }
    return success({
        {QStringLiteral("tab"), target},
        {QStringLiteral("messages"), messages},
        {QStringLiteral("cursor"), static_cast<double>(reading.cursor)},
        {QStringLiteral("truncated"), reading.truncated},
    });
}

void AgentControl::recordConsoleMessage(const QString &tabId, const QString &document, int level,
    const QString &message, const QString &source, int line)
{
    // Only an Agent tab is listened to. A page the reader has to themselves
    // says nothing that is kept.
    if (!m_allowAgents || (!m_attached.contains(tabId) && !m_windows.contains(tabId))) {
        return;
    }
    m_console.record(tabId, document, level, message, source, line);
}

void AgentControl::startConsoleDocument(const QString &tabId, const QString &document)
{
    m_console.start(tabId, document);
}

// Checked before whether the Agent may read the page at all, so that nothing
// which lets an Agent into one of the reader's Spaces, a Space grant included,
// lets it upload there.
QJsonObject AgentControl::refuseUpload(
    const Connection &connection, const QJsonObject &request) const
{
    const auto steps = request.value(QStringLiteral("steps")).toArray();
    const auto uploads = std::any_of(steps.begin(), steps.end(), [](const QJsonValue &step) {
        return step.toObject().value(QStringLiteral("action")).toString() == u"upload";
    });
    if (!uploads) {
        return {};
    }
    const auto tab = m_browser->findTab(openerOf(targetId(connection, request)));
    if (!tab || m_browser->agentSpace(tab->spaceId)) {
        return {};
    }
    return refusal(QStringLiteral("refused"),
        QStringLiteral("An Agent uploads files only in an Agent Space. Anywhere else an upload is "
                       "how a page could take the reader's files."));
}

void AgentControl::askPage(const QString &verb, const QString &name, Connection &connection,
    const QJsonObject &request, const Reply &reply)
{
    std::optional<TabState> tab;
    QString target;
    if (const auto refused = pageTab(connection, request, tab, target); !refused.isEmpty()) {
        reply(refused);
        return;
    }
    const auto tabId = tab->id;
    // Asked first, so no screenshot's file is made for a page nobody holds.
    if (!isSignalConnected(QMetaMethod::fromSignal(&AgentControl::pageRequested))) {
        reply(refusal(QStringLiteral("unavailable"),
            QStringLiteral("This browser has no page to answer the verb.")));
        return;
    }
    QVariantMap arguments;
    if (const auto refused = pageArguments(verb, request, arguments); !refused.isEmpty()) {
        reply(refused);
        return;
    }

    connection.currentTabId = target;
    connection.currentSpaceId = tab->spaceId;

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
        emit agentActivityChanged();
        removeUntakenShot(pending);
        pending.reply(
            refusal(QStringLiteral("timeout"), QStringLiteral("The page did not answer in time.")));
    });
    m_pendingPages.insert(requestId,
        PendingPage {.reply = reply,
            .deadline = deadline,
            .tabId = target,
            .verb = verb,
            .steps = arguments.value(QStringLiteral("steps")).toList(),
            .shot = arguments.value(QStringLiteral("destination")).toString()});
    // Attached once the request is out, so the tab is told busy from the
    // first moment it is an Agent tab.
    attach(tabId, tab->spaceId, name);
    deadline->start();
    emit pageRequested(requestId,
        QVariantMap {
            {QStringLiteral("verb"), verb},
            {QStringLiteral("tabId"), target},
            {QStringLiteral("window"), target != tabId},
            {QStringLiteral("spaceId"), tab->spaceId},
            {QStringLiteral("url"), tab->url},
            {QStringLiteral("name"), name},
            {QStringLiteral("downloadDirectory"), downloadDirectoryFor(name)},
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
    if (!result.value(QStringLiteral("ok")).toBool()) {
        removeUntakenShot(pending);
    }
    result.insert(QStringLiteral("tab"), pending.tabId);
    // A verb in one of a tab's windows is the tab's Agent at work there.
    if (const auto attachment = m_attached.find(openerOf(pending.tabId));
        attachment != m_attached.end()) {
        attachment->lastUsed = m_clock.elapsed();
        if (const auto act = describeAct(pending.verb, pending.steps, result); !act.isEmpty()) {
            attachment->act = act;
        }
    }
    // The windows the page opened since it last answered, which the Agent
    // reaches by these ids.
    if (const auto opened = m_newWindows.take(pending.tabId); !opened.isEmpty()) {
        result.insert(QStringLiteral("opened"), QJsonArray::fromStringList(opened));
    }
    emit agentActivityChanged();
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
    // The page's picture is written as the name's suffix says.
    if (!name.isEmpty() && !name.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        refused = refusal(QStringLiteral("bad-request"),
            QStringLiteral("A screenshot is a PNG, so name it with .png, such as page.png."));
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

// The empty file made for a screenshot the page did not take, so it neither
// lies there nor keeps its name from the next try.
void AgentControl::removeUntakenShot(const PendingPage &pending)
{
    if (!pending.shot.isEmpty()) {
        QFile::remove(pending.shot);
    }
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
        if (action == u"dialog") {
            const auto answer = step.value(QStringLiteral("answer")).toString();
            if (answer != u"accept" && answer != u"dismiss") {
                return at(QStringLiteral("`dialog` is answered with accept or dismiss."));
            }
            if (!text.isUndefined() && !text.isString()) {
                return at(QStringLiteral("`dialog accept` takes the text to answer with."));
            }
        }
        if (action == u"upload") {
            const auto files = step.value(QStringLiteral("files")).toArray();
            if (target.isEmpty() || files.isEmpty()) {
                return at(QStringLiteral("`upload` needs a label and at least one file."));
            }
            if (files.size() > maximumUploadFiles) {
                return at(
                    QStringLiteral("`upload` takes at most %1 files.").arg(maximumUploadFiles));
            }
            // Named by the Agent in full, and each one a file the reader can
            // read, so what the page is given is exactly what was named.
            QVariantList paths;
            for (const auto &file : files) {
                const QFileInfo info(file.toString());
                if (!info.isAbsolute()) {
                    return at(
                        QStringLiteral("give the file's whole path: %1").arg(file.toString()));
                }
                if (!info.isFile() || !info.isReadable()) {
                    return at(
                        QStringLiteral("there is no file to read at %1").arg(file.toString()));
                }
                paths.append(info.canonicalFilePath());
            }
            auto checkedStep = step.toVariantMap();
            checkedStep.insert(QStringLiteral("files"), paths);
            checked.append(checkedStep);
            continue;
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
        // Checked as the number it is, since one past an int's range has no
        // int to be converted to.
        const auto number = given.toDouble();
        if (!(number >= lowest && number <= highest)) {
            return false;
        }
        value = static_cast<int>(number);
        return true;
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

bool AgentControl::usableSpace(const QString &spaceId) const
{
    return m_allowAgents && (m_browser->agentSpace(spaceId) || m_browser->spaceGranted(spaceId));
}

// A tab an Agent opened this run, or any tab of a Space it may use. A Pinned
// tab's address is the reader's, whatever the Space.
bool AgentControl::mayLoad(const TabState &tab) const
{
    return !tab.pinned && (m_openedTabIds.contains(tab.id) || mayRead(tab));
}

// A grant lets an Agent use the reader's tabs, not take them away: in a
// granted Space it closes only the tabs an Agent opened.
bool AgentControl::mayClose(const TabState &tab) const
{
    return !tab.pinned
        && (m_openedTabIds.contains(tab.id)
            || (m_allowAgents && m_browser->agentSpace(tab.spaceId)));
}

// A page verb reads the page and acts in it as the Space's own identity, with
// its cookies and its logins. That is an Agent Space's, or a Space's the
// reader granted, whoever opened the tab: a tab an Agent opened in any other
// of the reader's Spaces is the reader's page. A Pinned tab is used as any
// other, and only its pin is out of reach.
bool AgentControl::mayRead(const TabState &tab) const { return usableSpace(tab.spaceId); }

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
    if (request.value(QStringLiteral("all")).toBool()) {
        if (!named.isEmpty()) {
            return refusal(QStringLiteral("bad-request"),
                QStringLiteral("`tabs --all` lists every Space, so it takes no --space."));
        }
        return listAllTabs(connection);
    }
    const auto spaceId = named.isEmpty() ? defaultSpace(connection) : findSpace(named);
    if (spaceId.isEmpty()) {
        return noSpace(named);
    }
    QJsonArray tabs;
    for (const auto &tab : m_browser->spaceTabs(spaceId)) {
        tabs.append(describeTab(tab, connection));
    }
    for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it) {
        const auto opener = m_browser->findTab(it->openerTabId);
        if (!opener || opener->spaceId != spaceId) {
            continue;
        }
        tabs.append(QJsonObject {
            {QStringLiteral("id"), it.key()},
            {QStringLiteral("space"), spaceId},
            {QStringLiteral("window"), true},
            {QStringLiteral("opener"), it->openerTabId},
            {QStringLiteral("current"), it.key() == connection.currentTabId},
        });
    }
    return success({{QStringLiteral("space"), spaceId}, {QStringLiteral("tabs"), tabs}});
}

// Every Space's tabs in the order the reader sees them, for a launcher that
// searches them all. An Auxiliary window has no title to search by, so only
// tabs are listed.
QJsonObject AgentControl::listAllTabs(const Connection &connection) const
{
    QJsonArray tabs;
    const auto *model = m_browser->spaces();
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto spaceId = model->index(row, 0).data(SpaceListModel::IdRole).toString();
        const auto name = spaceName(spaceId);
        for (const auto &tab : m_browser->spaceTabs(spaceId)) {
            auto described = describeTab(tab, connection);
            described.insert(QStringLiteral("spaceName"), name);
            tabs.append(described);
        }
    }
    return success({{QStringLiteral("tabs"), tabs}});
}

// Opening an address is a browser command, as the desktop's own handover is.
// It never selects the tab, so the page in front of the reader stays there.
QJsonObject AgentControl::open(
    const QString &name, Connection &connection, const QJsonObject &request)
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
    if (const auto tabId = tabToLoad(connection, request); !tabId.isEmpty()) {
        auto tab = m_browser->findTab(tabId, connection.currentSpaceId);
        if (!tab) {
            return refusal(
                QStringLiteral("not-found"), QStringLiteral("There is no tab \"%1\".").arg(tabId));
        }
        if (tab->pinned) {
            return refusal(QStringLiteral("refused"),
                QStringLiteral("A Pinned tab's address is the reader's to change."));
        }
        if (!mayLoad(*tab)) {
            return refusal(QStringLiteral("refused"),
                QStringLiteral("An Agent loads an address only in a tab an Agent opened, or one of "
                               "an Agent Space or a granted Space. Open a new tab instead."));
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
            attach(tabId, tab->spaceId, name, describeOpened(url));
        }
        tab->url = url;
        tab->title = BrowserController::addressTitle(url);
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
    if (usableSpace(spaceId)) {
        attach(tabId, spaceId, name, describeOpened(url));
    }
    const auto tab = m_browser->findTab(tabId, spaceId);
    return success({{QStringLiteral("tab"), tab ? describeTab(*tab, connection) : QJsonObject {}}});
}

// An Agent closes a tab an Agent opened, or any ordinary tab of an Agent
// Space. Every other tab is the reader's, in a granted Space too.
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
    if (isWindowId(tabId)) {
        if (!m_windows.contains(tabId)) {
            return noWindow(tabId);
        }
        emit windowCloseRequested(tabId);
        windowClosed(tabId);
        return success({{QStringLiteral("closed"), tabId}});
    }
    const auto tab = m_browser->findTab(tabId, connection.currentSpaceId);
    if (!tab) {
        return refusal(
            QStringLiteral("not-found"), QStringLiteral("There is no tab \"%1\".").arg(tabId));
    }
    if (!mayClose(*tab)) {
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
