#include "SyncAccount.h"

#include "GitHubForge.h"
#include "LinuxSecretStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QUuid>
#include <QtConcurrentRun>

#include <sodium.h>

#include <algorithm>
#include <utility>

namespace omaweb {

Q_DECLARE_LOGGING_CATEGORY(syncLog)

namespace {

    GitHubForge forge()
    {
        return GitHubForge(QString::fromLatin1(OMAWEB_GITHUB_APP_CLIENT_ID),
            QString::fromLatin1(OMAWEB_GITHUB_APP_SLUG));
    }

    void wipe(QString &text)
    {
        std::fill(text.begin(), text.end(), QChar {});
        text.clear();
    }

    void wipe(QByteArray &bytes)
    {
        sodium_memzero(bytes.data(), static_cast<size_t>(bytes.size()));
    }

} // namespace

SyncAccount::SyncAccount(QString dataRoot, QString configRoot, QObject *parent)
    : QObject(parent)
    , m_dataRoot(std::move(dataRoot))
    , m_configRoot(std::move(configRoot))
{
    connect(&m_authorizationPoll, &QTimer::timeout, this, &SyncAccount::pollAuthorization);
    connect(&m_authorizationStartWatcher, &QFutureWatcherBase::finished, this, [this] {
        const auto result = m_authorizationStartWatcher.result();
        const auto &prompt = result.first;
        if (prompt.deviceCode.isEmpty()) {
            m_connecting = false;
            qCWarning(syncLog) << "GitHub authorization could not start:" << result.second;
            announce(QStringLiteral("GitHub connection failed"));
            emit connectionFailed(result.second);
            return;
        }
        m_deviceCode = prompt.deviceCode;
        m_userCode = prompt.userCode;
        m_verificationUrl = prompt.verificationUrl;
        m_authorizationPoll.setInterval(qMax(1, prompt.pollIntervalSeconds) * 1'000);
        m_authorizationPoll.start();
        qCInfo(syncLog) << "GitHub authorization page requested";
        announce(QStringLiteral("Waiting for GitHub authorization"));
        emit consentPageRequested(m_verificationUrl);
    });
    connect(&m_authorizationFinishWatcher, &QFutureWatcherBase::finished, this, [this] {
        auto result = m_authorizationFinishWatcher.result();
        auto &connected = result.first;
        if (!connected.ready && result.second.isEmpty()) {
            if (connected.repositoryCreationRequired) {
                clearPendingAuthorization();
                m_pendingAuthorization = std::move(connected.authorization);
                m_pendingRepository = std::move(connected.repository);
                if (!m_awaitingRepositoryCreation) {
                    m_awaitingRepositoryCreation = true;
                    m_authorizationPoll.stop();
                    qCInfo(syncLog) << "GitHub repository creation page requested";
                    announce(QStringLiteral("Create the private Sync repository on GitHub"));
                    emit consentPageRequested(connected.repositoryCreationUrl);
                }
                return;
            }
            if (connected.installationRequired) {
                clearPendingAuthorization();
                m_pendingAuthorization = std::move(connected.authorization);
                m_pendingRepository = std::move(connected.repository);
                if (!m_awaitingInstallation) {
                    m_awaitingInstallation = true;
                    qCInfo(syncLog) << "GitHub App installation page requested";
                    announce(QStringLiteral("Waiting for GitHub App installation"));
                    emit consentPageRequested(connected.installationUrl);
                }
                return;
            }
            if (connected.pollIntervalAdjustmentSeconds > 0) {
                m_authorizationPoll.setInterval(m_authorizationPoll.interval()
                    + connected.pollIntervalAdjustmentSeconds * 1'000);
            }
            return;
        }
        m_authorizationPoll.stop();
        m_connecting = false;
        m_awaitingRepositoryCreation = false;
        m_awaitingInstallation = false;
        clearPendingAuthorization();
        wipe(m_pendingRecoveryKey);
        if (!connected.ready) {
            qCWarning(syncLog) << "GitHub connection failed:" << result.second;
            announce(QStringLiteral("GitHub connection failed"));
            emit connectionFailed(result.second);
            return;
        }
        m_login = connected.login;
        m_remoteUrl = connected.remoteUrl;
        m_recoveryKey = connected.recoveryKey;
        m_accessToken = std::move(connected.accessToken);
        m_accessTokenExpiresAt
            = QDateTime::currentDateTimeUtc().addSecs(connected.accessTokenExpiresInSeconds);
        m_adoptsRemote = !connected.repositoryCreated;
        qCInfo(syncLog) << "GitHub Sync connection ready for" << m_login;
        emit changed();
        emit connectionReady();
    });
    loadMarker();
}

SyncAccount::~SyncAccount()
{
    wipe(m_accessToken);
    wipe(m_recoveryKey);
    wipe(m_pendingRecoveryKey);
    clearPendingAuthorization();
}

QString SyncAccount::provider() const { return QStringLiteral("GitHub"); }
bool SyncAccount::connected() const { return !m_login.isEmpty() && m_remoteUrl.isValid(); }
bool SyncAccount::connecting() const { return m_connecting; }
bool SyncAccount::awaitingRepositoryCreation() const { return m_awaitingRepositoryCreation; }
bool SyncAccount::awaitingInstallation() const { return m_awaitingInstallation; }
QString SyncAccount::login() const { return m_login; }
QUrl SyncAccount::remoteUrl() const { return m_remoteUrl; }
QString SyncAccount::userCode() const { return m_userCode; }
QUrl SyncAccount::verificationUrl() const { return m_verificationUrl; }
QString SyncAccount::recoveryKey() const { return m_recoveryKey; }
bool SyncAccount::wasEnabled() const { return m_enabled; }
QDateTime SyncAccount::lastSuccessfulSync() const { return m_lastSuccessfulSync; }

QString SyncAccount::avatarPath() const
{
    const auto path = QDir(m_dataRoot).filePath(QStringLiteral("sync/avatar"));
    return QFileInfo::exists(path) ? path : QString {};
}

SyncIntent SyncAccount::intent() const
{
    return m_adoptsRemote ? SyncIntent::AdoptRemote : SyncIntent::Reconcile;
}

void SyncAccount::markRemoteAdopted() { m_adoptsRemote = false; }

void SyncAccount::announce(const QString &status)
{
    emit statusChanged(status);
    emit changed();
}

void SyncAccount::loadMarker()
{
    QFile file(QDir(m_configRoot).filePath(QStringLiteral("sync.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto marker = QJsonDocument::fromJson(file.readAll()).object();
    if (marker.value(QStringLiteral("provider")).toString() != QLatin1String("github")) {
        return;
    }
    m_enabled = marker.value(QStringLiteral("enabled")).toBool();
    m_login = marker.value(QStringLiteral("login")).toString();
    m_remoteUrl = QUrl(marker.value(QStringLiteral("remoteUrl")).toString());
    m_adoptsRemote = marker.value(QStringLiteral("initialRemoteRestore")).toBool();
    m_lastSuccessfulSync = QDateTime::fromString(
        marker.value(QStringLiteral("lastSuccessfulSync")).toString(), Qt::ISODateWithMs);
}

bool SyncAccount::remember(bool enabled, const QDateTime &lastSuccessfulSync)
{
    m_enabled = enabled;
    m_lastSuccessfulSync = lastSuccessfulSync;
    const auto path = QDir(m_configRoot).filePath(QStringLiteral("sync.json"));
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(
        QJsonObject {{QStringLiteral("version"), 1}, {QStringLiteral("enabled"), m_enabled},
            {QStringLiteral("provider"), QStringLiteral("github")},
            {QStringLiteral("login"), m_login},
            {QStringLiteral("remoteUrl"), m_remoteUrl.toString()},
            {QStringLiteral("initialRemoteRestore"), m_adoptsRemote},
            {QStringLiteral("lastSuccessfulSync"),
                m_lastSuccessfulSync.toString(Qt::ISODateWithMs)}})
            .toJson(QJsonDocument::Indented));
    return file.commit();
}

QString SyncAccount::machineId()
{
    if (!m_machineId.isEmpty()) {
        return m_machineId;
    }
    const auto path = QDir(m_dataRoot).filePath(QStringLiteral("sync/machine-id"));
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly)) {
        m_machineId = QString::fromLatin1(existing.readAll()).trimmed();
    }
    if (!m_machineId.isEmpty()) {
        return m_machineId;
    }
    m_machineId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(m_machineId.toLatin1());
        file.commit();
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    return m_machineId;
}

bool SyncAccount::beginConnection(const QString &recoveryKey)
{
    if (m_connecting) {
        return false;
    }
    m_awaitingRepositoryCreation = false;
    m_awaitingInstallation = false;
    m_pendingRecoveryKey = recoveryKey;
    m_connecting = true;
    qCInfo(syncLog) << "GitHub Sync connection started";
    announce(QStringLiteral("Contacting GitHub"));
    m_authorizationStartWatcher.setFuture(QtConcurrent::run([recoveryKey] {
        QString error;
        auto provider = forge();
        LinuxSecretStore secrets;
        SyncSetup setup(provider, secrets, {});
        const auto prompt = setup.beginConnect(recoveryKey, &error);
        return QPair {prompt, error};
    }));
    return true;
}

bool SyncAccount::continueConnection()
{
    if (!m_connecting || !m_awaitingRepositoryCreation
        || m_pendingAuthorization.state != AuthorizationState::Complete) {
        return false;
    }
    m_awaitingRepositoryCreation = false;
    m_awaitingInstallation = true;
    m_pendingRepository = {.name = QStringLiteral("omaweb-sync")};
    auto provider = forge();
    qCInfo(syncLog) << "GitHub App installation page requested";
    announce(QStringLiteral("Waiting for GitHub App installation"));
    emit consentPageRequested(
        provider.installationUrl(m_pendingAuthorization.accountId, m_pendingRepository.id));
    m_authorizationPoll.start();
    return true;
}

void SyncAccount::pollAuthorization()
{
    if (m_authorizationFinishWatcher.isRunning()) {
        return;
    }
    const auto deviceCode = m_deviceCode;
    const auto recoveryKey = m_pendingRecoveryKey;
    const auto dataRoot = m_dataRoot;
    auto authorization = std::move(m_pendingAuthorization);
    auto repository = std::move(m_pendingRepository);
    m_authorizationFinishWatcher.setFuture(QtConcurrent::run(
        [deviceCode, recoveryKey, dataRoot, authorization = std::move(authorization),
            repository = std::move(repository)]() mutable {
            QString error;
            auto provider = forge();
            LinuxSecretStore secrets;
            SyncSetup setup(provider, secrets, dataRoot);
            if (authorization.state == AuthorizationState::Complete) {
                setup.resumeConnect(std::move(authorization), std::move(repository), recoveryKey);
            } else {
                setup.resumeConnect(deviceCode, recoveryKey);
            }
            auto connection = setup.finishConnect(&error);
            return QPair {std::move(connection), error};
        }));
}

void SyncAccount::clearPendingAuthorization()
{
    wipe(m_pendingAuthorization.accessToken);
    wipe(m_pendingAuthorization.refreshToken);
    m_pendingAuthorization = {};
    m_pendingRepository = {};
}

void SyncAccount::forget()
{
    LinuxSecretStore secrets;
    secrets.remove(QStringLiteral("forge-refresh/%1").arg(m_login), nullptr);
    secrets.remove(QStringLiteral("sync-key/%1").arg(m_login), nullptr);
    wipe(m_accessToken);
    m_accessTokenExpiresAt = {};
    QFile::remove(QDir(m_dataRoot).filePath(QStringLiteral("sync/avatar")));
    QDir(QDir(m_dataRoot).filePath(QStringLiteral("sync/repository"))).removeRecursively();
    QFile::remove(QDir(m_configRoot).filePath(QStringLiteral("sync.json")));
    m_login.clear();
    m_remoteUrl.clear();
    m_enabled = false;
    m_adoptsRemote = false;
    m_lastSuccessfulSync = {};
    clearRecoveryKey();
}

void SyncAccount::clearRecoveryKey()
{
    wipe(m_recoveryKey);
    emit changed();
}

bool SyncAccount::saveRecoveryKey(const QUrl &destination) const
{
    if (m_recoveryKey.isEmpty() || !destination.isLocalFile()) {
        return false;
    }
    QSaveFile file(destination.toLocalFile());
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write((m_recoveryKey + QLatin1Char('\n')).toUtf8()) < 0 || !file.commit()) {
        return false;
    }
    return true;
}

QByteArray SyncAccount::currentAccessToken() const
{
    const auto current = !m_accessToken.isEmpty()
        && m_accessTokenExpiresAt > QDateTime::currentDateTimeUtc().addSecs(60);
    return current ? m_accessToken : QByteArray {};
}

void SyncAccount::cacheAccessToken(QByteArray token, int expiresInSeconds)
{
    if (token.isEmpty()) {
        return;
    }
    wipe(m_accessToken);
    m_accessToken = std::move(token);
    m_accessTokenExpiresAt = QDateTime::currentDateTimeUtc().addSecs(expiresInSeconds);
}

SyncAccount::Session SyncAccount::openSession(const QString &login, QByteArray accessToken)
{
    const auto failed = [](SyncFailure failure, const QString &message) {
        return Session {.credentials = {},
            .refreshedAccessToken = {},
            .refreshedAccessTokenExpiresInSeconds = 0,
            .error = {.failure = failure, .message = message}};
    };
    QString error;
    LinuxSecretStore secrets;
    Session session;
    session.credentials
        = {.recoveryKey = secrets.retrieve(QStringLiteral("sync-key/%1").arg(login), &error),
            .accessToken = std::move(accessToken)};
    if (session.credentials.recoveryKey.size() != 32) {
        return failed(
            SyncFailure::RecoveryKeyRejected, QStringLiteral("Enter the Sync recovery key"));
    }
    if (!session.credentials.accessToken.isEmpty()) {
        return session;
    }
    auto refresh = secrets.retrieve(QStringLiteral("forge-refresh/%1").arg(login), &error);
    auto provider = forge();
    auto authorization = provider.refreshAuthorization(refresh, &error);
    wipe(refresh);
    if (authorization.state != AuthorizationState::Complete) {
        return failed(SyncFailure::AuthorizationExpired,
            error.isEmpty() ? QStringLiteral("GitHub authorization expired; connect again")
                            : error);
    }
    session.credentials.accessToken = std::move(authorization.accessToken);
    session.refreshedAccessToken = session.credentials.accessToken;
    session.refreshedAccessTokenExpiresInSeconds = authorization.expiresInSeconds;
    if (!authorization.refreshToken.isEmpty()) {
        if (!secrets.store(QStringLiteral("forge-refresh/%1").arg(login),
                authorization.refreshToken, &error)) {
            return failed(SyncFailure::Failed, error);
        }
        wipe(authorization.refreshToken);
    }
    return session;
}

} // namespace omaweb
