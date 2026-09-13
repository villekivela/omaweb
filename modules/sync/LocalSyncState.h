#pragma once

#include "BrowserStateExchange.h"

#include <QByteArray>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

namespace omaweb {

struct LocalSyncRevision {
    quint64 generation = 0;
    QString protectedTabId;
    bool pristine = false;
};

enum class LocalSyncApplyStatus { Applied, NoChange, Stale, Refused, Failed };

enum class LocalSyncFailureCode { None, PrivateStateRefused, LocalStateChanged, StorageFailed };

struct LocalSyncApplyRequest {
    quint64 expectedGeneration = 0;
    QUrl remoteUrl;
    QString machineId;
    QString protectedTabId;
    QByteArray recoveryKey;
    bool initialRemoteRestore = false;
    bool discardPristineLocalState = false;
    bool replaceLocalState = false;
};

struct LocalSyncApplyResult {
    LocalSyncApplyStatus status = LocalSyncApplyStatus::Failed;
    LocalSyncFailureCode failure = LocalSyncFailureCode::None;
    QString errorMessage;
};

class LocalSyncState final : public QObject {
    Q_OBJECT

public:
    LocalSyncState(BrowserStateExchange &exchange, QString dataRoot, QString configRoot,
        QObject *parent = nullptr);

    bool eligible() const;
    LocalSyncRevision checkpoint() const;
    LocalSyncApplyResult applyRemoteState(LocalSyncApplyRequest request);

signals:
    void meaningfulChange(quint64 generation);

private:
    struct Fingerprints {
        QByteArray browser;
        QByteArray keybindings;
        QByteArray filterSubscriptions;

        bool operator==(const Fingerprints &) const = default;
    };

    static BrowserStateSelection selection();
    static Fingerprints fingerprints(const BrowserStateImage &image);
    void scheduleProjectionCheck();
    void checkProjection();

    BrowserStateExchange &m_exchange;
    QString m_dataRoot;
    QString m_configRoot;
    Fingerprints m_fingerprints;
    quint64 m_generation = 0;
    bool m_applying = false;
    QTimer m_projectionCheck;
    QFileSystemWatcher m_configWatcher;
};

} // namespace omaweb
