#pragma once

#include <QString>

namespace omaweb {

// Says where the engine this process loaded keeps its own files, before the
// engine asks QtCore and is told where the distribution's Qt keeps files this
// engine did not put there. See EnginePaths. Call before
// `QtWebEngineQuick::initialize()`.
//
// Answers the directory of the engine's own QtWebEngine QML module, which a
// QML engine has to search ahead of Qt's own directory. Empty for an engine
// that is part of Qt, and on a platform that cannot say where its engine is.
QString announceQtEnginePaths();

} // namespace omaweb
