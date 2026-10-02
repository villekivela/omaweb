#include "Translations.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>

namespace {

// The catalogue the build compiled, which the tests copy into directories of
// their own so a test chooses where each one is found.
QString compiledFinnishCatalogue()
{
    return QDir(QStringLiteral(OMAWEB_TRANSLATIONS_DIRECTORY))
        .filePath(QStringLiteral("omaweb_fi.qm"));
}

bool provide(const QTemporaryDir &directory)
{
    return QFile::copy(
        compiledFinnishCatalogue(), directory.filePath(QStringLiteral("omaweb_fi.qm")));
}

void clearLocaleEnvironment()
{
    qunsetenv("LC_ALL");
    qunsetenv("LC_MESSAGES");
    qunsetenv("LANG");
}

} // namespace

class TranslationsTests final : public QObject {
    Q_OBJECT

private slots:
    void init() { clearLocaleEnvironment(); }
    void cleanup() { clearLocaleEnvironment(); }

    void lcAllOutranksLcMessagesAndLang();
    void lcMessagesOutranksLang();
    void langIsReadWhenNothingOutranksIt();
    void aCodesetAndRegionAreAcceptedInTheLocaleName();
    void theRegionlessCatalogueAnswersARegionalLocale();
    void theFirstDirectoryWithACatalogueWins();
    void aLocaleWithoutACatalogueInstallsNothing();
    void theInstalledDirectoryComesBeforeTheBuildTree();
};

void TranslationsTests::lcAllOutranksLcMessagesAndLang()
{
    qputenv("LC_ALL", "fi_FI.UTF-8");
    qputenv("LC_MESSAGES", "de_DE.UTF-8");
    qputenv("LANG", "sv_SE.UTF-8");
    QCOMPARE(omaweb::requestedLocale().language(), QLocale::Finnish);
}

void TranslationsTests::lcMessagesOutranksLang()
{
    qputenv("LC_MESSAGES", "de_DE.UTF-8");
    qputenv("LANG", "fi_FI.UTF-8");
    QCOMPARE(omaweb::requestedLocale().language(), QLocale::German);
}

void TranslationsTests::langIsReadWhenNothingOutranksIt()
{
    qputenv("LANG", "sv_SE.UTF-8");
    QCOMPARE(omaweb::requestedLocale().language(), QLocale::Swedish);
}

void TranslationsTests::aCodesetAndRegionAreAcceptedInTheLocaleName()
{
    qputenv("LANG", "fi_FI.UTF-8");
    const auto locale = omaweb::requestedLocale();
    QCOMPARE(locale.language(), QLocale::Finnish);
    QCOMPARE(locale.territory(), QLocale::Finland);
}

void TranslationsTests::theRegionlessCatalogueAnswersARegionalLocale()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(provide(directory));
    const auto *translator = omaweb::installCatalogue(
        QCoreApplication::instance(), QLocale(QStringLiteral("fi_FI")), {directory.path()});
    QVERIFY(translator != nullptr);
    QCOMPARE(QFileInfo(translator->filePath()).fileName(), QStringLiteral("omaweb_fi.qm"));
    QCOMPARE(QCoreApplication::translate("StartPage", "shortcuts"), QStringLiteral("pikanäppäimet"));
    delete translator;
    QCOMPARE(QCoreApplication::translate("StartPage", "shortcuts"), QStringLiteral("shortcuts"));
}

void TranslationsTests::theFirstDirectoryWithACatalogueWins()
{
    QTemporaryDir empty;
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(empty.isValid() && first.isValid() && second.isValid());
    QVERIFY(provide(first));
    QVERIFY(provide(second));
    const auto *translator = omaweb::installCatalogue(QCoreApplication::instance(),
        QLocale(QStringLiteral("fi")), {empty.path(), first.path(), second.path()});
    QVERIFY(translator != nullptr);
    QCOMPARE(QFileInfo(translator->filePath()).absolutePath(), first.path());
    delete translator;
}

void TranslationsTests::aLocaleWithoutACatalogueInstallsNothing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(provide(directory));
    QVERIFY(omaweb::installCatalogue(
                QCoreApplication::instance(), QLocale(QStringLiteral("de_DE")), {directory.path()})
        == nullptr);
    QCOMPARE(QCoreApplication::translate("StartPage", "shortcuts"), QStringLiteral("shortcuts"));
}

void TranslationsTests::theInstalledDirectoryComesBeforeTheBuildTree()
{
    const auto directories = omaweb::catalogueDirectories(QStringLiteral("/build/tree"));
    QCOMPARE(directories.size(), 2);
    QVERIFY(directories.first().endsWith(QStringLiteral("../share/omaweb/translations")));
    QCOMPARE(directories.last(), QStringLiteral("/build/tree"));
    QCOMPARE(omaweb::catalogueDirectories({}).size(), 1);
}

QTEST_MAIN(TranslationsTests)
#include "tst_translations.moc"
