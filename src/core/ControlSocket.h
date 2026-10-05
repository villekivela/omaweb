#pragma once

#include <QObject>
#include <QSet>
#include <QString>

class QLocalServer;
class QLocalSocket;

namespace omaweb {

class AgentControl;
class BrowserController;

// The Agent socket (ADR 0051): a Unix socket only this user can open, which
// carries one JSON request per line and answers each with one JSON line.
//
// The socket is the boundary. Anything running as the reader can open it, and
// nothing else can, so what a request may do is AgentControl's to decide from
// what its verb reaches.
//
// A page verb is answered when the page answers, so a connection's requests
// are taken one at a time: the next line is read once the one before it has
// its answer, and answers come back in the order they were asked.
class ControlSocket final : public QObject {
    Q_OBJECT

public:
    explicit ControlSocket(AgentControl *control, QObject *parent = nullptr);
    ~ControlSocket() override;

    // Where clients look for it: `agentSocketPath()`.
    static QString defaultPath();

    // Listens at `path` with mode 0600, in a directory only this user can
    // enter. A socket another browser is answering on is left alone and this
    // answers false; one left behind by a browser that has gone is replaced.
    bool listen(const QString &path);

private:
    void accept();
    // `connection` numbers the socket for as long as this browser runs.
    void read(QLocalSocket *socket, quint64 connection);
    void tooLong(QLocalSocket *socket);

    AgentControl *m_control;
    QLocalServer *m_server;
    // The connections waiting on a page's answer, which read nothing more
    // until it comes.
    QSet<QLocalSocket *> m_waiting;
    quint64 m_connections = 0;
};

// Opens the Agent socket at `path` and, once this browser is the one answering
// on it, deletes the temporary Agent Spaces a browser that crashed left
// behind. A browser that finds another answering deletes nothing, because
// those Spaces may be the running browser's. Answers whether this browser is
// answering.
bool openAgentSocket(ControlSocket &socket, BrowserController &browser, const QString &path);

} // namespace omaweb
