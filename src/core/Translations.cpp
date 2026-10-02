#include "Translations.h"

#include <QDir>
#include <QTranslator>

namespace omaweb {

QLocale requestedLocale()
{
    for (const auto *name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const auto value = qEnvironmentVariable(name);
        if (!value.isEmpty()) {
            // QLocale reads `fi_FI.UTF-8` and `fi_FI@euro` as `fi_FI`.
            return QLocale(value);
        }
    }
    return QLocale::system();
}

QTranslator *installCatalogue(
    QCoreApplication *application, const QLocale &locale, const QStringList &directories)
{
    for (const auto &directory : directories) {
        auto *translator = new QTranslator(application);
        if (translator->load(locale, QStringLiteral("omaweb"), QStringLiteral("_"), directory)
            && QCoreApplication::installTranslator(translator)) {
            return translator;
        }
        delete translator;
    }
    return nullptr;
}

QStringList catalogueDirectories(const QString &buildTreeDirectory)
{
    QStringList directories;
    if (!buildTreeDirectory.isEmpty()) {
        directories.append(buildTreeDirectory);
    }
    directories.append(QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../share/omaweb/translations")));
    return directories;
}

} // namespace omaweb
