#include "DiagnosticOverrides.h"

#include "AgentCommand.h"
#include "LaunchRequest.h"

#include <QTest>

using omaweb::readDiagnosticOverrides;

namespace {

// What QtWebEngine 6.11 presents on Linux, with its token already dropped.
const auto engineAgent = QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                                        "(KHTML, like Gecko) Chrome/140.0.7339.207 Safari/537.36");
const QVariantMap engineVersions {
    {QStringLiteral("Chromium"), QStringLiteral("140.0.7339.207")},
    {QStringLiteral("Not=A?Brand"), QStringLiteral("24.0.0.0")},
};

} // namespace

class DiagnosticOverridesTest final : public QObject {
    Q_OBJECT

private slots:
    void anOrdinaryLaunchHasNone();
    void readsEachOverrideInEitherSpelling();
    void aDiagnosedLaunchIsALaunch();
    void refusesAnUnknownNameAndListsTheOverrides();
    void refusesAMalformedOverride_data();
    void refusesAMalformedOverride();
    void refusesAnOverrideNamedTwice();
    void leavesWhatSitesSeeAloneWithoutOverrides();
    void reportsTheNamedChromeEverywhere();
    void sendsTheBrandListGiven();
    void tellsSitesTheOneLanguage();
};

void DiagnosticOverridesTest::anOrdinaryLaunchHasNone()
{
    const auto overrides = readDiagnosticOverrides(
        {QStringLiteral("omaweb"), QStringLiteral("https://example.com/")});
    QVERIFY(overrides.empty());
    QVERIFY(overrides.refusal.isEmpty());
    QVERIFY(!overrides.reportedChrome);
    QVERIFY(!overrides.brands);
    QVERIFY(!overrides.language);
}

// `--diagnose <name>=<value>` as two words or as one, as often as there are
// overrides, beside the address and the other switches a launch takes.
void DiagnosticOverridesTest::readsEachOverrideInEitherSpelling()
{
    const auto overrides = readDiagnosticOverrides({
        QStringLiteral("omaweb"),
        QStringLiteral("--diagnose"),
        QStringLiteral("report-chrome=152"),
        QStringLiteral("--private-window"),
        QStringLiteral("--diagnose=brands=Chromium, Google Chrome,Not=A?Brand"),
        QStringLiteral("https://example.com/"),
        QStringLiteral("--diagnose"),
        QStringLiteral("language=fi-FI"),
    });
    QVERIFY2(overrides.refusal.isEmpty(), qPrintable(overrides.refusal));
    QVERIFY(!overrides.empty());
    QCOMPARE(overrides.reportedChrome, 152);
    QCOMPARE(overrides.brands,
        QStringList({QStringLiteral("Chromium"), QStringLiteral("Google Chrome"),
            QStringLiteral("Not=A?Brand")}));
    QCOMPARE(overrides.language, QStringLiteral("fi-FI"));
    QCOMPARE(overrides.named,
        QStringList({QStringLiteral("report-chrome=152"),
            QStringLiteral("brands=Chromium,Google Chrome,Not=A?Brand"),
            QStringLiteral("language=fi-FI")}));
}

// The browser reads the launch, not the client: no verb, no option of a
// verb's, and an override is not the address to open.
void DiagnosticOverridesTest::aDiagnosedLaunchIsALaunch()
{
    const QStringList arguments {QStringLiteral("omaweb"), QStringLiteral("--diagnose"),
        QStringLiteral("report-chrome=152"), QStringLiteral("--diagnose"),
        QStringLiteral("brands=Not:A-Brand,Chromium"), QStringLiteral("https://example.com/")};
    QVERIFY(!omaweb::isAgentCommand(arguments));
    QVERIFY(!omaweb::isAgentHelpCommand(arguments));
    QVERIFY(omaweb::misplacedAgentOption(arguments).isEmpty());
    QCOMPARE(omaweb::readLaunchUrl(arguments), QUrl(QStringLiteral("https://example.com/")));
    QVERIFY(omaweb::readLaunchUrl(arguments.mid(0, 5)).isEmpty());
}

void DiagnosticOverridesTest::refusesAnUnknownNameAndListsTheOverrides()
{
    const auto overrides = readDiagnosticOverrides({QStringLiteral("omaweb"),
        QStringLiteral("--diagnose"), QStringLiteral("platform=Windows")});
    QVERIFY(overrides.refusal.contains(QStringLiteral("platform")));
    for (const auto *form : {"report-chrome=<major>", "brands=<list>", "language=<tag>"}) {
        QVERIFY2(overrides.refusal.contains(QLatin1String(form)), qPrintable(overrides.refusal));
    }
}

void DiagnosticOverridesTest::refusesAMalformedOverride_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::newRow("no override after the switch") << QStringList {QStringLiteral("--diagnose")};
    QTest::newRow("an empty override") << QStringList {QStringLiteral("--diagnose=")};
    QTest::newRow("a name without a value")
        << QStringList {QStringLiteral("--diagnose"), QStringLiteral("report-chrome")};
    QTest::newRow("a version that is not a number")
        << QStringList {QStringLiteral("--diagnose=report-chrome=latest")};
    QTest::newRow("a full version")
        << QStringList {QStringLiteral("--diagnose=report-chrome=152.0")};
    QTest::newRow("version zero") << QStringList {QStringLiteral("--diagnose=report-chrome=0")};
    QTest::newRow("no brands") << QStringList {QStringLiteral("--diagnose=brands=")};
    QTest::newRow("an empty brand")
        << QStringList {QStringLiteral("--diagnose=brands=Chromium,,Google Chrome")};
    QTest::newRow("a brand with a quote")
        << QStringList {QStringLiteral("--diagnose=brands=Chromium,\"Google Chrome\"")};
    QTest::newRow("a brand named twice")
        << QStringList {QStringLiteral("--diagnose=brands=Chromium,Chromium")};
    QTest::newRow("a locale rather than a tag")
        << QStringList {QStringLiteral("--diagnose=language=fi_FI")};
    QTest::newRow("a list of languages")
        << QStringList {QStringLiteral("--diagnose=language=fi-FI,en")};
    QTest::newRow("no language") << QStringList {QStringLiteral("--diagnose=language=")};
}

void DiagnosticOverridesTest::refusesAMalformedOverride()
{
    QFETCH(QStringList, arguments);
    arguments.prepend(QStringLiteral("omaweb"));
    const auto overrides = readDiagnosticOverrides(arguments);
    QVERIFY(!overrides.refusal.isEmpty());
    QVERIFY2(overrides.refusal.contains(QStringLiteral("report-chrome=<major>")),
        qPrintable(overrides.refusal));
}

void DiagnosticOverridesTest::refusesAnOverrideNamedTwice()
{
    const auto overrides = readDiagnosticOverrides(
        {QStringLiteral("omaweb"), QStringLiteral("--diagnose=report-chrome=152"),
            QStringLiteral("--diagnose=report-chrome=153")});
    QVERIFY(overrides.refusal.contains(QStringLiteral("report-chrome")));
}

void DiagnosticOverridesTest::leavesWhatSitesSeeAloneWithoutOverrides()
{
    const omaweb::DiagnosticOverrides none;
    QCOMPARE(omaweb::overriddenUserAgent(engineAgent, none), engineAgent);
    QCOMPARE(omaweb::overriddenFullVersionList(engineVersions, none), engineVersions);
    QCOMPARE(omaweb::overriddenFullVersion(QStringLiteral("140.0.7339.207"), none),
        QStringLiteral("140.0.7339.207"));
    QCOMPARE(
        omaweb::overriddenAcceptLanguage(QStringLiteral("fi-FI"), none), QStringLiteral("fi-FI"));
}

// Chrome's own user agent carries only its major version, the rest zeros, and
// so does every brand at Chromium's version here. The engine's other brand,
// the made-up one Chrome adds so sites do not match the list exactly, keeps
// its own.
void DiagnosticOverridesTest::reportsTheNamedChromeEverywhere()
{
    const auto overrides = readDiagnosticOverrides(
        {QStringLiteral("omaweb"), QStringLiteral("--diagnose=report-chrome=152")});
    QCOMPARE(omaweb::overriddenUserAgent(engineAgent, overrides),
        QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                       "Chrome/152.0.0.0 Safari/537.36"));
    QCOMPARE(omaweb::overriddenFullVersionList(engineVersions, overrides),
        QVariantMap({
            {QStringLiteral("Chromium"), QStringLiteral("152.0.0.0")},
            {QStringLiteral("Not=A?Brand"), QStringLiteral("24.0.0.0")},
        }));
    QCOMPARE(omaweb::overriddenFullVersion(QStringLiteral("140.0.7339.207"), overrides),
        QStringLiteral("152.0.0.0"));
    QCOMPARE(omaweb::overriddenAcceptLanguage(QStringLiteral("fi-FI"), overrides),
        QStringLiteral("fi-FI"));
}

// The brands named are the brands sent: one the engine has keeps its version,
// one it lacks takes Chromium's, and one not named is not sent.
void DiagnosticOverridesTest::sendsTheBrandListGiven()
{
    const auto added = readDiagnosticOverrides({QStringLiteral("omaweb"),
        QStringLiteral("--diagnose=brands=Chromium,Google Chrome,Not=A?Brand")});
    QCOMPARE(omaweb::overriddenFullVersionList(engineVersions, added),
        QVariantMap({
            {QStringLiteral("Chromium"), QStringLiteral("140.0.7339.207")},
            {QStringLiteral("Google Chrome"), QStringLiteral("140.0.7339.207")},
            {QStringLiteral("Not=A?Brand"), QStringLiteral("24.0.0.0")},
        }));
    QCOMPARE(omaweb::overriddenUserAgent(engineAgent, added), engineAgent);

    const auto reported = readDiagnosticOverrides(
        {QStringLiteral("omaweb"), QStringLiteral("--diagnose=brands=Google Chrome"),
            QStringLiteral("--diagnose=report-chrome=152")});
    QCOMPARE(omaweb::overriddenFullVersionList(engineVersions, reported),
        QVariantMap({{QStringLiteral("Google Chrome"), QStringLiteral("152.0.0.0")}}));
}

void DiagnosticOverridesTest::tellsSitesTheOneLanguage()
{
    const auto overrides = readDiagnosticOverrides(
        {QStringLiteral("omaweb"), QStringLiteral("--diagnose"), QStringLiteral("language=sv-FI")});
    QCOMPARE(omaweb::overriddenAcceptLanguage(QStringLiteral("fi-FI"), overrides),
        QStringLiteral("sv-FI"));
    QCOMPARE(omaweb::overriddenUserAgent(engineAgent, overrides), engineAgent);
    QCOMPARE(omaweb::overriddenFullVersionList(engineVersions, overrides), engineVersions);
}

QTEST_GUILESS_MAIN(DiagnosticOverridesTest)

#include "tst_diagnosticoverrides.moc"
