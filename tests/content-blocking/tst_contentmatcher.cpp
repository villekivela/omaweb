#include "ContentMatcher.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

using tanto::ContentMatcher;

class ContentMatcherTest final : public QObject {
    Q_OBJECT

private slots:
    void sharedConformanceFixtures();
    void sharedSurveyFixtures();
    void scriptletsCallTheLibraryFunctionTheRuleNames();
    void scriptletExceptionsSurviveTheirArgumentCount();
    void reportsTheScriptletRulesThatWillNotRun();
    void scriptletArgumentsAreNotReadAsSelectorSyntax();
    void sendsOnlyTheGenericRulesAPageCouldTrigger();
    void reportsUnsupportedCategories();
    void popupRulesKeepTheirOtherConditions();
    void negatedPopupRulesStayOrdinaryRules();
};

void ContentMatcherTest::sharedConformanceFixtures()
{
    QFile fixture(QStringLiteral(TANTO_CONTENT_BLOCKER_FIXTURES));
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    const auto document = QJsonDocument::fromJson(fixture.readAll());
    QVERIFY(document.isObject());
    const auto root = document.object();
    QStringList rules;
    for (const auto &rule : root.value(QStringLiteral("rules")).toArray()) {
        rules.append(rule.toString());
    }
    const auto compilation = ContentMatcher::compile(rules.join(QLatin1Char('\n')));
    QVERIFY(compilation.matcher);

    for (const auto &value : root.value(QStringLiteral("network")).toArray()) {
        const auto fixture = value.toObject();
        const auto blocked = compilation.matcher->shouldBlock(
            QUrl(fixture.value(QStringLiteral("url")).toString()),
            QUrl(fixture.value(QStringLiteral("source")).toString()),
            fixture.value(QStringLiteral("type")).toString());
        QCOMPARE(blocked, fixture.value(QStringLiteral("blocked")).toBool());
    }
    for (const auto &value : root.value(QStringLiteral("popup")).toArray()) {
        const auto fixture = value.toObject();
        const auto blocked = compilation.matcher->shouldBlockPopup(
            QUrl(fixture.value(QStringLiteral("url")).toString()),
            QUrl(fixture.value(QStringLiteral("opener")).toString()));
        QCOMPARE(blocked, fixture.value(QStringLiteral("blocked")).toBool());
    }
    for (const auto &value : root.value(QStringLiteral("scriptlet")).toArray()) {
        const auto fixture = value.toObject();
        const auto source = compilation.matcher->scriptletSource(
            QUrl(fixture.value(QStringLiteral("url")).toString()));
        QCOMPARE(source.contains(fixture.value(QStringLiteral("contains")).toString()),
            fixture.value(QStringLiteral("injected")).toBool());
    }
    for (const auto &value : root.value(QStringLiteral("cosmetic")).toArray()) {
        const auto fixture = value.toObject();
        const auto css = compilation.matcher->cosmeticStyleSheet(
            QUrl(fixture.value(QStringLiteral("url")).toString()));
        QCOMPARE(css.contains(fixture.value(QStringLiteral("contains")).toString()),
            fixture.value(QStringLiteral("hidden")).toBool());
    }
}

// A `##+js(...)` rule names a function in the vendored library and the engine
// returns that function's source together with the call. A list never supplies
// the code, which is what keeps a filter list data rather than a program.
void ContentMatcherTest::scriptletsCallTheLibraryFunctionTheRuleNames()
{
    const auto compilation = ContentMatcher::compile(QStringLiteral(
        "site.example##+js(set-constant, tantoScriptletRan, true)"));
    QVERIFY(compilation.matcher);
    const auto source = compilation.matcher->scriptletSource(
        QUrl(QStringLiteral("https://site.example/")));

    // The dependency's source, the scriptlet's own, and the call with the
    // rule's arguments.
    QVERIFY(source.contains(QStringLiteral("function setConstantFn(")));
    QVERIFY(source.contains(QStringLiteral("function setConstant(")));
    QVERIFY(source.contains(QStringLiteral("setConstant(\"tantoScriptletRan\", \"true\")")));
}

// An exception naming the same scriptlet takes it back, and it is a cosmetic
// rule however many arguments it spells out.
void ContentMatcherTest::scriptletExceptionsSurviveTheirArgumentCount()
{
    const auto compilation = ContentMatcher::compile(QStringLiteral(
        "site.example##+js(set-attr, div, hidden, true)\n"
        "site.example#@#+js(set-attr, div, hidden, true)"));
    QVERIFY(compilation.matcher);
    QCOMPARE(compilation.report.value(QStringLiteral("acceptedRuleCount")).toInt(), 2);
    QVERIFY(compilation.report.value(QStringLiteral("unsupported")).toObject().isEmpty());
    QVERIFY(compilation.matcher->scriptletSource(
        QUrl(QStringLiteral("https://site.example/"))).isEmpty());
}

// Generic rules reach a page through the survey, so the fixtures for them read
// the page's classes and ids rather than the stylesheet for its hostname.
void ContentMatcherTest::sharedSurveyFixtures()
{
    QFile fixture(QStringLiteral(TANTO_CONTENT_BLOCKER_FIXTURES));
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    const auto root = QJsonDocument::fromJson(fixture.readAll()).object();
    QStringList rules;
    for (const auto &rule : root.value(QStringLiteral("rules")).toArray()) {
        rules.append(rule.toString());
    }
    const auto compilation = ContentMatcher::compile(rules.join(QLatin1Char('\n')));
    QVERIFY(compilation.matcher);

    const auto stringList = [](const QJsonValue &value) {
        QStringList result;
        for (const auto &entry : value.toArray()) {
            result.append(entry.toString());
        }
        return result;
    };
    for (const auto &value : root.value(QStringLiteral("cosmeticSurveyWanted")).toArray()) {
        const auto fixture = value.toObject();
        QCOMPARE(compilation.matcher->cosmeticSurveyWanted(
                     QUrl(fixture.value(QStringLiteral("url")).toString())),
            fixture.value(QStringLiteral("wanted")).toBool());
    }
    for (const auto &value : root.value(QStringLiteral("cosmeticSurvey")).toArray()) {
        const auto fixture = value.toObject();
        const auto css = compilation.matcher->genericCosmeticStyleSheet(
            QUrl(fixture.value(QStringLiteral("url")).toString()),
            stringList(fixture.value(QStringLiteral("classes"))),
            stringList(fixture.value(QStringLiteral("ids"))));
        QCOMPARE(css.contains(fixture.value(QStringLiteral("contains")).toString()),
            fixture.value(QStringLiteral("hidden")).toBool());
    }
}

// A rule naming a scriptlet this build cannot run is a rule that does nothing,
// and counting it accepted would advertise a compatibility Tanto does not have.
// The two reasons are reported apart: one is a version behind, the other is a
// deliberate refusal.
void ContentMatcherTest::reportsTheScriptletRulesThatWillNotRun()
{
    const auto compilation = ContentMatcher::compile(QStringLiteral(
        "site.example##+js(set-constant, adsShown, false)\n"
        "site.example##+js(trusted-set-cookie, consent, yes)\n"
        "site.example##+js(no-such-scriptlet-anywhere, a)"));
    QVERIFY(compilation.matcher);
    QCOMPARE(compilation.report.value(QStringLiteral("acceptedRuleCount")).toInt(), 1);
    const auto unsupported = compilation.report.value(QStringLiteral("unsupported")).toObject();
    QCOMPARE(unsupported.value(QStringLiteral("scriptlets requiring trust")).toInt(), 1);
    QCOMPARE(
        unsupported.value(QStringLiteral("scriptlets this build does not carry")).toInt(), 1);
    const auto source = compilation.matcher->scriptletSource(
        QUrl(QStringLiteral("https://site.example/")));
    QVERIFY(source.contains(QStringLiteral("adsShown")));
    QVERIFY(!source.contains(QStringLiteral("trustedSetCookie")));
}

// A scriptlet's arguments are that scriptlet's business and may contain
// anything, the markers a procedural selector uses included. Reading them as
// selector syntax would throw the rule away for the shape of its arguments.
void ContentMatcherTest::scriptletArgumentsAreNotReadAsSelectorSyntax()
{
    const auto compilation = ContentMatcher::compile(QStringLiteral(
        "site.example##+js(remove-node-text, script, :has-text(ad))"));
    QVERIFY(compilation.matcher);
    QCOMPARE(compilation.report.value(QStringLiteral("acceptedRuleCount")).toInt(), 1);
    QVERIFY(compilation.report.value(QStringLiteral("unsupported")).toObject().isEmpty());
    QVERIFY(compilation.matcher->scriptletSource(
        QUrl(QStringLiteral("https://site.example/"))).contains(
        QStringLiteral("has-text")));
}

// The survey is what keeps a page from carrying every generic rule in the
// lists: EasyList and EasyPrivacy together hold 13,634 of them.
void ContentMatcherTest::sendsOnlyTheGenericRulesAPageCouldTrigger()
{
    const auto compilation = ContentMatcher::compile(QStringLiteral(
        "##.first-ad\n##.second-ad\n##.third-ad"));
    QVERIFY(compilation.matcher);
    const QUrl url(QStringLiteral("https://site.example/"));

    QVERIFY(compilation.matcher->cosmeticStyleSheet(url).isEmpty());
    const auto css = compilation.matcher->genericCosmeticStyleSheet(
        url, QStringList{QStringLiteral("second-ad")}, {});
    QVERIFY(css.contains(QStringLiteral(".second-ad")));
    QVERIFY(!css.contains(QStringLiteral(".first-ad")));
    QVERIFY(!css.contains(QStringLiteral(".third-ad")));
}

void ContentMatcherTest::reportsUnsupportedCategories()
{
    const auto compilation = ContentMatcher::compile(QStringLiteral(
        "example.com##+js(abort-on-property-read, ad)\n"
        "example.com#?#div:has(.ad)\n"
        "&popunder=$popup\n"
        "||example.com^$redirect=noopjs\n"
        "||safe.example^"));
    QVERIFY(compilation.matcher);
    // The popup and scriptlet rules count among the accepted: both are kept
    // and answered for now.
    QCOMPARE(compilation.report.value(QStringLiteral("acceptedRuleCount")).toInt(), 3);
    const auto unsupported = compilation.report.value(QStringLiteral("unsupported")).toObject();
    QVERIFY(!unsupported.contains(QStringLiteral("scriptlets")));
    QCOMPARE(unsupported.value(QStringLiteral("procedural selectors")).toInt(), 1);
    QVERIFY(!unsupported.contains(QStringLiteral("popup blocking")));
    QCOMPARE(unsupported.value(QStringLiteral("redirects or resource replacement")).toInt(), 1);
}

// A $popup rule carries the same address, party, and domain conditions as any
// other rule, and they have to survive the option being stripped off.
void ContentMatcherTest::popupRulesKeepTheirOtherConditions()
{
    const auto compilation = ContentMatcher::compile(QStringLiteral(
        "&popunder=$popup\n"
        "||ads.example^$popup,domain=site.example"));
    QVERIFY(compilation.matcher);
    const QUrl opener(QStringLiteral("https://site.example/article"));

    QVERIFY(compilation.matcher->shouldBlockPopup(
        QUrl(QStringLiteral("https://tracker.example/go?&popunder=1")), opener));
    QVERIFY(compilation.matcher->shouldBlockPopup(
        QUrl(QStringLiteral("https://ads.example/win")), opener));
    QVERIFY(!compilation.matcher->shouldBlockPopup(QUrl(QStringLiteral("https://ads.example/win")),
        QUrl(QStringLiteral("https://elsewhere.example/article"))));
    QVERIFY(!compilation.matcher->shouldBlockPopup(
        QUrl(QStringLiteral("https://ads.example/win")), QUrl()));
}

// $~popup is "anything but a popup". The engine that answers for ordinary
// requests is never asked about popups, so the option comes off and the rule
// keeps working rather than failing to parse and taking its blocking with it.
void ContentMatcherTest::negatedPopupRulesStayOrdinaryRules()
{
    const auto compilation = ContentMatcher::compile(
        QStringLiteral("||tracker.example^$~popup"));
    QVERIFY(compilation.matcher);
    QCOMPARE(compilation.report.value(QStringLiteral("invalidRuleCount")).toInt(), 0);
    QVERIFY(compilation.matcher->shouldBlock(QUrl(QStringLiteral("https://tracker.example/p")),
        QUrl(QStringLiteral("https://site.example/")), QStringLiteral("image")));
    QVERIFY(!compilation.matcher->shouldBlockPopup(
        QUrl(QStringLiteral("https://tracker.example/p")),
        QUrl(QStringLiteral("https://site.example/"))));
}

QTEST_APPLESS_MAIN(ContentMatcherTest)

#include "tst_contentmatcher.moc"
