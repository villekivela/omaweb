#pragma once

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

namespace omaweb {

class BrowserController;

// What an Agent may do through the control socket, and the one setting that
// widens it (ADR 0051).
//
// The socket cannot tell the reader's own script from a coding agent, so what
// a request may do depends on what its verb reaches and never on who asks.
// Browser commands (listing Spaces and tabs, opening an address, closing a tab
// an Agent opened) are always open, because they are what a keybind or a
// script needs and none of them reads a page. Agent Spaces and the page verbs
// (`look`, `read`, `do`, `shot` and `eval`) wait for Allow agents, which is off
// until the reader turns it on.
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

    QStringList agentTabIds() const;
    // What the interface needs to build an Agent tab's page: `tabId`,
    // `spaceId`, `url`, `zoom` and `muted`. Empty for a tab that is not one.
    Q_INVOKABLE QVariantMap agentTab(const QString &tabId) const;

    // Where `shot` writes every screenshot: a directory only this user can
    // enter, beside the socket.
    void setShotDirectory(const QString &directory);
    // Tests shorten how long a tab stays an Agent tab unused.
    void setAttachmentIdleMs(int milliseconds);

signals:
    void allowAgentsChanged();
    void agentTabsChanged();
    // A page verb for the page of `request.tabId`, with `verb`, `spaceId`, the
    // tab's `url`, the connection's `name` and the verb's own `arguments`.
    // Whoever holds the page answers it with `answerPage(requestId, ...)`.
    void pageRequested(int requestId, const QVariantMap &request);
    // Every page request still out has been refused, and the pages working
    // on them stop without sending more input or answering.
    void pageRequestsCancelled();

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
    };

    void reload();
    void apply(bool allowed);
    QJsonObject gate(const QString &verb) const;
    void resolveCurrentTab(Connection &connection) const;
    QJsonObject answerBrowserCommand(const QString &verb, const QString &name,
        Connection &connection, const QJsonObject &request, quint64 socketConnection);
    void askPage(const QString &verb, const QString &name, Connection &connection,
        const QJsonObject &request, const Reply &reply);
    // The verb's own arguments as the page is to get them, or a refusal.
    QJsonObject pageArguments(
        const QString &verb, const QJsonObject &request, QVariantMap &out) const;
    void attach(const QString &tabId);
    void detach(const QString &tabId);
    void detachIdle();
    Connection &connectionNamed(const QString &name);
    bool mayDrive(const TabState &tab) const;
    bool mayRead(const TabState &tab) const;
    QString reserveShot(const QString &name, QJsonObject &refused) const;
    void pruneShots() const;

    QJsonObject listSpaces() const;
    QJsonObject listTabs(Connection &connection, const QJsonObject &request) const;
    QJsonObject open(Connection &connection, const QJsonObject &request);
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
    int m_nextPageRequest = 1;
    QHash<int, PendingPage> m_pendingPages;
    // Each Agent tab, and when a verb last used it on this clock.
    QHash<QString, qint64> m_attached;
    QElapsedTimer m_clock;
    QTimer m_idleCheck;
    int m_attachmentIdleMs = defaultAttachmentIdleMs;
    // The temporary Agent Spaces each socket connection made.
    QHash<quint64, QStringList> m_temporarySpaces;
};

} // namespace omaweb
