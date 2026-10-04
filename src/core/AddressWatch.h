#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace omaweb {

// Waits for an address to answer: something taking a connection at its host
// and port. It asks again until one does, and then says so once. Nothing is
// sent over the connection, so a dev server sees no request it did not get
// from the page.
class AddressWatch final : public QObject {
    Q_OBJECT

public:
    // How long one attempt may wait for a host that neither takes nor
    // refuses the connection.
    static constexpr int attemptTimeoutMs = 2000;

    AddressWatch(const QUrl &url, int retryMs, QObject *parent = nullptr);

    QUrl url() const;

signals:
    void answered();

private:
    void attempt();
    void retry();

    QUrl m_url;
    QTcpSocket m_socket;
    QTimer m_retry;
    QTimer m_timeout;
};

} // namespace omaweb
