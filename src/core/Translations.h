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

// The locale `requestedLocale` returns and the environment variable that chose it, so
// Settings can say why the chrome speaks as it does. The variable is empty when none was
// set and the system's locale answered.
struct LocaleChoice {
    QLocale locale;
    QString variable;
};
LocaleChoice localeChoice();

// A locale named by its own language and its code, `Suomi (fi_FI)`. The C locale is English.
QString localeTitle(const QLocale &locale);

// The languages a reader can have the chrome in: English, which is the source text, then
// each catalogue found in `directories`, named in its own language and without duplicates.
QStringList shippedLanguages(const QStringList &directories);

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
