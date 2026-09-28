#pragma once

#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;

namespace omaweb {

class AgentControl;

// The Agent socket (ADR 0051): a Unix socket only this user can open, which
// carries one JSON request per line and answers each with one JSON line.
//
// The socket is the boundary. Anything running as the reader can open it, and
// nothing else can, so what a request may do is AgentControl's to decide from
// what its verb reaches.
class ControlSocket final : public QObject {
    Q_OBJECT

public:
    explicit ControlSocket(AgentControl *control, QObject *parent = nullptr);
    ~ControlSocket() override;

    // `$XDG_RUNTIME_DIR/omaweb/control.sock` on Linux, and the same name under
    // the per-user temporary directory on macOS, which has no runtime
    // directory. `OMAWEB_CONTROL_SOCKET` names another, so a scratch browser
    // can run beside the everyday one.
    static QString defaultPath();

    // Listens at `path` with mode 0600, in a directory only this user can
    // enter. A socket another browser is answering on is left alone and this
    // answers false; one left behind by a browser that has gone is replaced.
    bool listen(const QString &path);

private:
    void accept();
    void read(QLocalSocket *socket);

    AgentControl *m_control;
    QLocalServer *m_server;
};

} // namespace omaweb
