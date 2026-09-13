#pragma once

#include "SyncError.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QSet>
#include <QUrl>

#include <functional>
#include <atomic>
#include <memory>

namespace omaweb {

class SessionStore;

struct SyncCredentials {
    QByteArray recoveryKey;
    QByteArray accessToken {};
};

struct SyncOptions {
    QString dataRoot;
    QString configRoot;
    QUrl remoteUrl;
    QString machineId;
    QString askPassPath {};
    QString authorName {};
    QString authorEmail {};
    QString protectedTabId {};
    SyncIntent intent = SyncIntent::Reconcile;
    bool localStateIsPristine = false;
    std::shared_ptr<std::atomic_bool> cancellationRequested {};
    std::function<qint64()> now {};
    int historyCommitLimit = 256;
    qint64 historyMaxAgeMilliseconds = 30LL * 24 * 60 * 60 * 1'000;
    qint64 tombstoneRetentionMilliseconds = 90LL * 24 * 60 * 60 * 1'000;
};

class SyncModule final : public QObject {
    Q_OBJECT

public:
    static constexpr int contractVersion = 1;

    explicit SyncModule(SyncOptions options, QObject *parent = nullptr);
    ~SyncModule() override;

    static QString createRecoveryKey();
    static QByteArray decodeRecoveryKey(const QString &displayed, QString *errorMessage = nullptr);

    // One reconciliation runs as two phases. `reconcile` settles the remote from the worker thread
    // that owns it; `applyRemoteState` puts the merged result into local state on the shell thread.
    // Each phase is handed the session store belonging to its own thread, and what the first phase
    // learned about the remote carries into the second without passing through the caller.
    SyncError open(SyncCredentials credentials);
    SyncError reconcile(SessionStore &store);
    bool awaitsLocalApply() const;
    SyncError applyRemoteState(SessionStore &store);

private:
    bool openCheckout(QString *errorMessage);
    bool reconcileRemote(SessionStore &store, QString *errorMessage);
    SyncError phaseError(const QString &message) const;
    bool captureConfiguration(SessionStore &store, QString *errorMessage);
    bool restoreConfiguration(SessionStore &store, QString *errorMessage);
    bool restoreRemoteState(SessionStore &store, QString *errorMessage);
    bool gitRefExists(const QString &reference) const;
    bool mergeRemote(QString *errorMessage);
    bool resolveMergeConflicts(QString *errorMessage);
    bool prepareGitCredential(QProcess &git, int *readDescriptor, QString *errorMessage) const;
    bool runGit(const QStringList &arguments, QString *errorMessage) const;
    QByteArray gitOutput(const QStringList &arguments, QString *errorMessage = nullptr) const;
    bool compactHistoryIfNeeded(QString *errorMessage);
    QByteArray readEncryptedRecord(
        const QString &kind, const QString &id, const QString &path, QString *errorMessage) const;
    QByteArray decryptEncryptedRecord(const QString &kind, const QString &id,
        const QByteArray &encrypted, QString *errorMessage) const;
    bool writeEncryptedRecord(
        const QString &kind, const QString &id, const QByteArray &plainText, QString *errorMessage);
    bool tombstoneMissingRecords(
        const QString &kind, const QSet<QString> &currentIds, QString *errorMessage);
    QString protectedTabId() const;
    QByteArray checkoutTree() const;
    bool checkoutAheadOfLocalState() const;
    QString checkoutRoot() const;
    bool cancelled(QString *errorMessage) const;
    bool writeAppliedRecordInventory(QString *errorMessage) const;
    bool writeLocalBaselineInventory(SessionStore &store, QString *errorMessage) const;
    bool stageConfigurationFile(
        const QString &relativePath, const QByteArray &contents, QString *errorMessage) const;

    SyncOptions m_options;
    SyncCredentials m_credentials;
    mutable SyncFailure m_failure = SyncFailure::Failed;
    bool m_reconciled = false;
    bool m_heldBackLocalRecord = false;
    bool m_remoteEpochAdvanced = false;
    bool m_remoteStateChanged = false;
};

} // namespace omaweb
