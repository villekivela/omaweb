#include "ControlSocket.h"

#include "AgentControl.h"
#include "BrowserController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>

#include <memory>

#include <sys/stat.h>

namespace omaweb {
namespace {

    // A request is a verb and a few arguments. Anything longer is not one, and
    // holding it would let a connection grow the browser's memory at will.
    constexpr qint64 maximumRequestBytes = 64 * 1024;

    // What a client may queue behind a request still being answered, a batch
    // that can take minutes: many requests, but not memory without end.
    constexpr qint64 maximumQueuedBytes = 4 * 1024 * 1024;

    // An answer is a few kilobytes. A client that lets this much wait unread
    // is not reading, and what it has not read is the browser's memory.
    constexpr qint64 maximumUnreadBytes = 4 * 1024 * 1024;

    // Long enough for a browser busy with a page to answer, short enough that
    // a socket nobody answers on is taken over at start without a wait.
    constexpr int probeTimeoutMs = 500;

    void reply(QLocalSocket *socket, const QJsonObject &answer)
    {
        socket->write(QJsonDocument(answer).toJson(QJsonDocument::Compact) + '\n');
        if (socket->bytesToWrite() > maximumUnreadBytes) {
            socket->abort();
        }
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

// A connection closing asks the control to delete what it made, and the
// control may already be going too, so the sockets stop reporting first.
ControlSocket::~ControlSocket()
{
    for (auto *socket : m_server->findChildren<QLocalSocket *>()) {
        socket->disconnect(this);
    }
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

    // Only a socket is ever taken away. Anything else at this path is not a
    // browser that has gone, and Qt would rename the socket over it.
    struct stat status {};
    if (::lstat(QFile::encodeName(path).constData(), &status) == 0) {
        if (!S_ISSOCK(status.st_mode)) {
            qWarning("%s is not a socket, so this Omaweb opens no Agent socket over it",
                qPrintable(path));
            return false;
        }
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
    if (::chmod(QFile::encodeName(path).constData(), S_IRUSR | S_IWUSR) != 0) {
        qWarning("The Agent socket %s keeps the mode Qt gave it, 0700", qPrintable(path));
    }
    return true;
}

void ControlSocket::accept()
{
    while (auto *socket = m_server->nextPendingConnection()) {
        const auto connection = ++m_connections;
        connect(socket, &QLocalSocket::readyRead, this,
            [this, socket, connection] { read(socket, connection); });
        connect(socket, &QLocalSocket::disconnected, this,
            [this, connection] { m_control->connectionClosed(connection); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QObject::destroyed, this, [this, socket] { m_waiting.remove(socket); });
    }
}

void ControlSocket::read(QLocalSocket *socket, quint64 connection)
{
    while (!m_waiting.contains(socket) && socket->state() == QLocalSocket::ConnectedState
        && socket->canReadLine()) {
        // One byte past the limit, so a line that fills it is told from one
        // that ends inside it. A line cut short is never read as a request,
        // and neither is what follows it.
        const auto line = socket->readLine(maximumRequestBytes + 1);
        if (!line.endsWith('\n')) {
            tooLong(socket);
            return;
        }
        const auto request = line.trimmed();
        if (request.isEmpty()) {
            continue;
        }
        QJsonParseError error {};
        const auto document = QJsonDocument::fromJson(request, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            reply(socket, badRequest(QStringLiteral("A request is one JSON object per line.")));
            continue;
        }
        // Answered now for a browser command, and later for a page verb, when
        // the connection may have gone.
        m_waiting.insert(socket);
        auto answered = std::make_shared<bool>(false);
        auto dispatching = std::make_shared<bool>(true);
        const QPointer<QLocalSocket> guarded(socket);
        m_control->handle(
            document.object(),
            [this, guarded, answered, dispatching, connection](const QJsonObject &answer) {
                if (*answered || !guarded) {
                    return;
                }
                *answered = true;
                m_waiting.remove(guarded.data());
                reply(guarded.data(), answer);
                // A reply that came later has lines behind it that nothing will
                // announce again.
                if (!*dispatching) {
                    QMetaObject::invokeMethod(
                        this,
                        [this, guarded, connection] {
                            if (guarded) {
                                read(guarded.data(), connection);
                            }
                        },
                        Qt::QueuedConnection);
                }
            },
            connection);
        *dispatching = false;
    }
    // With no line left to read, what is there is a line too long. Behind a
    // request still being answered it is requests queued.
    const auto limit = m_waiting.contains(socket) ? maximumQueuedBytes : maximumRequestBytes;
    if (socket->state() == QLocalSocket::ConnectedState && socket->bytesAvailable() > limit) {
        tooLong(socket);
    }
}

void ControlSocket::tooLong(QLocalSocket *socket)
{
    reply(socket, badRequest(QStringLiteral("The request is too long.")));
    socket->disconnectFromServer();
}

bool openAgentSocket(ControlSocket &socket, BrowserController &browser, const QString &path)
{
    if (!socket.listen(path)) {
        return false;
    }
    browser.deleteTemporarySpaces();
    return true;
}

} // namespace omaweb
