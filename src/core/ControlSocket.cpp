#include "ControlSocket.h"

#include "AgentControl.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

#include <sys/stat.h>

namespace omaweb {
namespace {

    // A request is a verb and a few arguments. Anything longer is not one, and
    // holding it would let a connection grow the browser's memory at will.
    constexpr qint64 maximumRequestBytes = 64 * 1024;

    // Long enough for a browser busy with a page to answer, short enough that
    // a socket nobody answers on is taken over at start without a wait.
    constexpr int probeTimeoutMs = 500;

    void reply(QLocalSocket *socket, const QJsonObject &answer)
    {
        socket->write(QJsonDocument(answer).toJson(QJsonDocument::Compact) + '\n');
    }

    QJsonObject badRequest(const QString &error)
    {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("code"), QStringLiteral("bad-request")},
            {QStringLiteral("error"), error},
        };
    }

} // namespace

ControlSocket::ControlSocket(AgentControl *control, QObject *parent)
    : QObject(parent)
    , m_control(control)
    , m_server(new QLocalServer(this))
{
    connect(m_server, &QLocalServer::newConnection, this, &ControlSocket::accept);
}

ControlSocket::~ControlSocket() = default;

QString ControlSocket::defaultPath()
{
    const auto override = qEnvironmentVariable("OMAWEB_CONTROL_SOCKET");
    if (!override.isEmpty()) {
        return override;
    }
#if defined(Q_OS_MACOS)
    const auto base = QDir::tempPath();
#else
    // Qt reads XDG_RUNTIME_DIR, and falls back to a directory of its own that
    // it has checked belongs to this user when the session did not set one.
    const auto base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#endif
    return QDir(base).filePath(QStringLiteral("omaweb/control.sock"));
}

bool ControlSocket::listen(const QString &path)
{
    const auto directory = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directory)) {
        qWarning("The Agent socket's directory %s could not be made", qPrintable(directory));
        return false;
    }
    // Only this user may enter Omaweb's own directory. A path the environment
    // named may sit in a directory that is not Omaweb's to change, and the
    // socket's own mode is what holds there: Qt binds it in a private directory
    // and moves it into place already closed to everyone else.
    if (QFileInfo(directory).fileName() == u"omaweb"
        && ::chmod(QFile::encodeName(directory).constData(), S_IRWXU) != 0) {
        qWarning(
            "The Agent socket's directory %s could not be made private", qPrintable(directory));
        return false;
    }

    if (QFileInfo::exists(path)) {
        QLocalSocket probe;
        probe.connectToServer(path);
        if (probe.waitForConnected(probeTimeoutMs)) {
            qWarning("Another Omaweb is answering on %s, so this one opens no Agent socket",
                qPrintable(path));
            return false;
        }
        QLocalServer::removeServer(path);
    }

    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(path)) {
        qWarning("The Agent socket %s could not be opened: %s", qPrintable(path),
            qPrintable(m_server->errorString()));
        return false;
    }
    // Qt grants the user read, write and execute. A socket has no use for the
    // last, and 0600 is the mode ADR 0051 states.
    ::chmod(QFile::encodeName(path).constData(), S_IRUSR | S_IWUSR);
    return true;
}

void ControlSocket::accept()
{
    while (auto *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { read(socket); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void ControlSocket::read(QLocalSocket *socket)
{
    while (socket->canReadLine()) {
        const auto line = socket->readLine(maximumRequestBytes).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QJsonParseError error {};
        const auto document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            reply(socket, badRequest(QStringLiteral("A request is one JSON object per line.")));
            continue;
        }
        reply(socket, m_control->answer(document.object()));
    }
    if (socket->bytesAvailable() > maximumRequestBytes) {
        reply(socket, badRequest(QStringLiteral("The request is too long.")));
        socket->disconnectFromServer();
    }
}

} // namespace omaweb
