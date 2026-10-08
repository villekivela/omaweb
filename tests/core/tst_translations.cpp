#include "ContentBlocker.h"
#include "RuntimeSecurity.h"
#include "SyncLauncher.h"
#include "Translations.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
    void theVariableThatChoseTheLocaleIsNamed();
    void aLocaleFromNoVariableNamesNone();
    void aLocaleIsTitledInItsOwnLanguageWithItsCode();
    void theShippedLanguagesAreNamedInTheirOwnLanguage();
    void aRefusalFromCppReachesTheUiInFinnish();
    void aLogLineStaysEnglishUnderFinnish();
    void theOnDiskFormatStaysEnglishUnderFinnish();
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
    QCOMPARE(
        QCoreApplication::translate("ShortcutsCue", "shortcuts"), QStringLiteral("pikanäppäimet"));
    delete translator;
    QCOMPARE(QCoreApplication::translate("ShortcutsCue", "shortcuts"), QStringLiteral("shortcuts"));
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
    QCOMPARE(QCoreApplication::translate("ShortcutsCue", "shortcuts"), QStringLiteral("shortcuts"));
}

void TranslationsTests::theInstalledDirectoryComesBeforeTheBuildTree()
{
    const auto directories = omaweb::catalogueDirectories(QStringLiteral("/build/tree"));
    QCOMPARE(directories.size(), 2);
    QVERIFY(directories.first().endsWith(QStringLiteral("../share/omaweb/translations")));
    QCOMPARE(directories.last(), QStringLiteral("/build/tree"));
    QCOMPARE(omaweb::catalogueDirectories({}).size(), 1);
}

void TranslationsTests::theVariableThatChoseTheLocaleIsNamed()
{
    qputenv("LANG", "sv_SE.UTF-8");
    QCOMPARE(omaweb::localeChoice().variable, QStringLiteral("LANG"));
    qputenv("LC_MESSAGES", "de_DE.UTF-8");
    QCOMPARE(omaweb::localeChoice().variable, QStringLiteral("LC_MESSAGES"));
    qputenv("LC_ALL", "fi_FI.UTF-8");
    const auto choice = omaweb::localeChoice();
    QCOMPARE(choice.variable, QStringLiteral("LC_ALL"));
    QCOMPARE(choice.locale.name(), QStringLiteral("fi_FI"));
}

void TranslationsTests::aLocaleFromNoVariableNamesNone()
{
    QCOMPARE(omaweb::localeChoice().variable, QString());
    QCOMPARE(omaweb::localeChoice().locale, QLocale::system());
}

void TranslationsTests::aLocaleIsTitledInItsOwnLanguageWithItsCode()
{
    QCOMPARE(
        omaweb::localeTitle(QLocale(QStringLiteral("fi_FI"))), QStringLiteral("Suomi (fi_FI)"));
    QCOMPARE(
        omaweb::localeTitle(QLocale(QStringLiteral("en_US"))), QStringLiteral("English (en_US)"));
    QCOMPARE(omaweb::localeTitle(QLocale(QStringLiteral("C"))), QStringLiteral("English (C)"));
}

void TranslationsTests::theShippedLanguagesAreNamedInTheirOwnLanguage()
{
    QTemporaryDir empty;
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(empty.isValid() && first.isValid() && second.isValid());
    QVERIFY(provide(first));
    QVERIFY(provide(second));
    QCOMPARE(omaweb::shippedLanguages({empty.path()}), QStringList {QStringLiteral("English")});
    QCOMPARE(omaweb::shippedLanguages({empty.path(), first.path(), second.path()}),
        (QStringList {QStringLiteral("English"), QStringLiteral("Suomi")}));
}

// The UI reads errorMessage as a property, so what it shows is what C++ composed. The same
// refusal under no catalogue is the English source, which the other tests rely on.
void TranslationsTests::aRefusalFromCppReachesTheUiInFinnish()
{
    const auto refusal = [] {
        omaweb::SyncLauncher launcher(nullptr, nullptr, nullptr, {}, {}, {});
        [[maybe_unused]] const auto loaded = launcher.load();
        return launcher.errorMessage();
    };
    QCOMPARE(refusal(), QStringLiteral("The Sync Feature module is not installed"));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(provide(directory));
    const auto *translator = omaweb::installCatalogue(
        QCoreApplication::instance(), QLocale(QStringLiteral("fi_FI")), {directory.path()});
    QVERIFY(translator != nullptr);
    QCOMPARE(refusal(), QStringLiteral("Synkronointimoduulia ei ole asennettu"));
    delete translator;
}

// The startup refusal is printed as a log line. It reads in the reader's language only where the
// reader is meant to read it.
void TranslationsTests::aLogLineStaysEnglishUnderFinnish()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(provide(directory));
    const auto *translator = omaweb::installCatalogue(
        QCoreApplication::instance(), QLocale(QStringLiteral("fi_FI")), {directory.path()});
    QVERIFY(translator != nullptr);
    const omaweb::SandboxHost superuser {QStringLiteral("/proc"), true};
    const auto english = QStringLiteral(
        "Omaweb is running as the superuser. Chromium will not sandbox a renderer as "
        "root, and Omaweb does not run renderers without a sandbox. Start Omaweb as an "
        "ordinary user.");
    QCOMPARE(omaweb::sandboxDiagnostic(superuser, false), english);
    QVERIFY(omaweb::sandboxDiagnostic(superuser) != english);
    delete translator;
}

// The Content blocker keeps its update status as an English code in settings.json, and only the
// report to the UI is translated.
void TranslationsTests::theOnDiskFormatStaysEnglishUnderFinnish()
{
    QTemporaryDir directory;
    QTemporaryDir data;
    QVERIFY(directory.isValid() && data.isValid());
    QVERIFY(provide(directory));
    const auto *translator = omaweb::installCatalogue(
        QCoreApplication::instance(), QLocale(QStringLiteral("fi_FI")), {directory.path()});
    QVERIFY(translator != nullptr);
    {
        omaweb::ContentBlocker blocker(data.path(), omaweb::ContentBlocker::DefaultLists::Seed);
        const auto shown = blocker.subscriptions().first().toMap();
        QCOMPARE(shown.value(QStringLiteral("updateStatus")).toString(),
            QStringLiteral("ei päivitetty"));
    }
    QFile file(QDir(data.path()).filePath(QStringLiteral("content-blocking/settings.json")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto stored = file.readAll();
    QVERIFY(stored.contains("\"updateStatus\": \"not updated\"")
        || stored.contains("\"updateStatus\":\"not updated\""));
    QVERIFY(!stored.contains("ei päivitetty"));
    delete translator;
}

QTEST_MAIN(TranslationsTests)
#include "tst_translations.moc"
