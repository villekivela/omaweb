#include "LinkRules.h"
#include "SessionFixture.h"
#include "SettingsFile.h"
#include "WindowManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::BrowserController;
using omaweb::LinkRule;
using omaweb::SettingsFile;
using omaweb::WindowManager;
using omaweb::test::SessionFixture;
using omaweb::test::SessionSpec;
using omaweb::test::SpaceSpec;
using omaweb::test::TabSpec;
namespace linkrules = omaweb::linkrules;

class LinkRulesTest final : public QObject {
    Q_OBJECT

private slots:
    void takesASiteAndEverySiteBelowIt();
    void takesThePathsUnderAPrefix();
    void acceptsOnlyAHostAndAPathPrefix();
    void sendsALinkWhereTheFirstRuleThatTakesItSays();
    void opensALinkFromAnotherAppInTheSpaceItsRuleNames();
    void opensAPrivateRulesLinkInThePrivateWindowShownLast();
    void opensWhereItWouldHaveGoneWhenTheSpaceIsNotTheReaders();
    void leavesALinkNoRuleTakesInTheSpaceOnShow();
    void followsTheSpaceItNamesThroughARename();
    void goesWithTheSpaceItNames();
    void leavesTheBrowsersOwnLinksWhereTheyOpen();
    void followsAHandEditWhileItRuns();
};

namespace {

QUrl address(const char *text) { return QUrl(QString::fromLatin1(text)); }

QJsonArray rules(std::initializer_list<std::pair<const char *, const char *>> list)
{
    QList<LinkRule> written;
    for (const auto &[pattern, space] : list) {
        written.append(
            {.pattern = QString::fromLatin1(pattern), .space = QString::fromLatin1(space)});
    }
    return linkrules::write(written);
}

// Personal on show, with Work beside it.
SessionSpec twoSpaces()
{
    return SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = QStringLiteral("personal"),
                .name = QStringLiteral("Personal"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("personal-tab"),
                    .url = QUrl(QStringLiteral("https://personal.example/")),
                }},
            },
            SpaceSpec {
                .id = QStringLiteral("work"),
                .name = QStringLiteral("Work"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("work-tab"),
                    .url = QUrl(QStringLiteral("https://docs.acme.com/x")),
                }},
            },
        },
        .activeSpaceId = QStringLiteral("personal"),
    };
}

QStringList tabAddresses(const BrowserController &browser)
{
    QStringList addresses;
    for (const auto &tab : browser.spaceTabs(browser.activeSpaceId())) {
        addresses.append(tab.url.toString());
    }
    return addresses;
}

} // namespace

// `*.acme.com` is acme.com and everything under it, and nothing that only
// ends in the same letters.
void LinkRulesTest::takesASiteAndEverySiteBelowIt()
{
    const auto pattern = QStringLiteral("*.acme.com");
    QVERIFY(linkrules::matches(pattern, address("https://docs.acme.com/x")));
    QVERIFY(linkrules::matches(pattern, address("https://a.b.acme.com/")));
    QVERIFY(linkrules::matches(pattern, address("https://acme.com")));
    QVERIFY(linkrules::matches(pattern, address("http://ACME.com/y")));
    QVERIFY(!linkrules::matches(pattern, address("https://notacme.com/")));
    QVERIFY(!linkrules::matches(pattern, address("https://acme.com.evil.example/")));
    // A plain host is that host alone.
    QVERIFY(linkrules::matches(QStringLiteral("acme.com"), address("https://acme.com/z")));
    QVERIFY(!linkrules::matches(QStringLiteral("acme.com"), address("https://docs.acme.com/")));
    // A host outside ASCII is written as the DNS writes it.
    QVERIFY(linkrules::matches(
        QStringLiteral("*.xn--bcher-kva.de"), QUrl(QStringLiteral("https://www.bücher.de/"))));
}

// `github.com/acme/*` is the organisation's pages, its own page among them,
// and not another organisation whose name starts the same way.
void LinkRulesTest::takesThePathsUnderAPrefix()
{
    const auto pattern = QStringLiteral("github.com/acme/*");
    QVERIFY(linkrules::matches(pattern, address("https://github.com/acme/repo/pull/1")));
    QVERIFY(linkrules::matches(pattern, address("https://github.com/acme/")));
    QVERIFY(linkrules::matches(pattern, address("https://github.com/acme")));
    QVERIFY(!linkrules::matches(pattern, address("https://github.com/acmeco/repo")));
    QVERIFY(!linkrules::matches(pattern, address("https://github.com/other/acme/")));
    QVERIFY(!linkrules::matches(pattern, address("https://gist.github.com/acme/")));
    // Only a page is a link a rule can send somewhere.
    QVERIFY(!linkrules::matches(QStringLiteral("acme.com"), address("mailto:me@acme.com")));
}

void LinkRulesTest::acceptsOnlyAHostAndAPathPrefix()
{
    for (const auto *pattern : {"acme.com", "*.acme.com", "github.com/acme/*", "github.com/*",
             "*.acme.com/docs/*", "localhost", "Acme.COM"}) {
        QVERIFY2(linkrules::parses(QString::fromLatin1(pattern)), pattern);
    }
    for (const auto *pattern : {"", "*", "*.", "acme.*", "a*.acme.com", "https://acme.com",
             "acme.com/docs", "acme.com/*/x", "acme.com:8080", "acme.com/?q=*", "acme .com",
             "acme..com", "*.*.acme.com"}) {
        QVERIFY2(!linkrules::parses(QString::fromLatin1(pattern)), pattern);
    }
    QVERIFY(!linkrules::matches(QStringLiteral("acme.com/docs"), address("https://acme.com/docs")));
}

// The rules as settings.json holds them. The first that takes the address
// wins, a rule whose pattern does not parse is passed over, and an address no
// rule takes goes nowhere in particular.
void LinkRulesTest::sendsALinkWhereTheFirstRuleThatTakesItSays()
{
    const auto rules = linkrules::read(QJsonDocument::fromJson(R"([
        {"pattern": "acme.com/*bad", "space": "Broken"},
        {"pattern": "github.com/acme/*", "space": "Work"},
        {"pattern": "github.com", "space": "Personal"},
        {"pattern": "*.bank.example", "space": "private"}
    ])")
            .array());
    QCOMPARE(rules.size(), 4);

    const auto route = [&rules](const char *text) {
        const auto rule = linkrules::route(rules, address(text));
        return rule ? rule->space : QString();
    };
    QCOMPARE(route("https://github.com/acme/repo"), QStringLiteral("Work"));
    QCOMPARE(route("https://github.com/someone"), QStringLiteral("Personal"));
    QCOMPARE(route("https://online.bank.example/login"), linkrules::privateWindow());
    QCOMPARE(route("https://acme.com/x"), QString());
    QCOMPARE(route("https://elsewhere.example"), QString());

    // What is written is what was read, in the same order.
    QCOMPARE(linkrules::read(linkrules::write(rules)), rules);
    // A value that is not a list of rules holds none.
    QVERIFY(linkrules::read(QJsonValue(QStringLiteral("acme.com"))).isEmpty());
}

// A link from another app that a rule names Work for opens there while
// Personal is on show: Work comes on show, which is what brings the Space
// notice, with the link as a new tab even though Work already has it open.
void LinkRulesTest::opensALinkFromAnotherAppInTheSpaceItsRuleNames()
{
    QTemporaryDir config;
    QVERIFY(SettingsFile(config.path())
            .set(QStringLiteral("link-rules"),
                rules({{"github.com", "Personal"}, {"*.acme.com", "Work"}})));
    SessionFixture fixture(twoSpaces(), config.path());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    WindowManager windows;
    QSignalSpy switched(browser.get(), &BrowserController::activeSpaceChanged);

    QCOMPARE(
        windows.openFromOutside(browser.get(), address("https://docs.acme.com/x")), browser.get());
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("work"));
    QCOMPARE(switched.count(), 1);
    QCOMPARE(tabAddresses(*browser),
        QStringList({QStringLiteral("https://docs.acme.com/x"),
            QStringLiteral("https://docs.acme.com/x")}));
    QCOMPARE(browser->activeUrl(), address("https://docs.acme.com/x"));
    QVERIFY(browser->activeTabId() != QStringLiteral("work-tab"));
}

// `"private"` opens the link in a new Private window when none is open, and
// after that in the one the reader saw last. The main window opens nothing.
void LinkRulesTest::opensAPrivateRulesLinkInThePrivateWindowShownLast()
{
    QTemporaryDir config;
    QVERIFY(SettingsFile(config.path())
            .set(QStringLiteral("link-rules"), rules({{"*.bank.example", "private"}})));
    SessionFixture fixture(twoSpaces(), config.path());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    WindowManager windows(config.path(), true);
    QSignalSpy requested(&windows, &WindowManager::privateWindowRequested);

    auto *first = windows.openFromOutside(browser.get(), address("https://online.bank.example/a"));
    QVERIFY(first);
    QVERIFY(first != browser.get());
    QVERIFY(first->privateBrowsing());
    QCOMPARE(requested.count(), 1);
    QCOMPARE(first->activeUrl(), address("https://online.bank.example/a"));

    windows.openPrivateWindow();
    QCOMPARE(requested.count(), 2);
    auto *second = requested.at(1).value(0).value<BrowserController *>();
    windows.notePrivateWindowShown(first);

    QCOMPARE(
        windows.openFromOutside(browser.get(), address("https://online.bank.example/b")), first);
    QCOMPARE(requested.count(), 2);
    QCOMPARE(first->tabs()->rowCount(), 2);
    QCOMPARE(first->activeUrl(), address("https://online.bank.example/b"));
    QCOMPARE(second->tabs()->rowCount(), 1);

    QCOMPARE(browser->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(tabAddresses(*browser), QStringList {QStringLiteral("https://personal.example/")});
    windows.releasePrivateWindow(first);
    windows.releasePrivateWindow(second);
}

// A rule naming a Space the reader has not got, or an Agent's Space, opens
// the link where it would have gone without the rule, and says so.
void LinkRulesTest::opensWhereItWouldHaveGoneWhenTheSpaceIsNotTheReaders()
{
    QTemporaryDir config;
    QVERIFY(SettingsFile(config.path())
            .set(QStringLiteral("link-rules"),
                rules({{"gone.example", "Clients"}, {"agent.example", "Checks"}})));
    SessionFixture fixture(twoSpaces(), config.path());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    QVERIFY(
        !browser->createAgentSpace(QStringLiteral("Checks"), QStringLiteral("agent")).isEmpty());
    WindowManager windows;
    QSignalSpy missing(browser.get(), &BrowserController::linkRuleSpaceMissing);

    QCOMPARE(
        windows.openFromOutside(browser.get(), address("https://gone.example/")), browser.get());
    QCOMPARE(
        windows.openFromOutside(browser.get(), address("https://agent.example/")), browser.get());
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(tabAddresses(*browser),
        QStringList({QStringLiteral("https://personal.example/"),
            QStringLiteral("https://gone.example/"), QStringLiteral("https://agent.example/")}));
    QCOMPARE(missing.count(), 2);
    QCOMPARE(missing.at(0).at(0).toString(), QStringLiteral("Clients"));
    QCOMPARE(missing.at(1).at(0).toString(), QStringLiteral("Checks"));
    // Settings marks both rules: neither name is one of the reader's Spaces.
    QCOMPARE(browser->readersSpaceNames(),
        QStringList({QStringLiteral("Personal"), QStringLiteral("Work")}));
}

void LinkRulesTest::leavesALinkNoRuleTakesInTheSpaceOnShow()
{
    QTemporaryDir config;
    QVERIFY(SettingsFile(config.path())
            .set(QStringLiteral("link-rules"), rules({{"*.acme.com", "Work"}})));
    SessionFixture fixture(twoSpaces(), config.path());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    WindowManager windows;
    QSignalSpy missing(browser.get(), &BrowserController::linkRuleSpaceMissing);

    QCOMPARE(windows.openFromOutside(browser.get(), address("https://elsewhere.example/")),
        browser.get());
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(browser->activeUrl(), address("https://elsewhere.example/"));
    QCOMPARE(missing.count(), 0);
}

// A rule names its Space by name, so renaming the Space renames it in its
// rules and nowhere else.
void LinkRulesTest::followsTheSpaceItNamesThroughARename()
{
    QTemporaryDir config;
    QVERIFY(SettingsFile(config.path())
            .set(QStringLiteral("link-rules"),
                rules({{"*.acme.com", "Work"}, {"github.com", "Personal"},
                    {"github.com/acme/*", "Work"}})));
    SessionFixture fixture(twoSpaces(), config.path());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();

    QVERIFY(browser->renameSpace(QStringLiteral("work"), QStringLiteral("Acme")));
    QCOMPARE(SettingsFile(config.path()).value(QStringLiteral("link-rules")),
        QJsonValue(rules(
            {{"*.acme.com", "Acme"}, {"github.com", "Personal"}, {"github.com/acme/*", "Acme"}})));

    // Two Spaces can share a name. The rules stay with the one that keeps it.
    const auto twin = browser->createSpace(QStringLiteral("Personal"));
    QVERIFY(browser->renameSpace(twin, QStringLiteral("Personal two")));
    QCOMPARE(SettingsFile(config.path()).value(QStringLiteral("link-rules")),
        QJsonValue(rules(
            {{"*.acme.com", "Acme"}, {"github.com", "Personal"}, {"github.com/acme/*", "Acme"}})));
}

// Deleting a Space asks first, naming how many rules name it, and takes them
// with it.
void LinkRulesTest::goesWithTheSpaceItNames()
{
    QTemporaryDir config;
    QVERIFY(SettingsFile(config.path())
            .set(QStringLiteral("link-rules"),
                rules({{"*.acme.com", "Work"}, {"github.com", "Personal"},
                    {"github.com/acme/*", "Work"}})));
    SessionFixture fixture(twoSpaces(), config.path());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    QCOMPARE(browser->linkRulesNaming(QStringLiteral("work")), 2);
    QCOMPARE(browser->linkRulesNaming(QStringLiteral("personal")), 1);

    // A deletion the reader did not confirm leaves them.
    QVERIFY(!browser->deleteSpace(QStringLiteral("work"), QStringLiteral("work")));
    QCOMPARE(browser->linkRulesNaming(QStringLiteral("work")), 2);

    QVERIFY(browser->deleteSpace(QStringLiteral("work"), QStringLiteral("Work")));
    QCOMPARE(SettingsFile(config.path()).value(QStringLiteral("link-rules")),
        QJsonValue(rules({{"github.com", "Personal"}})));

    // A Space whose name another Space also has takes no rules with it.
    const auto twin = browser->createSpace(QStringLiteral("Personal"));
    QCOMPARE(browser->linkRulesNaming(twin), 0);
    QVERIFY(browser->deleteSpace(twin, QStringLiteral("Personal")));
    QCOMPARE(SettingsFile(config.path()).value(QStringLiteral("link-rules")),
        QJsonValue(rules({{"github.com", "Personal"}})));
}

// Rules are for links from other apps. A link clicked in a page opens a tab
// through openInput or openInputInBackground, and an Agent's `open` names its
// own Space through openTabInSpace: none of them is sent anywhere else.
void LinkRulesTest::leavesTheBrowsersOwnLinksWhereTheyOpen()
{
    QTemporaryDir config;
    QVERIFY(SettingsFile(config.path())
            .set(QStringLiteral("link-rules"),
                rules({{"*.acme.com", "Work"}, {"bank.example", "private"}})));
    SessionFixture fixture(twoSpaces(), config.path());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    QSignalSpy missing(browser.get(), &BrowserController::linkRuleSpaceMissing);

    browser->openInput(QStringLiteral("https://docs.acme.com/clicked"), true);
    browser->openLink(address("https://docs.acme.com/opened"), true);
    browser->openInputInBackground(address("https://docs.acme.com/background"));
    QVERIFY(!browser->openTabInSpace(QStringLiteral("personal"), address("https://bank.example/"))
            .isEmpty());

    QCOMPARE(browser->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(tabAddresses(*browser),
        QStringList({QStringLiteral("https://personal.example/"),
            QStringLiteral("https://docs.acme.com/clicked"),
            QStringLiteral("https://docs.acme.com/opened"),
            QStringLiteral("https://docs.acme.com/background"),
            QStringLiteral("https://bank.example/")}));
    QCOMPARE(missing.count(), 0);
}

// A rule the reader writes into settings.json while the browser runs applies
// to the next link, and the chrome hears of it as of any setting.
void LinkRulesTest::followsAHandEditWhileItRuns()
{
    QTemporaryDir config;
    SessionFixture fixture(twoSpaces(), config.path());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    WindowManager windows;
    QSignalSpy changed(browser.get(), &BrowserController::preferenceChanged);

    QFile file(QDir(config.path()).filePath(SettingsFile::fileName()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version": 1, "link-rules": [{"pattern": "*.acme.com", "space": "Work"}]})");
    file.close();
    QTRY_VERIFY(changed.contains(QVariantList {QStringLiteral("link-rules")}));

    windows.openFromOutside(browser.get(), address("https://docs.acme.com/y"));
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("work"));
    QCOMPARE(browser->activeUrl(), address("https://docs.acme.com/y"));
}

QTEST_GUILESS_MAIN(LinkRulesTest)

#include "tst_linkrules.moc"
