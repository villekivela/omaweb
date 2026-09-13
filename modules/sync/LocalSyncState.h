#pragma once

#include "BrowserStateExchange.h"
#include "SyncError.h"

#include <QByteArray>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

namespace omaweb {

class SyncModule;

struct LocalSyncRevision {
    quint64 generation = 0;
    QString protectedTabId;
    bool pristine = false;
};

enum class LocalSyncApplyStatus { Applied, NoChange, Stale, Refused, Failed };

struct LocalSyncApplyResult {
    LocalSyncApplyStatus status = LocalSyncApplyStatus::Failed;
    SyncError error;
};

class LocalSyncState final : public QObject {
    Q_OBJECT

public:
    LocalSyncState(BrowserStateExchange &exchange, QString dataRoot, QString configRoot,
        QObject *parent = nullptr);

    bool eligible() const;
    LocalSyncRevision checkpoint() const;
    // Finishes a reconciliation that the worker thread prepared, on the store this thread owns.
    LocalSyncApplyResult applyRemoteState(SyncModule &transaction, quint64 expectedGeneration);

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
