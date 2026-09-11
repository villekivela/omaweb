#pragma once

#include "ForgeProvider.h"
#include "SyncSetup.h"

#include <QByteArray>
#include <QDateTime>
#include <QFutureWatcher>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <atomic>

namespace omaweb {

class BrowserController;
class ContentBlocker;
class GitHubForge;
class LinuxSecretStore;
class KeyboardNavigation;

struct ReconcileResult {
    bool succeeded = false;
    QString errorMessage;
    QByteArray recoveryKey;
    QByteArray refreshedAccessToken;
    int refreshedAccessTokenExpiresInSeconds = 0;
    bool remoteEpochAdvanced = false;
};

class SyncController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY stateChanged)
    Q_PROPERTY(bool pending READ pending NOTIFY stateChanged)
    Q_PROPERTY(QString provider READ provider CONSTANT)
    Q_PROPERTY(QString login READ login NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(QString userCode READ userCode NOTIFY stateChanged)
    Q_PROPERTY(QUrl verificationUrl READ verificationUrl NOTIFY stateChanged)
    Q_PROPERTY(QUrl installationUrl READ installationUrl CONSTANT)
    Q_PROPERTY(QString avatarPath READ avatarPath NOTIFY stateChanged)
    Q_PROPERTY(QString recoveryKey READ recoveryKey NOTIFY stateChanged)
    Q_PROPERTY(QDateTime lastSuccessfulSync READ lastSuccessfulSync NOTIFY stateChanged)

public:
    SyncController(BrowserController *browser, ContentBlocker *blocker,
        KeyboardNavigation *keyboardNavigation, QString dataRoot, QString configRoot,
        QObject *parent = nullptr);
    ~SyncController() override;

    bool enabled() const;
    bool connecting() const;
    bool pending() const;
    QString provider() const;
    QString login() const;
    QString status() const;
    QString errorMessage() const;
    QString userCode() const;
    QUrl verificationUrl() const;
    QUrl installationUrl() const;
    QString avatarPath() const;
    QString recoveryKey() const;
    QDateTime lastSuccessfulSync() const;

    Q_INVOKABLE bool beginGitHubConnection(const QString &recoveryKey);
    Q_INVOKABLE void syncNow();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void disconnectProvider();
    Q_INVOKABLE void clearRecoveryKey();
    Q_INVOKABLE bool saveRecoveryKey(const QUrl &destination);

signals:
    void stateChanged();

private:
    void loadMarker();
    bool writeMarker();
    QString machineId();
    void pollAuthorization();
    void markPending();
    void finishDisconnect();

    BrowserController *m_browser = nullptr;
    ContentBlocker *m_blocker = nullptr;
    KeyboardNavigation *m_keyboardNavigation = nullptr;
    QString m_dataRoot;
    QString m_configRoot;
    QString m_machineId;
    QString m_login;
    QUrl m_remoteUrl;
    QByteArray m_accessToken;
    QByteArray m_keybindingsDigest;
    QByteArray m_subscriptionDigest;
    QDateTime m_accessTokenExpiresAt;
    QString m_status = QStringLiteral("Sync is off");
    QString m_errorMessage;
    QString m_userCode;
    QUrl m_verificationUrl;
    QString m_recoveryKey;
    QString m_deviceCode;
    QString m_pendingRecoveryKey;
    QDateTime m_lastSuccessfulSync;
    bool m_enabled = false;
    bool m_connecting = false;
    bool m_pending = false;
    bool m_syncing = false;
    bool m_initialRemoteRestore = false;
    bool m_changedWhileSyncing = false;
    bool m_applyingRemote = false;
    bool m_disconnectPending = false;
    QTimer m_authorizationPoll;
    QTimer m_quietReconcile;
    QTimer m_periodicReconcile;
    QFileSystemWatcher m_configWatcher;
    std::unique_ptr<LinuxSecretStore> m_secrets;
    std::shared_ptr<std::atomic_bool> m_cancellationRequested;
    QFutureWatcher<QPair<DeviceAuthorization, QString>> m_authorizationStartWatcher;
    QFutureWatcher<QPair<SyncConnection, QString>> m_authorizationFinishWatcher;
    QFutureWatcher<ReconcileResult> m_reconcileWatcher;
};

} // namespace omaweb
