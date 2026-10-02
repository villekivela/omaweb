#pragma once

#include <QCoreApplication>
#include <QLocale>
#include <QStringList>

class QTranslator;

namespace omaweb {

// The locale the reader asked for, read the way a POSIX program reads it: the
// first of LC_ALL, LC_MESSAGES and LANG that is set. Qt reads these itself on
// Linux and ignores them on macOS, where the system's language wins; reading
// them here makes the two behave alike and lets a test pick a locale per run.
// Falls back to the system locale when none is set.
QLocale requestedLocale();

// Installs the catalogue `omaweb_<locale>.qm` from the first directory that
// has one for `locale`, trying the region-less name after the full one. Returns
// null and installs nothing when there is none, which leaves the source
// strings, English, on screen. The translator is parented to `application`.
QTranslator *installCatalogue(
    QCoreApplication *application, const QLocale &locale, const QStringList &directories);

// Where the build tree and an installed copy keep their compiled catalogues,
// in the order to look. The installed one is found relative to the executable, so
// a package's prefix is not baked into the binary, and comes first.
QStringList catalogueDirectories(const QString &buildTreeDirectory);

} // namespace omaweb
