#include "SyncController.h"

#include "BrowserController.h"
#include "ContentBlocker.h"
#include "GitHubForge.h"
#include "LinuxSecretStore.h"
#include "KeyboardNavigation.h"
#include "SqliteSessionStore.h"
#include "SyncModule.h"
#include "SyncSetup.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonArray>
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

Q_LOGGING_CATEGORY(syncLog, "omaweb.sync")

namespace {

    QByteArray subscriptionDigest(ContentBlocker *blocker)
    {
        QJsonArray subscriptions;
        if (blocker) {
            for (const auto &value : blocker->subscriptions()) {
                const auto local = value.toMap();
                QJsonObject subscription;
                for (const auto &name : {QStringLiteral("id"), QStringLiteral("title"),
                         QStringLiteral("source"), QStringLiteral("license"),
                         QStringLiteral("updateAddress"), QStringLiteral("enabled")}) {
                    if (local.contains(name)) {
                        subscription.insert(name, QJsonValue::fromVariant(local.value(name)));
                    }
                }
                subscriptions.append(subscription);
            }
        }
        return QCryptographicHash::hash(QJsonDocument(subscriptions).toJson(QJsonDocument::Compact),
            QCryptographicHash::Sha256);
    }

} // namespace

SyncController::SyncController(BrowserController *browser, ContentBlocker *blocker,
    KeyboardNavigation *keyboardNavigation, QString dataRoot, QString configRoot, QObject *parent)
    : QObject(parent)
    , m_browser(browser)
    , m_blocker(blocker)
    , m_keyboardNavigation(keyboardNavigation)
    , m_dataRoot(std::move(dataRoot))
    , m_configRoot(std::move(configRoot))
{
    connect(&m_authorizationPoll, &QTimer::timeout, this, &SyncController::pollAuthorization);
    connect(&m_authorizationStartWatcher, &QFutureWatcherBase::finished, this, [this] {
        const auto result = m_authorizationStartWatcher.result();
        const auto &prompt = result.first;
        if (prompt.deviceCode.isEmpty()) {
            m_connecting = false;
            m_errorMessage = result.second;
            m_status = QStringLiteral("GitHub connection failed");
            qCWarning(syncLog) << "GitHub authorization could not start:" << m_errorMessage;
            emit stateChanged();
            return;
        }
        m_deviceCode = prompt.deviceCode;
        m_userCode = prompt.userCode;
        m_verificationUrl = prompt.verificationUrl;
        m_authorizationPoll.setInterval(qMax(1, prompt.pollIntervalSeconds) * 1'000);
        m_authorizationPoll.start();
        m_status = QStringLiteral("Waiting for GitHub authorization");
        qCInfo(syncLog) << "GitHub authorization page requested";
        emit stateChanged();
        emit consentPageRequested(m_verificationUrl);
    });
    connect(&m_authorizationFinishWatcher, &QFutureWatcherBase::finished, this, [this] {
        auto result = m_authorizationFinishWatcher.result();
        auto &connected = result.first;
        if (!connected.ready && result.second.isEmpty()) {
            if (connected.installationRequired) {
                clearPendingAuthorization();
                m_pendingAuthorization = std::move(connected.authorization);
                m_pendingRepository = std::move(connected.repository);
                if (!m_awaitingInstallation) {
                    m_awaitingInstallation = true;
                    m_status = QStringLiteral("Waiting for GitHub App installation");
                    qCInfo(syncLog) << "GitHub App installation page requested";
                    emit stateChanged();
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
        m_awaitingInstallation = false;
        clearPendingAuthorization();
        std::fill(m_pendingRecoveryKey.begin(), m_pendingRecoveryKey.end(), QChar {});
        m_pendingRecoveryKey.clear();
        if (!connected.ready) {
            m_errorMessage = result.second;
            m_status = QStringLiteral("GitHub connection failed");
            qCWarning(syncLog) << "GitHub connection failed:" << m_errorMessage;
            emit stateChanged();
            return;
        }
        m_login = connected.login;
        m_remoteUrl = connected.remoteUrl;
        m_recoveryKey = connected.recoveryKey;
        m_accessToken = std::move(connected.accessToken);
        m_accessTokenExpiresAt
            = QDateTime::currentDateTimeUtc().addSecs(connected.accessTokenExpiresInSeconds);
        m_initialRemoteRestore = !connected.repositoryCreated;
        m_enabled = true;
        m_pending = true;
        m_periodicReconcile.start();
        writeMarker();
        qCInfo(syncLog) << "GitHub Sync connection ready for" << m_login;
        emit stateChanged();
        syncNow();
    });
    m_quietReconcile.setSingleShot(true);
    m_quietReconcile.setInterval(30'000);
    connect(&m_quietReconcile, &QTimer::timeout, this, &SyncController::syncNow);
    m_periodicReconcile.setInterval(5 * 60 * 1'000);
    connect(&m_periodicReconcile, &QTimer::timeout, this, [this] {
        if (m_enabled) {
            syncNow();
        }
    });
    connect(&m_reconcileWatcher, &QFutureWatcherBase::finished, this, [this] {
        auto result = m_reconcileWatcher.result();
        m_syncing = false;
        if (!result.refreshedAccessToken.isEmpty()) {
            sodium_memzero(m_accessToken.data(), static_cast<size_t>(m_accessToken.size()));
            m_accessToken = std::move(result.refreshedAccessToken);
            m_accessTokenExpiresAt = QDateTime::currentDateTimeUtc().addSecs(
                result.refreshedAccessTokenExpiresInSeconds);
        }
        if (m_disconnectPending) {
            sodium_memzero(
                result.recoveryKey.data(), static_cast<size_t>(result.recoveryKey.size()));
            finishDisconnect();
            return;
        }
        if (!result.succeeded) {
            m_errorMessage = result.errorMessage;
            const auto keyFailure
                = result.errorMessage.contains(QStringLiteral("recovery key"), Qt::CaseInsensitive);
            if (keyFailure) {
                m_enabled = false;
                m_pending = false;
                m_quietReconcile.stop();
                m_periodicReconcile.stop();
                m_status = QStringLiteral("Sync is paused");
                writeMarker();
            } else if (m_enabled) {
                m_status = QStringLiteral("Sync failed");
                m_pending = true;
            }
            emit stateChanged();
            return;
        }
        if (!m_enabled) {
            sodium_memzero(
                result.recoveryKey.data(), static_cast<size_t>(result.recoveryKey.size()));
            return;
        }
        if (!m_changedWhileSyncing) {
            QString applyError;
            SqliteSessionStore store(m_dataRoot);
            SyncModule sync(store,
                {.dataRoot = m_dataRoot,
                    .configRoot = m_configRoot,
                    .remoteUrl = m_remoteUrl,
                    .machineId = machineId(),
                    .recoveryKey = result.recoveryKey,
                    .protectedTabId
                    = m_initialRemoteRestore ? QString {} : m_browser->activeTabId(),
                    .initialRemoteRestore = m_initialRemoteRestore,
                    .discardPristineLocalState
                    = m_initialRemoteRestore && m_browser->startedWithEmptyState(),
                    .replaceLocalState = m_initialRemoteRestore || result.remoteEpochAdvanced});
            if (!store.open(&applyError) || !sync.open(&applyError)
                || !sync.applyRemoteState(&applyError)) {
                sodium_memzero(
                    result.recoveryKey.data(), static_cast<size_t>(result.recoveryKey.size()));
                m_errorMessage = applyError;
                const auto keyFailure
                    = applyError.contains(QStringLiteral("recovery key"), Qt::CaseInsensitive);
                if (keyFailure) {
                    m_enabled = false;
                    m_pending = false;
                    m_quietReconcile.stop();
                    m_periodicReconcile.stop();
                    m_status = QStringLiteral("Sync is paused");
                    writeMarker();
                } else {
                    m_status = QStringLiteral("Sync failed");
                    m_pending = true;
                }
                emit stateChanged();
                return;
            }
            m_applyingRemote = true;
            m_browser->reloadSyncedState();
            if (m_keyboardNavigation) {
                m_keyboardNavigation->reload();
            }
            if (m_blocker) {
                m_blocker->reloadSyncedConfiguration();
            }
            m_applyingRemote = false;
        }
        sodium_memzero(result.recoveryKey.data(), static_cast<size_t>(result.recoveryKey.size()));
        m_initialRemoteRestore = false;
        m_pending = m_changedWhileSyncing;
        m_changedWhileSyncing = false;
        m_lastSuccessfulSync = QDateTime::currentDateTimeUtc();
        m_status = m_pending ? QStringLiteral("Changes waiting to sync") : QStringLiteral("Synced");
        writeMarker();
        emit stateChanged();
        if (m_pending) {
            m_quietReconcile.start();
        }
    });
    const auto watchModel = [this](QAbstractItemModel *model) {
        connect(model, &QAbstractItemModel::dataChanged, this, [this] { markPending(); });
        connect(model, &QAbstractItemModel::rowsInserted, this, [this] { markPending(); });
        connect(model, &QAbstractItemModel::rowsRemoved, this, [this] { markPending(); });
        connect(model, &QAbstractItemModel::rowsMoved, this, [this] { markPending(); });
        connect(model, &QAbstractItemModel::modelReset, this, [this] { markPending(); });
    };
    watchModel(browser->spaces());
    watchModel(browser->tabs());
    connect(browser, &BrowserController::preferenceChanged, this, [this](const QString &name) {
        static const QSet<QString> approved {QStringLiteral("floating-controls"),
            QStringLiteral("ease-sidebar"), QStringLiteral("use-favicons"),
            QStringLiteral("tint-favicons")};
        if (approved.contains(name)) {
            markPending();
        }
    });
    const auto keybindingsPath = QDir(m_configRoot).filePath(QStringLiteral("keybindings.json"));
    QFile keybindings(keybindingsPath);
    if (keybindings.open(QIODevice::ReadOnly)) {
        m_keybindingsDigest
            = QCryptographicHash::hash(keybindings.readAll(), QCryptographicHash::Sha256);
    }
    if (m_configWatcher.addPath(m_configRoot)) {
        connect(
            &m_configWatcher, &QFileSystemWatcher::directoryChanged, this, [this, keybindingsPath] {
                QFile file(keybindingsPath);
                const auto contents
                    = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
                const auto digest = QCryptographicHash::hash(contents, QCryptographicHash::Sha256);
                if (digest != m_keybindingsDigest) {
                    m_keybindingsDigest = digest;
                    markPending();
                }
            });
    }
    if (m_blocker) {
        m_subscriptionDigest = subscriptionDigest(m_blocker);
        connect(m_blocker, &ContentBlocker::subscriptionsChanged, this, [this] {
            const auto digest = subscriptionDigest(m_blocker);
            if (digest != m_subscriptionDigest) {
                m_subscriptionDigest = digest;
                markPending();
            }
        });
    }
    loadMarker();
    if (m_enabled) {
        m_periodicReconcile.start();
        m_pending = true;
        m_status = QStringLiteral("Sync starting");
        QTimer::singleShot(0, this, &SyncController::syncNow);
    }
}

SyncController::~SyncController()
{
    sodium_memzero(m_accessToken.data(), static_cast<size_t>(m_accessToken.size()));
    std::fill(m_recoveryKey.begin(), m_recoveryKey.end(), QChar {});
    std::fill(m_pendingRecoveryKey.begin(), m_pendingRecoveryKey.end(), QChar {});
    clearPendingAuthorization();
}

bool SyncController::enabled() const { return m_enabled; }
bool SyncController::connecting() const { return m_connecting; }
bool SyncController::awaitingInstallation() const { return m_awaitingInstallation; }
bool SyncController::pending() const { return m_pending; }
QString SyncController::provider() const { return QStringLiteral("GitHub"); }
QString SyncController::login() const { return m_login; }
QString SyncController::status() const { return m_status; }
QString SyncController::errorMessage() const { return m_errorMessage; }
QString SyncController::userCode() const { return m_userCode; }
QUrl SyncController::verificationUrl() const { return m_verificationUrl; }
QString SyncController::avatarPath() const
{
    const auto path = QDir(m_dataRoot).filePath(QStringLiteral("sync/avatar"));
    return QFileInfo::exists(path) ? path : QString {};
}
QString SyncController::recoveryKey() const { return m_recoveryKey; }
QDateTime SyncController::lastSuccessfulSync() const { return m_lastSuccessfulSync; }

void SyncController::loadMarker()
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
    m_initialRemoteRestore = marker.value(QStringLiteral("initialRemoteRestore")).toBool();
    m_lastSuccessfulSync = QDateTime::fromString(
        marker.value(QStringLiteral("lastSuccessfulSync")).toString(), Qt::ISODateWithMs);
}

bool SyncController::writeMarker()
{
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
            {QStringLiteral("initialRemoteRestore"), m_initialRemoteRestore},
            {QStringLiteral("lastSuccessfulSync"),
                m_lastSuccessfulSync.toString(Qt::ISODateWithMs)}})
            .toJson(QJsonDocument::Indented));
    return file.commit();
}

QString SyncController::machineId()
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

bool SyncController::beginGitHubConnection(const QString &recoveryKey)
{
    if (m_connecting) {
        return false;
    }
    m_errorMessage.clear();
    m_pendingRecoveryKey = recoveryKey;
    m_connecting = true;
    m_status = QStringLiteral("Contacting GitHub");
    qCInfo(syncLog) << "GitHub Sync connection started";
    emit stateChanged();
    m_authorizationStartWatcher.setFuture(QtConcurrent::run([recoveryKey] {
        QString error;
        GitHubForge forge(QString::fromLatin1(OMAWEB_GITHUB_APP_CLIENT_ID),
            QString::fromLatin1(OMAWEB_GITHUB_APP_SLUG));
        LinuxSecretStore secrets;
        SyncSetup setup(forge, secrets, {});
        const auto prompt = setup.beginConnect(recoveryKey, &error);
        return QPair {prompt, error};
    }));
    return true;
}

void SyncController::pollAuthorization()
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
            GitHubForge forge(QString::fromLatin1(OMAWEB_GITHUB_APP_CLIENT_ID),
                QString::fromLatin1(OMAWEB_GITHUB_APP_SLUG));
            LinuxSecretStore secrets;
            SyncSetup setup(forge, secrets, dataRoot);
            if (authorization.state == AuthorizationState::Complete) {
                setup.resumeConnect(std::move(authorization), std::move(repository), recoveryKey);
            } else {
                setup.resumeConnect(deviceCode, recoveryKey);
            }
            auto connection = setup.finishConnect(&error);
            return QPair {std::move(connection), error};
        }));
}

void SyncController::clearPendingAuthorization()
{
    sodium_memzero(m_pendingAuthorization.accessToken.data(),
        static_cast<size_t>(m_pendingAuthorization.accessToken.size()));
    sodium_memzero(m_pendingAuthorization.refreshToken.data(),
        static_cast<size_t>(m_pendingAuthorization.refreshToken.size()));
    m_pendingAuthorization = {};
    m_pendingRepository = {};
}

void SyncController::markPending()
{
    if (!m_enabled || m_applyingRemote) {
        return;
    }
    if (m_syncing) {
        m_changedWhileSyncing = true;
        return;
    }
    m_pending = true;
    m_status = QStringLiteral("Changes waiting to sync");
    m_quietReconcile.start();
    emit stateChanged();
}

void SyncController::syncNow()
{
    if (!m_enabled || m_syncing || m_login.isEmpty() || !m_remoteUrl.isValid()) {
        return;
    }
    m_status = QStringLiteral("Syncing");
    m_errorMessage.clear();
    emit stateChanged();
    const auto tokenIsCurrent = !m_accessToken.isEmpty()
        && m_accessTokenExpiresAt > QDateTime::currentDateTimeUtc().addSecs(60);
    m_cancellationRequested = std::make_shared<std::atomic_bool>(false);
    SyncOptions options {.dataRoot = m_dataRoot,
        .configRoot = m_configRoot,
        .remoteUrl = m_remoteUrl,
        .machineId = machineId(),
        .recoveryKey = {},
        .accessToken = tokenIsCurrent ? m_accessToken : QByteArray {},
        .authorName = m_login,
        .protectedTabId = m_initialRemoteRestore ? QString {} : m_browser->activeTabId(),
        .initialRemoteRestore = m_initialRemoteRestore,
        .discardPristineLocalState = m_initialRemoteRestore && m_browser->startedWithEmptyState(),
        .deferRemoteApply = true,
        .cancellationRequested = m_cancellationRequested};
    const auto login = m_login;
    m_syncing = true;
    m_reconcileWatcher.setFuture(QtConcurrent::run([options = std::move(options), login]() mutable {
        QString workerError;
        LinuxSecretStore secrets;
        options.recoveryKey
            = secrets.retrieve(QStringLiteral("sync-key/%1").arg(login), &workerError);
        if (options.recoveryKey.size() != 32) {
            ReconcileResult result;
            result.errorMessage = QStringLiteral("Enter the Sync recovery key");
            return result;
        }
        QByteArray refreshedAccessToken;
        int refreshedAccessTokenExpiresInSeconds = 0;
        if (options.accessToken.isEmpty()) {
            auto refresh
                = secrets.retrieve(QStringLiteral("forge-refresh/%1").arg(login), &workerError);
            GitHubForge forge(QString::fromLatin1(OMAWEB_GITHUB_APP_CLIENT_ID),
                QString::fromLatin1(OMAWEB_GITHUB_APP_SLUG));
            auto authorization = forge.refreshAuthorization(refresh, &workerError);
            sodium_memzero(refresh.data(), static_cast<size_t>(refresh.size()));
            if (authorization.state != AuthorizationState::Complete) {
                ReconcileResult result;
                result.errorMessage = workerError.isEmpty()
                    ? QStringLiteral("GitHub authorization expired; connect again")
                    : workerError;
                return result;
            }
            options.accessToken = std::move(authorization.accessToken);
            refreshedAccessToken = options.accessToken;
            refreshedAccessTokenExpiresInSeconds = authorization.expiresInSeconds;
            if (!authorization.refreshToken.isEmpty()) {
                if (!secrets.store(QStringLiteral("forge-refresh/%1").arg(login),
                        authorization.refreshToken, &workerError)) {
                    ReconcileResult result;
                    result.errorMessage = workerError;
                    return result;
                }
                sodium_memzero(authorization.refreshToken.data(),
                    static_cast<size_t>(authorization.refreshToken.size()));
            }
        }
        SqliteSessionStore store(options.dataRoot);
        if (!store.open(&workerError)) {
            ReconcileResult result;
            result.errorMessage = workerError;
            return result;
        }
        auto recoveryKey = options.recoveryKey;
        SyncModule sync(store, std::move(options));
        const auto succeeded = sync.open(&workerError) && sync.reconcile(&workerError);
        return ReconcileResult {.succeeded = succeeded,
            .errorMessage = workerError,
            .recoveryKey = std::move(recoveryKey),
            .refreshedAccessToken = std::move(refreshedAccessToken),
            .refreshedAccessTokenExpiresInSeconds = refreshedAccessTokenExpiresInSeconds,
            .remoteEpochAdvanced = sync.remoteEpochAdvanced()};
    }));
}

void SyncController::pause()
{
    if (m_cancellationRequested) {
        m_cancellationRequested->store(true);
    }
    m_enabled = false;
    m_pending = false;
    m_quietReconcile.stop();
    m_periodicReconcile.stop();
    m_status = QStringLiteral("Sync is paused");
    writeMarker();
    emit stateChanged();
}

void SyncController::resume()
{
    if (m_login.isEmpty() || !m_remoteUrl.isValid()) {
        return;
    }
    m_enabled = true;
    m_pending = true;
    m_periodicReconcile.start();
    m_status = QStringLiteral("Sync starting");
    writeMarker();
    emit stateChanged();
    syncNow();
}

void SyncController::disconnectProvider()
{
    m_enabled = false;
    m_pending = false;
    m_quietReconcile.stop();
    m_periodicReconcile.stop();
    if (m_syncing) {
        m_disconnectPending = true;
        if (m_cancellationRequested) {
            m_cancellationRequested->store(true);
        }
        m_status = QStringLiteral("Disconnecting Sync");
        emit stateChanged();
        return;
    }
    finishDisconnect();
}

void SyncController::finishDisconnect()
{
    m_disconnectPending = false;
    if (!m_secrets) {
        m_secrets = std::make_unique<LinuxSecretStore>();
    }
    m_secrets->remove(QStringLiteral("forge-refresh/%1").arg(m_login), nullptr);
    m_secrets->remove(QStringLiteral("sync-key/%1").arg(m_login), nullptr);
    sodium_memzero(m_accessToken.data(), static_cast<size_t>(m_accessToken.size()));
    m_accessToken.clear();
    m_accessTokenExpiresAt = {};
    QFile::remove(QDir(m_dataRoot).filePath(QStringLiteral("sync/avatar")));
    QDir(QDir(m_dataRoot).filePath(QStringLiteral("sync/repository"))).removeRecursively();
    QFile::remove(QDir(m_configRoot).filePath(QStringLiteral("sync.json")));
    m_login.clear();
    m_remoteUrl.clear();
    clearRecoveryKey();
    m_status = QStringLiteral("Sync is off");
    emit stateChanged();
}

void SyncController::clearRecoveryKey()
{
    std::fill(m_recoveryKey.begin(), m_recoveryKey.end(), QChar {});
    m_recoveryKey.clear();
    emit stateChanged();
}

bool SyncController::saveRecoveryKey(const QUrl &destination)
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

} // namespace omaweb
