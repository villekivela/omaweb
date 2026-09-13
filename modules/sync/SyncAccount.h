#pragma once

#include "ForgeProvider.h"
#include "SyncError.h"
#include "SyncModule.h"
#include "SyncSetup.h"

#include <QByteArray>
#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

namespace omaweb {

// Everything a connection to a provider account owns: the authorization steps and the consent pages
// they need, the private repository, the App installation, the secrets the connection keeps, the
// avatar, the recovery key a person must hold on to, and the marker that remembers the connection
// across restarts. A reconciliation asks the account for a session and knows none of this.
class SyncAccount final : public QObject {
    Q_OBJECT

public:
    // What one reconciliation needs from the account, gathered on the thread that will use it.
    struct Session {
        SyncCredentials credentials;
        QByteArray refreshedAccessToken;
        int refreshedAccessTokenExpiresInSeconds = 0;
        SyncError error;
    };

    SyncAccount(QString dataRoot, QString configRoot, QObject *parent = nullptr);
    ~SyncAccount() override;

    QString provider() const;
    QVariantMap providerText() const;
    QString commitEmail() const;
    bool connected() const;
    bool connecting() const;
    bool awaitingRepositoryCreation() const;
    bool awaitingInstallation() const;
    QString login() const;
    QUrl remoteUrl() const;
    QString userCode() const;
    QUrl verificationUrl() const;
    QString avatarPath() const;
    QString recoveryKey() const;
    QString machineId();

    // The remote decides the first pass after connecting to a repository that already holds state.
    SyncIntent intent() const;
    void markRemoteAdopted();

    bool wasEnabled() const;
    QDateTime lastSuccessfulSync() const;
    bool remember(bool enabled, const QDateTime &lastSuccessfulSync);

    bool beginConnection(const QString &recoveryKey);
    bool continueConnection();
    void forget();
    void clearRecoveryKey();
    bool saveRecoveryKey(const QUrl &destination) const;

    QByteArray currentAccessToken() const;
    void cacheAccessToken(QByteArray token, int expiresInSeconds);
    static Session openSession(const QString &login, QByteArray accessToken);

signals:
    void changed();
    void statusChanged(const QString &status);
    void consentPageRequested(const QUrl &url);
    void connectionReady();
    void connectionFailed(const QString &message);

private:
    void loadMarker();
    void pollAuthorization();
    void clearPendingAuthorization();
    void announce(const QString &status);

    QString m_dataRoot;
    QString m_configRoot;
    QString m_machineId;
    QString m_login;
    QUrl m_remoteUrl;
    QByteArray m_accessToken;
    QDateTime m_accessTokenExpiresAt;
    ForgeAuthorization m_pendingAuthorization;
    ForgeRepository m_pendingRepository;
    QString m_userCode;
    QUrl m_verificationUrl;
    QString m_recoveryKey;
    QString m_deviceCode;
    QString m_pendingRecoveryKey;
    QDateTime m_lastSuccessfulSync;
    bool m_enabled = false;
    bool m_connecting = false;
    bool m_awaitingRepositoryCreation = false;
    bool m_awaitingInstallation = false;
    bool m_adoptsRemote = false;
    QTimer m_authorizationPoll;
    QFutureWatcher<QPair<DeviceAuthorization, QString>> m_authorizationStartWatcher;
    QFutureWatcher<QPair<SyncConnection, QString>> m_authorizationFinishWatcher;
};

} // namespace omaweb
