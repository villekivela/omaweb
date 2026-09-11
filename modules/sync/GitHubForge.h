#pragma once

#include "ForgeProvider.h"

#include <QNetworkAccessManager>
#include <QUrl>

namespace omaweb {

class GitHubForge final : public ForgeProvider {
public:
    explicit GitHubForge(QString clientId = {},
        QUrl webRoot = QUrl(QStringLiteral("https://github.com")),
        QUrl apiRoot = QUrl(QStringLiteral("https://api.github.com")));

    DeviceAuthorization beginAuthorization(QString *errorMessage = nullptr) override;
    ForgeAuthorization pollAuthorization(
        const QString &deviceCode, QString *errorMessage = nullptr) override;
    ForgeAuthorization refreshAuthorization(
        const QByteArray &refreshToken, QString *errorMessage = nullptr) override;
    ForgeRepository provisionPrivateRepository(const QByteArray &accessToken,
        const QString &preferredName, QString *errorMessage = nullptr) override;
    QByteArray fetchAvatar(const QUrl &avatarUrl, QString *errorMessage = nullptr) override;

private:
    struct Response {
        int status = 0;
        QByteArray body;
        QString error;
    };

    Response request(const QByteArray &method, const QUrl &url, const QByteArray &body = {},
        const QByteArray &accessToken = {}, const QByteArray &contentType = {},
        qsizetype maximumResponseBytes = -1);
    QUrl webUrl(const QString &path) const;
    QUrl apiUrl(const QString &path) const;

    QString m_clientId;
    QUrl m_webRoot;
    QUrl m_apiRoot;
    QString m_login;
    QNetworkAccessManager m_network;
};

} // namespace omaweb
