#pragma once

#include "TabListModel.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

namespace omaweb {

class BrowserController;

// What an Agent may do through the control socket, and the one setting that
// widens it (ADR 0051).
//
// The socket cannot tell the reader's own script from a coding agent, so what
// a request may do depends on what its verb reaches and never on who asks.
// Browser commands (listing Spaces and tabs, opening an address, closing a tab
// an Agent opened) are always open, because they are what a keybind or a
// script needs and none of them reads a page. Agent Spaces, and the page verbs
// that later slices add, wait for Allow agents, which is off until the reader
// turns it on.
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

public:
    // The most connection states kept at once. A name costs nothing to invent,
    // and the state used longest ago goes to make room.
    static constexpr qsizetype maximumConnections = 256;

    // Reads Allow agents from `privacy.json` under `configRoot` and follows
    // the file, so the reader turning it off there detaches every connection
    // without a restart.
    AgentControl(BrowserController *browser, QString configRoot, QObject *parent = nullptr);

    bool allowAgents() const;
    // Turning it off detaches every connection from the tab it was driving.
    void setAllowAgents(bool allowed);

    // Whether a verb waits for Allow agents.
    static bool gated(const QString &verb);

    // One request, `{"verb": ..., "name": ..., ...}`, and its answer:
    // `{"ok": true, ...}` or `{"ok": false, "code": ..., "error": ...}`. The
    // error is a sentence for the reader or the Agent; the code is for a
    // front end to act on.
    //
    // `connection` names the socket connection the request came over, which
    // is what a temporary Agent Space lasts as long as. Nothing else reads it,
    // and a request with none cannot make one.
    QJsonObject answer(const QJsonObject &request, quint64 connection = 0);

    // The socket connection is gone, and every temporary Agent Space it made
    // goes with it, unless the reader has taken one over.
    void connectionClosed(quint64 connection);

signals:
    void allowAgentsChanged();

private:
    struct Connection {
        QString currentTabId;
        // The current tab's Space while there is one, which is where it is
        // looked for first.
        QString currentSpaceId;
        quint64 lastUsed = 0;
    };

    void reload();
    void apply(bool allowed);
    Connection &connectionNamed(const QString &name);
    bool mayDrive(const TabState &tab) const;

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
    // connection's own, or the Space on show.
    QString defaultSpace(const Connection &connection) const;
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
    // The temporary Agent Spaces each socket connection made.
    QHash<quint64, QStringList> m_temporarySpaces;
};

} // namespace omaweb
