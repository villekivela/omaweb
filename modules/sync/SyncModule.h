#pragma once

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

struct SyncOptions {
    QString dataRoot;
    QString configRoot;
    QUrl remoteUrl;
    QString machineId;
    QByteArray recoveryKey;
    QByteArray accessToken {};
    QString askPassPath {};
    QString authorName {};
    QString protectedTabId {};
    bool initialRemoteRestore = false;
    bool discardPristineLocalState = false;
    bool deferRemoteApply = false;
    bool replaceLocalState = false;
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

    explicit SyncModule(SessionStore &store, SyncOptions options, QObject *parent = nullptr);
    ~SyncModule() override;

    static QString createRecoveryKey();
    static QByteArray decodeRecoveryKey(const QString &displayed, QString *errorMessage = nullptr);
    static bool includesSyncedSpaceChange(const QList<int> &roles);
    static bool includesSyncedTabChange(const QList<int> &roles);
    bool open(QString *errorMessage = nullptr);
    bool reconcile(QString *errorMessage = nullptr);
    bool applyRemoteState(QString *errorMessage = nullptr);
    bool remoteEpochAdvanced() const;
    bool remoteStateChanged() const;

private:
    bool captureConfiguration(QString *errorMessage);
    bool restoreConfiguration(QString *errorMessage);
    bool restoreRemoteState(QString *errorMessage);
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
    QString checkoutRoot() const;
    bool cancelled(QString *errorMessage) const;
    bool writeAppliedRecordInventory(QString *errorMessage) const;
    bool writeLocalBaselineInventory(QString *errorMessage) const;
    bool stageConfigurationFile(
        const QString &relativePath, const QByteArray &contents, QString *errorMessage) const;

    SessionStore &m_store;
    SyncOptions m_options;
    bool m_remoteEpochAdvanced = false;
    bool m_remoteStateChanged = false;
};

} // namespace omaweb
