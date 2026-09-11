#pragma once

#include "ForgeProvider.h"

#include <QString>
#include <QUrl>

namespace omaweb {

class SecretStore;

struct SyncConnection {
    bool ready = false;
    QString login;
    QString repositoryName;
    QUrl remoteUrl;
    QString recoveryKey;
    QByteArray accessToken {};
    int accessTokenExpiresInSeconds = 0;
    int pollIntervalAdjustmentSeconds = 0;
    bool repositoryCreated = false;
};

class SyncSetup final {
public:
    SyncSetup(ForgeProvider &forge, SecretStore &secrets, QString dataRoot);

    DeviceAuthorization beginConnect(QString recoveryKey = {}, QString *errorMessage = nullptr);
    void resumeConnect(QString deviceCode, QString recoveryKey = {});
    SyncConnection finishConnect(QString *errorMessage = nullptr);

private:
    ForgeProvider &m_forge;
    SecretStore &m_secrets;
    QString m_dataRoot;
    QString m_deviceCode;
    QString m_recoveryKey;
};

} // namespace omaweb
