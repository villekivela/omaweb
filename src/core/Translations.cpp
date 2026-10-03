#include "Translations.h"

#include <QDir>
#include <QTranslator>

namespace omaweb {

LocaleChoice localeChoice()
{
    for (const auto *name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const auto value = qEnvironmentVariable(name);
        if (!value.isEmpty()) {
            // QLocale reads `fi_FI.UTF-8` and `fi_FI@euro` as `fi_FI`.
            return {QLocale(value), QString::fromLatin1(name)};
        }
    }
    return {QLocale::system(), {}};
}

QLocale requestedLocale() { return localeChoice().locale; }

namespace {

    QString capitalized(QString name)
    {
        if (!name.isEmpty()) {
            name[0] = name.at(0).toUpper();
        }
        return name;
    }

    QString languageName(const QLocale &locale)
    {
        // Qt names English "American English" or "British English" by region, and the C locale
        // the same, where the catalogue's source language is simply English.
        return locale.language() == QLocale::C || locale.language() == QLocale::English
            ? QStringLiteral("English")
            : capitalized(locale.nativeLanguageName());
    }

} // namespace

QString localeTitle(const QLocale &locale)
{
    return QStringLiteral("%1 (%2)").arg(languageName(locale), locale.name());
}

QStringList shippedLanguages(const QStringList &directories)
{
    QStringList languages {QStringLiteral("English")};
    for (const auto &directory : directories) {
        for (const auto &file : QDir(directory).entryList({QStringLiteral("omaweb_*.qm")})) {
            const auto code = file.mid(7, file.size() - 7 - 3);
            const auto name = languageName(QLocale(code));
            if (!languages.contains(name)) {
                languages.append(name);
            }
        }
    }
    return languages;
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
    // An installed copy first, so a stale catalogue left in a build directory
    // the package was made from never shadows the one it shipped.
    QStringList directories {QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../share/omaweb/translations"))};
    if (!buildTreeDirectory.isEmpty()) {
        directories.append(buildTreeDirectory);
    }
    return directories;
}

} // namespace omaweb
