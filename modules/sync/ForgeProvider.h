#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace omaweb {

struct DeviceAuthorization {
    QString deviceCode {};
    QString userCode {};
    QUrl verificationUrl {};
    int expiresInSeconds = 0;
    int pollIntervalSeconds = 0;
};

enum class AuthorizationState { Pending, Complete, Expired, Failed };

enum class InstallationState { Pending, Complete, Failed };

struct ForgeAuthorization {
    AuthorizationState state = AuthorizationState::Failed;
    QByteArray accessToken {};
    QByteArray refreshToken {};
    QString login {};
    QUrl avatarUrl {};
    int expiresInSeconds = 0;
    int pollIntervalAdjustmentSeconds = 0;
};

struct ForgeRepository {
    QString name {};
    QUrl cloneUrl {};
    bool isPrivate = false;
    bool created = false;
};

class ForgeProvider {
public:
    virtual ~ForgeProvider() = default;

    ForgeProvider(const ForgeProvider &) = delete;
    ForgeProvider &operator=(const ForgeProvider &) = delete;

    virtual DeviceAuthorization beginAuthorization(QString *errorMessage = nullptr) = 0;
    virtual ForgeAuthorization pollAuthorization(
        const QString &deviceCode, QString *errorMessage = nullptr) = 0;
    virtual ForgeAuthorization refreshAuthorization(
        const QByteArray &refreshToken, QString *errorMessage = nullptr) = 0;
    virtual InstallationState installationState(
        const QByteArray &accessToken, const QString &login, QString *errorMessage = nullptr) = 0;
    virtual ForgeRepository provisionPrivateRepository(const QByteArray &accessToken,
        const QString &owner, const QString &preferredName, QString *errorMessage = nullptr) = 0;
    virtual QByteArray fetchAvatar(const QUrl &avatarUrl, QString *errorMessage = nullptr) = 0;

protected:
    ForgeProvider() = default;
};

} // namespace omaweb
