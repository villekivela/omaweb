#include "QtEnginePaths.h"

#include "EnginePaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

#if defined(Q_OS_LINUX)
#include <dlfcn.h>
#endif

namespace omaweb {

QString announceQtEnginePaths()
{
#if defined(Q_OS_LINUX)
    // The engine is found by asking the loader where it actually mapped it
    // from, so this is right for an installed tree, a build tree, and a reader
    // who moved it, without a path compiled in.
    Dl_info engineLibrary {};
    const bool located
        = dladdr(reinterpret_cast<const void *>(&qWebEngineVersion), &engineLibrary) != 0
        && engineLibrary.dli_fname != nullptr;
    const QString libraryDirectory = located
        ? QFileInfo(QString::fromLocal8Bit(engineLibrary.dli_fname)).absolutePath()
        : QString {};
    const auto paths = EnginePaths::beside(libraryDirectory);
    // An engine that is part of the Qt it was built against is already where
    // QtCore will look, and saying so again would only be a way to get it
    // wrong. That is the engine in Qt's own library directory, not one
    // anywhere under it: /usr/lib/omaweb/lib is under /usr/lib.
    const bool privatePrefix = !libraryDirectory.isEmpty()
        && QDir::cleanPath(libraryDirectory)
            != QDir::cleanPath(QLibraryInfo::path(QLibraryInfo::LibrariesPath));
    if (!privatePrefix) {
        return {};
    }
    // Each one only if it is there, and never over the reader: a missing file
    // is the packaging's fault and pointing the engine at nothing would
    // replace a clear failure with a confusing one.
    const auto say = [](const char *name, const QString &path, bool isDirectory) {
        if (qEnvironmentVariableIsSet(name)) {
            return;
        }
        const QFileInfo there(path);
        if (isDirectory ? there.isDir() : there.isExecutable()) {
            qputenv(name, QFile::encodeName(path));
        }
    };
    say("QTWEBENGINE_RESOURCES_PATH", paths.resources, true);
    say("QTWEBENGINE_LOCALES_PATH", paths.locales, true);
    say("QTWEBENGINEPROCESS_PATH", paths.renderer, false);
    return QFileInfo(paths.qml).isDir() ? paths.qml : QString {};
#else
    return {};
#endif
}

} // namespace omaweb
