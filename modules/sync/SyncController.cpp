#include "SyncController.h"

#include "BrowserStateExchange.h"
#include "LocalSyncState.h"
#include "SqliteSessionStore.h"
#include "SyncAccount.h"
#include "SyncModule.h"

#include <QLoggingCategory>
#include <QtConcurrentRun>

#include <utility>

namespace omaweb {

Q_LOGGING_CATEGORY(syncLog, "omaweb.sync")

SyncController::SyncController(
    BrowserStateExchange *state, QString dataRoot, QString configRoot, QObject *parent)
    : QObject(parent)
    , m_account(std::make_unique<SyncAccount>(dataRoot, configRoot))
    , m_localState(std::make_unique<LocalSyncState>(*state, dataRoot, configRoot))
    , m_dataRoot(std::move(dataRoot))
    , m_configRoot(std::move(configRoot))
{
    connect(m_localState.get(), &LocalSyncState::meaningfulChange, this,
        [this](quint64) { markPending(); });
    connect(m_account.get(), &SyncAccount::changed, this, &SyncController::stateChanged);
    connect(m_account.get(), &SyncAccount::consentPageRequested, this,
        &SyncController::consentPageRequested);
    connect(m_account.get(), &SyncAccount::statusChanged, this, [this](const QString &status) {
        m_status = status;
        m_errorMessage.clear();
    });
    connect(m_account.get(), &SyncAccount::connectionFailed, this, [this](const QString &message) {
        m_errorMessage = message;
        emit stateChanged();
    });
    connect(m_account.get(), &SyncAccount::connectionReady, this, [this] {
        m_enabled = true;
        m_pending = true;
        m_periodicReconcile.start();
        remember();
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
        m_account->cacheAccessToken(
            std::move(result.refreshedAccessToken), result.refreshedAccessTokenExpiresInSeconds);
        if (m_disconnectPending) {
            finishDisconnect();
            return;
        }
        if (result.error) {
            reportFailure(result.error);
            return;
        }
        if (!m_enabled) {
            return;
        }
        auto &transaction = *result.transaction;
        const auto revision = m_localState->checkpoint();
        auto deferred = revision.generation != result.localGeneration;
        if (!deferred && transaction.awaitsLocalApply()) {
            const auto applied
                = m_localState->applyRemoteState(transaction, result.localGeneration);
            deferred = applied.status == LocalSyncApplyStatus::Stale;
            if (applied.status == LocalSyncApplyStatus::Failed
                || applied.status == LocalSyncApplyStatus::Refused) {
                reportFailure(applied.error);
                return;
            }
        }
        if (!deferred) {
            m_account->markRemoteAdopted();
        }
        m_pending = deferred || m_localState->checkpoint().generation != result.localGeneration;
        m_lastSuccessfulSync = QDateTime::currentDateTimeUtc();
        m_status = m_pending ? QStringLiteral("Changes waiting to sync") : QStringLiteral("Synced");
        remember();
        emit stateChanged();
        if (m_pending) {
            m_quietReconcile.start();
        }
    });
    m_enabled = m_account->wasEnabled();
    m_lastSuccessfulSync = m_account->lastSuccessfulSync();
    if (m_enabled) {
        m_periodicReconcile.start();
        m_pending = true;
        m_status = QStringLiteral("Sync starting");
        QTimer::singleShot(0, this, &SyncController::syncNow);
    }
}

SyncController::~SyncController() = default;

bool SyncController::enabled() const { return m_enabled; }
bool SyncController::connecting() const { return m_account->connecting(); }
bool SyncController::awaitingRepositoryCreation() const
{
    return m_account->awaitingRepositoryCreation();
}
bool SyncController::awaitingInstallation() const { return m_account->awaitingInstallation(); }
bool SyncController::pending() const { return m_pending; }
QString SyncController::provider() const { return m_account->provider(); }
QString SyncController::login() const { return m_account->login(); }
QString SyncController::status() const { return m_status; }
QString SyncController::errorMessage() const { return m_errorMessage; }
QString SyncController::userCode() const { return m_account->userCode(); }
QUrl SyncController::verificationUrl() const { return m_account->verificationUrl(); }
QString SyncController::avatarPath() const { return m_account->avatarPath(); }
QString SyncController::recoveryKey() const { return m_account->recoveryKey(); }
QDateTime SyncController::lastSuccessfulSync() const { return m_lastSuccessfulSync; }

bool SyncController::remember() { return m_account->remember(m_enabled, m_lastSuccessfulSync); }

bool SyncController::beginConnection(const QString &recoveryKey)
{
    return m_account->beginConnection(recoveryKey);
}

bool SyncController::continueConnection() { return m_account->continueConnection(); }

QVariantMap SyncController::providerText() const { return m_account->providerText(); }

void SyncController::markPending()
{
    if (!m_enabled) {
        return;
    }
    m_pending = true;
    if (!m_syncing) {
        m_status = QStringLiteral("Changes waiting to sync");
        m_quietReconcile.start();
    }
    emit stateChanged();
}

void SyncController::reportFailure(const SyncError &error)
{
    m_errorMessage = error.message;
    if (error.pausesSync()) {
        m_enabled = false;
        m_pending = false;
        m_quietReconcile.stop();
        m_periodicReconcile.stop();
        m_status = QStringLiteral("Sync is paused");
        remember();
    } else if (m_enabled) {
        m_status = QStringLiteral("Sync failed");
        m_pending = true;
    }
    emit stateChanged();
}

void SyncController::syncNow()
{
    if (!m_enabled || m_syncing || !m_account->connected()) {
        return;
    }
    m_status = QStringLiteral("Syncing");
    m_errorMessage.clear();
    emit stateChanged();
    m_cancellationRequested = std::make_shared<std::atomic_bool>(false);
    const auto local = m_localState->checkpoint();
    auto transaction = std::make_shared<SyncModule>(SyncOptions {.dataRoot = m_dataRoot,
        .configRoot = m_configRoot,
        .remoteUrl = m_account->remoteUrl(),
        .machineId = m_account->machineId(),
        .authorName = m_account->login(),
        .authorEmail = m_account->commitEmail(),
        .protectedTabId = local.protectedTabId,
        .intent = m_account->intent(),
        .localStateIsPristine = local.pristine,
        .cancellationRequested = m_cancellationRequested});
    const auto login = m_account->login();
    const auto dataRoot = m_dataRoot;
    const auto localGeneration = local.generation;
    auto accessToken = m_account->currentAccessToken();
    m_syncing = true;
    m_reconcileWatcher.setFuture(
        QtConcurrent::run([transaction, login, dataRoot, localGeneration,
                              accessToken = std::move(accessToken)]() mutable {
            auto session = SyncAccount::openSession(login, std::move(accessToken));
            const auto answer = [&session, &transaction, localGeneration](SyncError error) {
                return ReconcileResult {.error = std::move(error),
                    .transaction = transaction,
                    .refreshedAccessToken = std::move(session.refreshedAccessToken),
                    .refreshedAccessTokenExpiresInSeconds
                    = session.refreshedAccessTokenExpiresInSeconds,
                    .localGeneration = localGeneration};
            };
            if (session.error) {
                return answer(session.error);
            }
            QString storeError;
            SqliteSessionStore store(dataRoot);
            if (!store.open(&storeError)) {
                return answer({.failure = SyncFailure::Failed, .message = storeError});
            }
            if (auto opened = transaction->open(std::move(session.credentials))) {
                return answer(std::move(opened));
            }
            return answer(transaction->reconcile(store));
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
    remember();
    emit stateChanged();
}

void SyncController::resume()
{
    if (!m_account->connected()) {
        return;
    }
    m_enabled = true;
    m_pending = true;
    m_periodicReconcile.start();
    m_status = QStringLiteral("Sync starting");
    remember();
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
    m_lastSuccessfulSync = {};
    m_account->forget();
    m_status = QStringLiteral("Sync is off");
    emit stateChanged();
}

void SyncController::clearRecoveryKey() { m_account->clearRecoveryKey(); }

bool SyncController::saveRecoveryKey(const QUrl &destination)
{
    return m_account->saveRecoveryKey(destination);
}

} // namespace omaweb
