#pragma once

#include <QObject>

class QWebEngineSettings;

namespace omaweb {

// The `QWebEngineSettings` a Quick profile carries, which is the root every
// view of the profile inherits from and which Qt does not hand out publicly
// (ADR 0047). This is the one place Omaweb reaches past the engine's public
// API for it, and the one place that says whether it can: a Qt other than
// the one this was compiled against is not reached into.
namespace QtProfileSettings {

    bool reachable();
    // Null when the object is not a Quick profile or the build cannot reach
    // its settings.
    QWebEngineSettings *of(QObject *profileObject);

} // namespace QtProfileSettings

} // namespace omaweb
