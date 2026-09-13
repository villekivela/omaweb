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

SyncSetup::~SyncSetup()
{
    sodium_memzero(m_authorization.accessToken.data(),
        static_cast<size_t>(m_authorization.accessToken.size()));
    sodium_memzero(m_authorization.refreshToken.data(),
        static_cast<size_t>(m_authorization.refreshToken.size()));
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

void SyncSetup::resumeConnect(
    ForgeAuthorization authorization, ForgeRepository repository, QString recoveryKey)
{
    m_authorization = std::move(authorization);
    m_repository = std::move(repository);
    m_recoveryKey = std::move(recoveryKey);
}

SyncConnection SyncSetup::finishConnect(QString *errorMessage)
{
    if (m_deviceCode.isEmpty() && m_authorization.state != AuthorizationState::Complete) {
        setError(errorMessage, m_forge.vocabulary().connectionNotStarted);
        return {};
    }
    if (m_authorization.state != AuthorizationState::Complete) {
        m_authorization = m_forge.pollAuthorization(m_deviceCode, errorMessage);
        if (m_authorization.state == AuthorizationState::Pending) {
            SyncConnection pending;
            pending.pollIntervalAdjustmentSeconds = m_authorization.pollIntervalAdjustmentSeconds;
            return pending;
        }
        m_deviceCode.clear();
    }
    if (m_authorization.state != AuthorizationState::Complete || m_authorization.login.isEmpty()
        || m_authorization.accountId <= 0 || m_authorization.accessToken.isEmpty()
        || m_authorization.refreshToken.isEmpty()) {
        setError(errorMessage, m_forge.vocabulary().authorizationRefused);
        return {};
    }

    auto installation = m_forge.installationState(
        m_authorization.accessToken, m_authorization.login, m_repository.id, errorMessage);
    if (installation == InstallationState::Failed) {
        return {};
    }

    if (installation == InstallationState::Pending) {
        if (errorMessage) {
            errorMessage->clear();
        }
        SyncConnection pending;
        if (m_repository.name.isEmpty()) {
            pending.repositoryCreationRequired = true;
            pending.repositoryCreationUrl = m_forge.repositoryCreationUrl(
                m_authorization.login, m_forge.vocabulary().repositoryName);
        } else {
            pending.installationRequired = true;
            pending.installationUrl
                = m_forge.installationUrl(m_authorization.accountId, m_repository.id);
        }
        pending.authorization = std::move(m_authorization);
        pending.repository = std::move(m_repository);
        return pending;
    }

    if (!m_repository.cloneUrl.isValid()) {
        const auto preferredName
            = m_repository.name.isEmpty() ? m_forge.vocabulary().repositoryName : m_repository.name;
        m_repository = m_forge.provisionPrivateRepository(
            m_authorization.accessToken, m_authorization.login, preferredName, errorMessage);
        if (m_repository.id <= 0 || m_repository.name.isEmpty() || !m_repository.cloneUrl.isValid()
            || !m_repository.isPrivate) {
            sodium_memzero(m_authorization.accessToken.data(),
                static_cast<size_t>(m_authorization.accessToken.size()));
            sodium_memzero(m_authorization.refreshToken.data(),
                static_cast<size_t>(m_authorization.refreshToken.size()));
            if (!errorMessage || errorMessage->isEmpty()) {
                setError(
                    errorMessage, QStringLiteral("The Sync repository was not created privately"));
            }
            return {};
        }
        installation = m_forge.installationState(
            m_authorization.accessToken, m_authorization.login, m_repository.id, errorMessage);
    }

    if (installation == InstallationState::Pending) {
        if (errorMessage) {
            errorMessage->clear();
        }
        SyncConnection pending;
        pending.installationRequired = true;
        pending.installationUrl
            = m_forge.installationUrl(m_authorization.accountId, m_repository.id);
        pending.authorization = std::move(m_authorization);
        pending.repository = std::move(m_repository);
        return pending;
    }
    if (installation != InstallationState::Complete) {
        return {};
    }

    if (!m_repository.created && m_recoveryKey.isEmpty()) {
        sodium_memzero(m_authorization.accessToken.data(),
            static_cast<size_t>(m_authorization.accessToken.size()));
        sodium_memzero(m_authorization.refreshToken.data(),
            static_cast<size_t>(m_authorization.refreshToken.size()));
        setError(errorMessage,
            QStringLiteral("Enter the recovery key for the existing Sync repository"));
        return {};
    }
    const auto displayedKey
        = m_recoveryKey.isEmpty() ? SyncModule::createRecoveryKey() : m_recoveryKey;
    auto key = SyncModule::decodeRecoveryKey(displayedKey, errorMessage);
    const auto refreshName = QStringLiteral("forge-refresh/%1").arg(m_authorization.login);
    const auto keyName = QStringLiteral("sync-key/%1").arg(m_authorization.login);
    if (key.isEmpty() || !m_secrets.store(refreshName, m_authorization.refreshToken, errorMessage)
        || !m_secrets.store(keyName, key, errorMessage)) {
        sodium_memzero(key.data(), static_cast<size_t>(key.size()));
        sodium_memzero(m_authorization.accessToken.data(),
            static_cast<size_t>(m_authorization.accessToken.size()));
        sodium_memzero(m_authorization.refreshToken.data(),
            static_cast<size_t>(m_authorization.refreshToken.size()));
        return {};
    }

    const auto avatar = m_forge.fetchAvatar(m_authorization.avatarUrl, nullptr);
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

    const auto login = m_authorization.login;
    sodium_memzero(key.data(), static_cast<size_t>(key.size()));
    sodium_memzero(m_authorization.refreshToken.data(),
        static_cast<size_t>(m_authorization.refreshToken.size()));
    if (errorMessage) {
        errorMessage->clear();
    }
    return {.ready = true,
        .login = login,
        .repositoryName = m_repository.name,
        .remoteUrl = m_repository.cloneUrl,
        .recoveryKey = displayedKey,
        .accessToken = std::move(m_authorization.accessToken),
        .accessTokenExpiresInSeconds = m_authorization.expiresInSeconds,
        .repositoryCreated = m_repository.created,
        .repositoryCreationUrl = {},
        .installationUrl = {},
        .authorization = {},
        .repository = {}};
}

} // namespace omaweb
