#pragma once

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
// markers and is not an identity: it grants nothing.
//
// Only the ordinary window's controller is handed to this. A Private window
// has no Spaces to list, and one handed over by mistake is refused whole.
class AgentControl final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool allowAgents READ allowAgents WRITE setAllowAgents NOTIFY allowAgentsChanged)

public:
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
    QJsonObject answer(const QJsonObject &request);

signals:
    void allowAgentsChanged();

private:
    struct Connection {
        QString currentTabId;
        QString currentSpaceId;
    };

    QJsonObject listSpaces() const;
    QJsonObject listTabs(Connection &connection, const QJsonObject &request) const;
    QJsonObject open(Connection &connection, const QJsonObject &request);
    QJsonObject close(Connection &connection, const QJsonObject &request);
    QJsonObject createSpace(Connection &connection, const QJsonObject &request);
    QJsonObject deleteSpace(Connection &connection, const QJsonObject &request);
    // A Space by id, or by a name no other Space shares.
    QString findSpace(const QString &idOrName) const;
    // The Space a request without `--space` is about: the current tab's, the
    // connection's own, or the Space on show.
    QString defaultSpace(const Connection &connection) const;
    QJsonObject describeTab(const QString &tabId, const Connection &connection) const;

    BrowserController *m_browser;
    QString m_configRoot;
    bool m_allowAgents = false;
    QHash<QString, Connection> m_connections;
    // Every tab an Agent opened in this run, so a reader's own tab is never
    // one an Agent can close.
    QSet<QString> m_openedTabIds;
};

} // namespace omaweb
