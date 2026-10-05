#include "AddressWatch.h"

#include <QNetworkReply>
#include <QNetworkRequest>

namespace omaweb {

AddressWatch::AddressWatch(const QUrl &url, int retryMs, QObject *parent)
    : QObject(parent)
    , m_url(url)
{
    m_network.setCookieJar(nullptr);
    m_retry.setSingleShot(true);
    m_retry.setInterval(retryMs);
    connect(&m_retry, &QTimer::timeout, this, &AddressWatch::ask);
    ask();
}

AddressWatch::~AddressWatch()
{
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
    }
}

QUrl AddressWatch::url() const { return m_url; }

void AddressWatch::ask()
{
    QNetworkRequest request(m_url);
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(
        QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setTransferTimeout(requestTimeoutMs);
    m_certificate = false;
    m_reply = m_network.head(request);
    // A certificate the engine would refuse still means a server is there, and
    // the page is the place to say what is wrong with it.
    connect(m_reply, &QNetworkReply::sslErrors, this, [this, reply = m_reply] {
        m_certificate = true;
        reply->abort();
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply] { heard(reply); });
}

void AddressWatch::heard(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid() || m_certificate) {
        emit answered();
        return;
    }
    m_retry.start();
}

} // namespace omaweb
