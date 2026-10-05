#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class QNetworkReply;

namespace omaweb {

// Waits for an address to answer: its server sending back any HTTP response,
// whatever the status, or a TLS handshake of its own. It asks again until one
// does, and then says so once. A port forward that takes the connection while
// nothing behind it is up has not answered, so a connection alone is not
// enough. Each ask is one HEAD request, with no cookies and no redirects
// followed.
class AddressWatch final : public QObject {
    Q_OBJECT

public:
    // How long one request may wait for a host that neither answers nor
    // refuses.
    static constexpr int requestTimeoutMs = 2000;

    AddressWatch(const QUrl &url, int retryMs, QObject *parent = nullptr);
    ~AddressWatch() override;

    QUrl url() const;

signals:
    void answered();

private:
    void ask();
    void heard(QNetworkReply *reply);

    QUrl m_url;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    // The request under way met a certificate, which only a server sends.
    bool m_certificate = false;
    QTimer m_retry;
};

} // namespace omaweb
