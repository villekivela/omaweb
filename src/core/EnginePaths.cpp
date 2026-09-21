#include "EnginePaths.h"

#include <QDir>

namespace omaweb {

EnginePaths EnginePaths::beside(const QString &engineLibraryDirectory)
{
    if (engineLibraryDirectory.isEmpty()) {
        return {};
    }
    // The layout `cmake --install` writes: the libraries in `lib`, the helper
    // in `lib/qt6`, and everything the engine reads under `share/qt6`.
    const QDir libraries(engineLibraryDirectory);
    const QDir prefix(libraries.filePath(QStringLiteral("..")));
    const QString share = prefix.filePath(QStringLiteral("share/qt6"));
    return EnginePaths {
        QDir::cleanPath(share + QStringLiteral("/resources")),
        QDir::cleanPath(share + QStringLiteral("/translations/qtwebengine_locales")),
        QDir::cleanPath(libraries.filePath(QStringLiteral("qt6/QtWebEngineProcess"))),
    };
}

} // namespace omaweb
