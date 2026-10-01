#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace omaweb::test {

// A search engine's suggest endpoint on loopback, so a test sees every request
// Omaweb makes and answers it without the network. Each request is kept with
// its target and headers; the answer is `body`, or nothing at all while
// `holding` is set, which a test uses to have a request outlive the reader's
// next keystroke or run past the timeout.
class SuggestServer final : public QObject {
public:
    struct Request {
        QByteArray target;
        QHash<QByteArray, QByteArray> headers;
    };

    explicit SuggestServer(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { read(socket); });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        m_server.listen(QHostAddress::LocalHost);
    }

    // A suggest URL on this server, with the `{query}` placeholder.
    QString suggestUrl(const QString &path = QStringLiteral("/suggest")) const
    {
        return QStringLiteral("http://127.0.0.1:%1%2?q={query}")
            .arg(m_server.serverPort())
            .arg(path);
    }

    const QList<Request> &requests() const { return m_requests; }

    QByteArray body = R"(["", []])";
    QByteArray status = "200 OK";
    QByteArray extraHeaders;
    bool holding = false;
    // Sends the answer a byte at a time, this far apart, rather than at once.
    int trickleMilliseconds = 0;

    // Answers every request held so far with `answer`.
    void release(const QByteArray &answer)
    {
        const auto held = m_held;
        m_held.clear();
        for (const auto &socket : held) {
            if (socket) {
                respond(socket, answer);
            }
        }
    }

private:
    void read(QTcpSocket *socket)
    {
        auto &buffer = m_buffers[socket];
        buffer.append(socket->readAll());
        const auto end = buffer.indexOf("\r\n\r\n");
        if (end < 0) {
            return;
        }
        const auto lines = buffer.left(end).split('\n');
        m_buffers.remove(socket);
        Request request;
        const auto requestLine = lines.first().trimmed().split(' ');
        request.target = requestLine.value(1);
        for (qsizetype index = 1; index < lines.size(); ++index) {
            const auto line = lines.at(index).trimmed();
            const auto colon = line.indexOf(':');
            if (colon > 0) {
                request.headers.insert(line.left(colon).toLower(), line.mid(colon + 1).trimmed());
            }
        }
        m_requests.append(request);
        if (holding) {
            m_held.append(socket);
            return;
        }
        respond(socket, body);
    }

    void respond(QTcpSocket *socket, const QByteArray &answer)
    {
        socket->write(
            "HTTP/1.1 " + status + "\r\nContent-Type: application/json; charset=utf-8\r\n");
        socket->write(extraHeaders);
        socket->write("Content-Length: " + QByteArray::number(answer.size())
            + "\r\nConnection: close\r\n\r\n");
        if (trickleMilliseconds > 0) {
            trickle(socket, answer);
            return;
        }
        socket->write(answer);
        socket->disconnectFromHost();
    }

    void trickle(QPointer<QTcpSocket> socket, QByteArray rest)
    {
        if (!socket) {
            return;
        }
        if (rest.isEmpty()) {
            socket->disconnectFromHost();
            return;
        }
        socket->write(rest.left(1));
        QTimer::singleShot(trickleMilliseconds, this,
            [this, socket, rest = rest.mid(1)] { trickle(socket, rest); });
    }

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<QPointer<QTcpSocket>> m_held;
    QList<Request> m_requests;
};

} // namespace omaweb::test
