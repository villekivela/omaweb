#pragma once

#include "AgentConsole.h"
#include "TabListModel.h"

#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <functional>
#include <optional>

namespace omaweb {

class AgentActivityLog;
class BrowserController;

// What an Agent may do through the control socket, and the one setting that
// widens it (ADR 0051).
//
// The socket cannot tell the reader's own script from a coding agent, so what
// a request may do depends on what its verb reaches and never on who asks.
// Browser commands (listing Spaces and tabs, opening an address, closing a tab
// an Agent opened) are always open, because they are what a keybind or a
// script needs and none of them reads a page. Switching Space, selecting a tab
// and running a public command of the command scope are browser commands too.
// Agent Spaces and the page verbs (`look`, `read`, `do`, `shot` and `eval`)
// wait for Allow agents, which is off until the reader turns it on.
//
// A page verb is answered by the page, which only the interface can reach. The
// core checks what the verb may reach, then hands the request on through
// `pageRequested` and replies when `answerPage` brings the answer back, or when
// the page has taken longer than any verb should.
//
// A request names its connection. The name picks that connection's state,
// its current tab and the Space its next tab goes to, so the CLI, which is one
// process per verb, keeps the tab it opened. It is used for the log and the
// markers and is not an identity. The one thing it decides is which Agent
// Space an Agent may delete, which guards against a mistake rather than
// against someone who means it.
//
// Only the ordinary window's controller is handed to this. A Private window
// has no Spaces to list, and one handed over by mistake is refused whole.
class AgentControl final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool allowAgents READ allowAgents WRITE setAllowAgents NOTIFY allowAgentsChanged)
    // The Agent tabs: the tabs a connection has opened or used a page verb on
    // lately. The interface keeps each rendered and running while it is
    // listed, wherever its Space is.
    Q_PROPERTY(QStringList agentTabIds READ agentTabIds NOTIFY agentTabsChanged)
    // What each Agent tab's Agent is doing, for the marks the interface draws
    // on its row, its page and its Space: by tab id, the tab's `spaceId`, the
    // connection's `name`, its last `act` in words, and `busy` while one of
    // its page verbs is in flight.
    Q_PROPERTY(QVariantMap agentActivity READ agentActivity NOTIFY agentActivityChanged)
    // The Auxiliary windows an Agent tab opened, which the connection drives
    // as it drives the tab, by these ids. The interface marks each as it
    // marks its tab.
    Q_PROPERTY(QStringList agentWindowIds READ agentWindowIds NOTIFY agentWindowsChanged)

public:
    // The most connection states kept at once. A name costs nothing to invent,
    // and the state used longest ago goes to make room.
    static constexpr qsizetype maximumConnections = 256;

    // How long a tab stays an Agent tab after the last verb that used it. The
    // CLI is a process per verb, so no connection stays open to say an Agent
    // is still working; a tab it has left alone this long stops costing the
    // reader a rendered page, and the next verb attaches it again.
    static constexpr int defaultAttachmentIdleMs = 5 * 60 * 1000;

    using Reply = std::function<void(const QJsonObject &)>;

    // Reads Allow agents from `privacy.json` under `configRoot` and follows
    // the file, so the reader turning it off there detaches every connection
    // without a restart.
    AgentControl(BrowserController *browser, QString configRoot, QObject *parent = nullptr);

    bool allowAgents() const;
    // Turning it off detaches every connection from the tab it was driving.
    void setAllowAgents(bool allowed);

    // Whether a verb waits for Allow agents.
    static bool gated(const QString &verb);
    // Whether a verb is answered by a page rather than by the core.
    static bool pageVerb(const QString &verb);

    // The commands of `BrowserCommands.qml` that `commands` lists and `run`
    // runs: every one but `private-window`, because a Private window is never
    // an Agent's, and the four screenshots, which read the page and so are
    // `shot`, behind Allow agents. A command outside it is refused by name.
    static const QStringList &publicCommands();
    // Whether a public command takes a position: `select-tab` and
    // `select-space`, counted from 1 as their keys are.
    static bool commandTakesPosition(const QString &command);

    // One request, `{"verb": ..., "name": ..., ...}`, and its answer:
    // `{"ok": true, ...}` or `{"ok": false, "code": ..., "error": ...}`. The
    // error is a sentence for the reader or the Agent; the code is for a
    // front end to act on. `reply` is called once, straight away for a browser
    // command and when the page answers for a page verb.
    //
    // `connection` names the socket connection the request came over, which
    // is what a temporary Agent Space lasts as long as. Nothing else reads it,
    // and a request with none cannot make one.
    void handle(const QJsonObject &request, const Reply &reply, quint64 connection = 0);
    // The same, for a request that needs no page. A page verb answers
    // `pending` here and is answered only through `handle`.
    QJsonObject answer(const QJsonObject &request, quint64 connection = 0);

    // The socket connection is gone, and every temporary Agent Space it made
    // goes with it, unless the reader has taken one over.
    void connectionClosed(quint64 connection);

    // The page's answer to the request `pageRequested` numbered.
    Q_INVOKABLE void answerPage(int requestId, const QVariantMap &answer);
    // The window's answer to the request `commandRequested` numbered, given
    // before that signal returns.
    Q_INVOKABLE void answerCommand(int requestId, const QVariantMap &answer);

    // A line an Agent tab's page wrote to its console, at the engine's level
    // (0 info, 1 warning, 2 error), for `console`. `document` changes when the
    // tab loads another document, or its page is built again. A tab that is not an Agent tab is not
    // listened to, and a tab's lines are forgotten when it stops being one.
    // The page area passes only the page's own lines, never Omaweb's reports.
    Q_INVOKABLE void recordConsoleMessage(const QString &tabId, const QString &document, int level,
        const QString &message, const QString &source, int line);

    QStringList agentTabIds() const;
    QVariantMap agentActivity() const;
    // What the interface needs to build an Agent tab's page: `tabId`,
    // `spaceId`, `url`, `zoom` and `muted`. Empty for a tab that is not one.
    Q_INVOKABLE QVariantMap agentTab(const QString &tabId) const;

    // Where every verb is written down once it is answered. Without one,
    // nothing is.
    void setActivityLog(AgentActivityLog *log);

    QStringList agentWindowIds() const;
    // An Auxiliary window the page of `openerTabId` opened. While the opener
    // is an Agent tab, the window becomes one of the Agent's, under the id
    // this answers; otherwise it answers nothing and stays the reader's.
    Q_INVOKABLE QString attachWindow(const QString &openerTabId);
    // What the interface needs to mark and drive one: `windowId`,
    // `openerTabId`, `spaceId`, `connection` and `downloadDirectory`. Empty
    // for a window that is not an Agent's.
    Q_INVOKABLE QVariantMap agentWindow(const QString &windowId) const;
    // The window has closed, whether the Agent closed it or the reader did.
    Q_INVOKABLE void windowClosed(const QString &windowId);

    // Where `shot` writes every screenshot: a directory only this user can
    // enter, beside the socket.
    void setShotDirectory(const QString &directory);
    // Tests shorten how long a tab stays an Agent tab unused.
    void setAttachmentIdleMs(int milliseconds);

signals:
    void allowAgentsChanged();
    void agentTabsChanged();
    void agentActivityChanged();
    void agentWindowsChanged();
    // An Agent closed the Auxiliary window of this id.
    void windowCloseRequested(const QString &windowId);
    // A page verb for the page of `request.tabId`, with `verb`, `spaceId`, the
    // tab's `url`, the connection's `name`, the connection's
    // `downloadDirectory` and the verb's own `arguments`. `window` is true for
    // an Auxiliary window, whose id is `tabId`.
    // Whoever holds the page answers it with `answerPage(requestId, ...)`.
    void pageRequested(int requestId, const QVariantMap &request);
    // Every page request still out has been refused, and the pages working
    // on them stop without sending more input or answering.
    void pageRequestsCancelled();
    // `commands` or `run` for the ordinary window, which holds the command
    // registry: `verb`, the `commands` it may list or run, and for `run` the
    // `command` and its `argument`, a position counted from 0, or -1. The
    // window answers with `answerCommand` before the signal returns.
    void commandRequested(int requestId, const QVariantMap &request);

private:
    struct Connection {
        QString currentTabId;
        // The current tab's Space while there is one, which is where it is
        // looked for first.
        QString currentSpaceId;
        quint64 lastUsed = 0;
    };

    struct PendingPage {
        Reply reply;
        QTimer *deadline = nullptr;
        QString tabId;
        // What was asked, so the answer can be told as the Agent's last act.
        QString verb;
        QVariantList steps;
    };

    struct Attachment {
        // When a verb last used the tab, on `m_clock`.
        qint64 lastUsed = 0;
        QString spaceId;
        QString name;
        QString act;
    };

    struct AgentWindow {
        QString openerTabId;
        QString connection;
    };

    void reload();
    void apply(bool allowed);
    QJsonObject gate(const QString &verb) const;
    void resolveCurrentTab(Connection &connection) const;
    QJsonObject answerBrowserCommand(const QString &verb, const QString &name,
        Connection &connection, const QJsonObject &request, quint64 socketConnection);
    // The tab, or the Auxiliary window, `--tab` or the current tab names.
    QString targetId(const Connection &connection, const QJsonObject &request) const;
    // `tab` is the page's tab, which for an Auxiliary window is its opener's,
    // and `target` the id the page answers for.
    QJsonObject pageTab(const Connection &connection, const QJsonObject &request,
        std::optional<TabState> &tab, QString &target) const;
    // The tab a target is the page of: a window's opener, a tab itself, or
    // nothing for a window that has closed.
    QString openerOf(const QString &target) const;
    static QJsonObject noWindow(const QString &windowId);
    bool forgetWindow(const QString &windowId);
    void releaseWindowsOf(const QString &openerTabId);
    // A batch with an upload in it, refused outside an Agent Space whatever
    // else the Agent may do there.
    QJsonObject refuseUpload(const Connection &connection, const QJsonObject &request) const;
    QJsonObject readConsole(Connection &connection, const QJsonObject &request);
    QJsonObject askWindow(const QString &verb, const QJsonObject &request);
    QJsonObject switchToSpace(const QJsonObject &request);
    QJsonObject focusTab(const QJsonObject &request);
    void askPage(const QString &verb, const QString &name, Connection &connection,
        const QJsonObject &request, const Reply &reply);
    // The verb's own arguments as the page is to get them, or a refusal.
    QJsonObject pageArguments(
        const QString &verb, const QJsonObject &request, QVariantMap &out) const;
    // `act` is left as it was when empty.
    void attach(
        const QString &tabId, const QString &spaceId, const QString &name, const QString &act = {});
    void detach(const QString &tabId);
    void detachIdle();
    Connection &connectionNamed(const QString &name);
    bool mayDrive(const TabState &tab) const;
    bool mayRead(const TabState &tab) const;
    // Where the connection's downloads land: a directory of its own under the
    // reader's downloads location. Empty when there is no location.
    QString downloadDirectoryFor(const QString &name) const;
    QString reserveShot(const QString &name, QJsonObject &refused) const;
    void pruneShots() const;

    QJsonObject listSpaces() const;
    QJsonObject listTabs(Connection &connection, const QJsonObject &request) const;
    QJsonObject open(const QString &name, Connection &connection, const QJsonObject &request);
    QJsonObject close(Connection &connection, const QJsonObject &request);
    QJsonObject createSpace(const QString &creator, Connection &connection,
        const QJsonObject &request, quint64 socketConnection);
    QJsonObject deleteSpace(
        const QString &requester, Connection &connection, const QJsonObject &request);
    // A Space by id, or by a name no other Space shares.
    QString findSpace(const QString &idOrName) const;
    // The Space a request without `--space` is about: the current tab's, the
    // connection's own, or the Space on show when it has never had one.
    // Nothing when its own Space is gone.
    QString defaultSpace(const Connection &connection) const;
    static QJsonObject noSpace(const QString &named);
    QJsonObject describeTab(const TabState &tab, const Connection &connection) const;
    // What a request is about before it runs: its tab, and the Space it
    // names by the name it has then.
    struct ActivityScope {
        std::optional<TabState> tab;
        QString spaceId;
        QString spaceName;
    };
    ActivityScope activityScope(
        const QString &verb, const QJsonObject &request, const Connection &connection) const;
    // What a verb acted on, as the activity log keeps it: an address, a hint
    // label, a Space or a command. Never a page's text, a value a step filled,
    // the option it chose, a key it pressed, the text it waited for, a
    // selector or the source it evaluated.
    QString activityTarget(const QString &verb, const QJsonObject &request) const;
    void logActivity(const QString &verb, const QString &name, const QJsonObject &request,
        const QJsonObject &answer, const ActivityScope &scope);
    QString spaceName(const QString &spaceId) const;

    BrowserController *m_browser;
    QString m_configRoot;
    bool m_allowAgents = false;
    QHash<QString, Connection> m_connections;
    quint64 m_requests = 0;
    // Every tab an Agent opened in this run, so a reader's own tab is never
    // one an Agent can load or close. It is not kept across a restart, after
    // which only an Agent Space's tabs are the Agents' own.
    QSet<QString> m_openedTabIds;
    QFileSystemWatcher m_watcher;
    QString m_shotDirectory;
    AgentActivityLog *m_activity = nullptr;
    int m_nextPageRequest = 1;
    int m_nextCommandRequest = 1;
    // The window's answer to the command request under way, while
    // `commandRequested` is being emitted.
    int m_commandRequest = 0;
    std::optional<QJsonObject> m_commandAnswer;
    QHash<int, PendingPage> m_pendingPages;
    // Each Agent tab, and what its Agent last did there.
    QHash<QString, Attachment> m_attached;
    QElapsedTimer m_clock;
    QTimer m_idleCheck;
    AgentConsole m_console;
    int m_attachmentIdleMs = defaultAttachmentIdleMs;
    // The connection that last used each Agent tab. The tab's downloads go to
    // that connection's directory.
    QHash<QString, QString> m_tabConnections;
    QHash<QString, AgentWindow> m_windows;
    int m_nextWindow = 1;
    // The Auxiliary windows each Agent tab opened that no answer has named
    // yet, so the `do` that opened one says so.
    QHash<QString, QStringList> m_newWindows;
    // The temporary Agent Spaces each socket connection made.
    QHash<quint64, QStringList> m_temporarySpaces;
};

} // namespace omaweb
