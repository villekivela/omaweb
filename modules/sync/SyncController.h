#pragma once

#include "SyncError.h"

#include <QByteArray>
#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <memory>
#include <atomic>

namespace omaweb {

class BrowserStateExchange;
class LocalSyncState;
class SyncAccount;
class SyncModule;

// What the worker thread hands back: the transaction it prepared, ready for the shell thread to
// finish, and the local generation it started from.
struct ReconcileResult {
    SyncError error;
    std::shared_ptr<SyncModule> transaction;
    QByteArray refreshedAccessToken;
    int refreshedAccessTokenExpiresInSeconds = 0;
    quint64 localGeneration = 0;
};

class SyncController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY stateChanged)
    Q_PROPERTY(bool awaitingRepositoryCreation READ awaitingRepositoryCreation NOTIFY stateChanged)
    Q_PROPERTY(bool awaitingInstallation READ awaitingInstallation NOTIFY stateChanged)
    Q_PROPERTY(bool pending READ pending NOTIFY stateChanged)
    Q_PROPERTY(QString provider READ provider CONSTANT)
    Q_PROPERTY(QVariantMap providerText READ providerText NOTIFY stateChanged)
    Q_PROPERTY(QString login READ login NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(QString userCode READ userCode NOTIFY stateChanged)
    Q_PROPERTY(QUrl verificationUrl READ verificationUrl NOTIFY stateChanged)
    Q_PROPERTY(QString avatarPath READ avatarPath NOTIFY stateChanged)
    Q_PROPERTY(QString recoveryKey READ recoveryKey NOTIFY stateChanged)
    Q_PROPERTY(QDateTime lastSuccessfulSync READ lastSuccessfulSync NOTIFY stateChanged)

public:
    SyncController(BrowserStateExchange *state, QString dataRoot, QString configRoot,
        QObject *parent = nullptr);
    ~SyncController() override;

    bool enabled() const;
    bool connecting() const;
    bool awaitingRepositoryCreation() const;
    bool awaitingInstallation() const;
    bool pending() const;
    QString provider() const;
    QVariantMap providerText() const;
    QString login() const;
    QString status() const;
    QString errorMessage() const;
    QString userCode() const;
    QUrl verificationUrl() const;
    QString avatarPath() const;
    QString recoveryKey() const;
    QDateTime lastSuccessfulSync() const;

    Q_INVOKABLE bool beginConnection(const QString &recoveryKey);
    Q_INVOKABLE bool continueConnection();
    Q_INVOKABLE void syncNow();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void disconnectProvider();
    Q_INVOKABLE void clearRecoveryKey();
    Q_INVOKABLE bool saveRecoveryKey(const QUrl &destination);

signals:
    void stateChanged();
    void consentPageRequested(const QUrl &url);

private:
    void markPending();
    void reportFailure(const SyncError &error);
    void finishDisconnect();
    bool remember();

    std::unique_ptr<SyncAccount> m_account;
    std::unique_ptr<LocalSyncState> m_localState;
    QString m_dataRoot;
    QString m_configRoot;
    QString m_status = QStringLiteral("Sync is off");
    QString m_errorMessage;
    QDateTime m_lastSuccessfulSync;
    bool m_enabled = false;
    bool m_pending = false;
    bool m_syncing = false;
    bool m_disconnectPending = false;
    QTimer m_quietReconcile;
    QTimer m_periodicReconcile;
    std::shared_ptr<std::atomic_bool> m_cancellationRequested;
    QFutureWatcher<ReconcileResult> m_reconcileWatcher;
};

} // namespace omaweb
