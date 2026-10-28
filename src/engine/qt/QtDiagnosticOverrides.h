#pragma once

#include "DiagnosticOverrides.h"

#include <QObject>

namespace omaweb {

// The run's Diagnostic overrides, set on every Engine profile QtWebEngine
// runs: a Space's, an Agent Space's and the Private windows' shared one alike.
// The user agent, the client hints and the language are a profile's, held by
// the engine in memory for as long as the profile lives.
class QtDiagnosticOverrides final : public QObject {
    Q_OBJECT

public:
    explicit QtDiagnosticOverrides(DiagnosticOverrides overrides, QObject *parent = nullptr);

public slots:
    // Called for every profile Content blocking attaches to, after it has set
    // the reader's language, so an override of it is the one that stands.
    void attachToProfile(QObject *profile);

private:
    DiagnosticOverrides m_overrides;
};

} // namespace omaweb
