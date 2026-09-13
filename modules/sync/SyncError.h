#pragma once

#include <QString>

namespace omaweb {

// What a reconciliation is for. The ordinary pass merges local and remote; the adopting pass is the
// first one against a repository that already holds state, and lets the remote decide.
enum class SyncIntent {
    Reconcile,
    AdoptRemote,
};

// Why a reconciliation stopped. Callers decide what to do from the code, never from the message.
enum class SyncFailure {
    None,
    Failed,
    Cancelled,
    PrivateStateRefused,
    RecoveryKeyRejected,
    AuthorizationExpired,
    LocalStateChanged,
};

struct SyncError {
    SyncFailure failure = SyncFailure::None;
    QString message;

    explicit operator bool() const { return failure != SyncFailure::None; }

    // A rejected recovery key cannot be retried without the person, so Sync stops until they act.
    bool pausesSync() const { return failure == SyncFailure::RecoveryKeyRejected; }
};

} // namespace omaweb
