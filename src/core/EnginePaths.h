#pragma once

#include <QString>

namespace omaweb {

// Where the engine keeps the files it cannot start without, when the engine is
// not the one the system supplies.
//
// QtWebEngine finds its resource packs, its ICU table and its renderer helper
// through *QtCore's* idea of where Qt was installed. Omaweb's own engine is
// installed beside the system Qt rather than over it, precisely so every other
// Qt application keeps the engine it had, and the consequence is that QtCore
// points the engine at the distribution's directories, where an engine that
// bundles its own ICU finds nothing. It dies before the first window with
// "Invalid file descriptor to ICU data received".
//
// So Omaweb says where its engine's files are, before the engine looks. This
// is only ever true of a private prefix: an engine installed as part of Qt is
// already where QtCore says, and nothing here applies.
struct EnginePaths {
    QString resources;
    QString locales;
    QString renderer;

    // Empty when `engineLibraryDirectory` is empty, which is what a platform
    // that cannot say where its engine is reports.
    static EnginePaths beside(const QString &engineLibraryDirectory);
};

} // namespace omaweb
