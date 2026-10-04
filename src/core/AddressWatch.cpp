#include "AddressWatch.h"

namespace omaweb {

AddressWatch::AddressWatch(const QUrl &url, int retryMs, QObject *parent)
    : QObject(parent)
    , m_url(url)
{
    m_retry.setSingleShot(true);
    m_retry.setInterval(retryMs);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(attemptTimeoutMs);
    connect(&m_retry, &QTimer::timeout, this, &AddressWatch::attempt);
    connect(&m_timeout, &QTimer::timeout, this, &AddressWatch::retry);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &AddressWatch::retry);
    connect(&m_socket, &QTcpSocket::connected, this, [this] {
        m_timeout.stop();
        m_socket.abort();
        emit answered();
    });
    attempt();
}

QUrl AddressWatch::url() const { return m_url; }

void AddressWatch::attempt()
{
    m_socket.abort();
    m_timeout.start();
    m_socket.connectToHost(
        m_url.host(), static_cast<quint16>(m_url.port(m_url.scheme() == u"https" ? 443 : 80)));
}

void AddressWatch::retry()
{
    m_timeout.stop();
    m_socket.abort();
    if (!m_retry.isActive()) {
        m_retry.start();
    }
}

} // namespace omaweb
