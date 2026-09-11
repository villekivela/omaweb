#include "SyncSetup.h"

#include "SecretStore.h"
#include "SyncModule.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <sodium.h>

#include <utility>

namespace omaweb {

namespace {

    void setError(QString *destination, const QString &message)
    {
        if (destination) {
            *destination = message;
        }
    }

} // namespace

SyncSetup::SyncSetup(ForgeProvider &forge, SecretStore &secrets, QString dataRoot)
    : m_forge(forge)
    , m_secrets(secrets)
    , m_dataRoot(std::move(dataRoot))
{
}

DeviceAuthorization SyncSetup::beginConnect(QString recoveryKey, QString *errorMessage)
{
    m_recoveryKey = std::move(recoveryKey);
    auto authorization = m_forge.beginAuthorization(errorMessage);
    m_deviceCode = authorization.deviceCode;
    return authorization;
}

void SyncSetup::resumeConnect(QString deviceCode, QString recoveryKey)
{
    m_deviceCode = std::move(deviceCode);
    m_recoveryKey = std::move(recoveryKey);
}

SyncConnection SyncSetup::finishConnect(QString *errorMessage)
{
    if (m_deviceCode.isEmpty()) {
        setError(errorMessage, QStringLiteral("GitHub connection has not been started"));
        return {};
    }
    auto authorization = m_forge.pollAuthorization(m_deviceCode, errorMessage);
    if (authorization.state == AuthorizationState::Pending) {
        SyncConnection pending;
        pending.pollIntervalAdjustmentSeconds = authorization.pollIntervalAdjustmentSeconds;
        return pending;
    }
    m_deviceCode.clear();
    if (authorization.state != AuthorizationState::Complete || authorization.login.isEmpty()
        || authorization.accessToken.isEmpty() || authorization.refreshToken.isEmpty()) {
        setError(errorMessage, QStringLiteral("GitHub did not authorize Sync"));
        return {};
    }

    auto repository = m_forge.provisionPrivateRepository(
        authorization.accessToken, QStringLiteral("omaweb-sync"), errorMessage);
    if (repository.name.isEmpty() || !repository.cloneUrl.isValid() || !repository.isPrivate) {
        sodium_memzero(authorization.accessToken.data(),
            static_cast<size_t>(authorization.accessToken.size()));
        sodium_memzero(authorization.refreshToken.data(),
            static_cast<size_t>(authorization.refreshToken.size()));
        setError(errorMessage, QStringLiteral("The Sync repository was not created privately"));
        return {};
    }

    if (!repository.created && m_recoveryKey.isEmpty()) {
        sodium_memzero(authorization.accessToken.data(),
            static_cast<size_t>(authorization.accessToken.size()));
        sodium_memzero(authorization.refreshToken.data(),
            static_cast<size_t>(authorization.refreshToken.size()));
        setError(errorMessage,
            QStringLiteral("Enter the recovery key for the existing Sync repository"));
        return {};
    }
    const auto displayedKey
        = m_recoveryKey.isEmpty() ? SyncModule::createRecoveryKey() : m_recoveryKey;
    auto key = SyncModule::decodeRecoveryKey(displayedKey, errorMessage);
    const auto refreshName = QStringLiteral("forge-refresh/%1").arg(authorization.login);
    const auto keyName = QStringLiteral("sync-key/%1").arg(authorization.login);
    if (key.isEmpty() || !m_secrets.store(refreshName, authorization.refreshToken, errorMessage)
        || !m_secrets.store(keyName, key, errorMessage)) {
        sodium_memzero(key.data(), static_cast<size_t>(key.size()));
        sodium_memzero(authorization.accessToken.data(),
            static_cast<size_t>(authorization.accessToken.size()));
        sodium_memzero(authorization.refreshToken.data(),
            static_cast<size_t>(authorization.refreshToken.size()));
        return {};
    }

    const auto avatar = m_forge.fetchAvatar(authorization.avatarUrl, nullptr);
    if (!avatar.isEmpty()) {
        const auto path = QDir(m_dataRoot).filePath(QStringLiteral("sync/avatar"));
        if (QDir().mkpath(QFileInfo(path).absolutePath())) {
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly) && file.write(avatar) == avatar.size()
                && file.commit()) {
                QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            }
        }
    }

    const auto login = authorization.login;
    sodium_memzero(key.data(), static_cast<size_t>(key.size()));
    sodium_memzero(
        authorization.refreshToken.data(), static_cast<size_t>(authorization.refreshToken.size()));
    if (errorMessage) {
        errorMessage->clear();
    }
    return {.ready = true,
        .login = login,
        .repositoryName = repository.name,
        .remoteUrl = repository.cloneUrl,
        .recoveryKey = displayedKey,
        .accessToken = std::move(authorization.accessToken),
        .accessTokenExpiresInSeconds = authorization.expiresInSeconds,
        .repositoryCreated = repository.created};
}

} // namespace omaweb
