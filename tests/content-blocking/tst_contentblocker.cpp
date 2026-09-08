#include "ContentBlocker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using omaweb::ContentBlocker;

namespace {

// The Space a request was made in, for the tests that are not about Spaces.
// Every refusal is made in one, because a Refusal tally belongs to a page
// address in a Space and there is no such thing as a request outside one.
const QString space = QStringLiteral("space-1");

} // namespace

class ContentBlockerTest final : public QObject {
    Q_OBJECT

private slots:
    void cosmeticsFollowRuleReplacementAndSiteToggles();
    void userRulesCompileOffTheCallerPath();
    void disablingASiteBypassesMatchingAndCosmetics();
    void disablingASiteRunsNoScriptlet();
    void subscriptionsExposeRequiredProvenanceAndUpdateStatus();
    void invalidSubscriptionUpdateKeepsTheActiveRules();
    void aListKeepsTheRulesThisContractParses();
    void aRefusedWindowCountsAsABlockedRequest();
    void aNewPageLoadStartsTheTallyAgain();
    void aReloadStartsTheTallyBothTabsReadAgain();
    void aRedirectCarriesTheTallyToTheAddressItResolvedTo();
    void aRefusalPendingAtALoadCountsForTheOutgoingPage();
    void twoPagesOnOneHostKeepSeparateTallies();
    void twoSpacesKeepSeparateTalliesForOneAddress();
    void aJumpInsideTheDocumentLeavesTheTallyRunning();
    void aPageNoViewIsShowingHasNoTally();
    void firstRunSubscribesToTheDefaultLists();
    void aSettingsFileWithNoMarkerSeedsOnceMore();
    void anEmptyListTheReaderChoseSurvivesTheNextRun();
    void aSettingsFileThatDoesNotParseIsLeftAlone();
};

namespace {

QString settingsPath(const QTemporaryDir &root)
{
    return root.filePath(QStringLiteral("content-blocking/settings.json"));
}

// A settings file as an earlier version left it, or as the reader's own
// choices did. Written by hand rather than by a first run, because what is
// under test is what load() makes of a file it did not write itself.
void writeSettings(const QTemporaryDir &root, const QJsonObject &fields)
{
    QVERIFY(QDir(root.path()).mkpath(QStringLiteral("content-blocking")));
    auto document = QJsonObject {
        {QStringLiteral("version"), 1},
        {QStringLiteral("userRules"), QString()},
        {QStringLiteral("disabledSites"), QJsonArray {}},
        {QStringLiteral("subscriptions"), QJsonArray {}},
    };
    for (auto field = fields.begin(); field != fields.end(); ++field) {
        document.insert(field.key(), field.value());
    }
    QFile file(settingsPath(root));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QJsonDocument(document).toJson()) > 0);
}

// A blocker with one rule that refuses everything from one host, ready to be
// asked. Every tally test wants the same thing, and none of them is about how
// rules get compiled.
std::unique_ptr<ContentBlocker> refusingBlocker(const QTemporaryDir &root)
{
    auto blocker
        = std::make_unique<ContentBlocker>(root.path(), ContentBlocker::DefaultLists::None);
    blocker->setUserRules(QStringLiteral("||ads.example^"));
    return blocker;
}

// One refused request, taken as far as the pending batch. Matching happens on
// whichever thread the engine hands a request to and the tally belongs to the
// blocker's, so a refusal crosses a queued call before it is even pending.
void refuse(ContentBlocker &blocker, const QUrl &page, const QString &spaceId)
{
    QVERIFY(blocker
            .checkRequest(QUrl(QStringLiteral("https://ads.example/ad.js")), page,
                QStringLiteral("script"), spaceId)
            .blocked);
    QCoreApplication::processEvents();
}

QJsonObject storedSettings(const QTemporaryDir &root)
{
    QFile file(settingsPath(root));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

void ContentBlockerTest::cosmeticsFollowRuleReplacementAndSiteToggles()
{
    QTemporaryDir root;
    ContentBlocker blocker(root.path(), ContentBlocker::DefaultLists::None);
    const QUrl page(QStringLiteral("https://example.com/article"));
    blocker.setUserRules(QStringLiteral("example.com##.old-ad\n##.generic-ad\n"
                                        "example.com##+js(set-constant, adsShown, false)"));
    QTRY_VERIFY_WITH_TIMEOUT(!blocker.compiling(), 5000);
    QVERIFY(blocker.cosmeticStyleSheet(page).contains(QStringLiteral(".old-ad")));
    QVERIFY(blocker.scriptletSource(page).contains(QStringLiteral("adsShown")));
    QVERIFY(blocker.cosmeticSurveyWanted(page));

    blocker.setSiteEnabled(page, false);
    QVERIFY(blocker.cosmeticStyleSheet(page).isEmpty());
    QVERIFY(blocker.scriptletSource(page).isEmpty());
    QVERIFY(!blocker.cosmeticSurveyWanted(page));
    QVERIFY(blocker.genericCosmeticStyleSheet(page, {QStringLiteral("generic-ad")}, {}).isEmpty());

    blocker.setUserRules(
        QStringLiteral("example.com##.new-ad\n##.generic-ad\nexample.com#@#.generic-ad\n"
                       "example.com##+js(set-constant, adsShown, false)\n"
                       "example.com#@#+js(set-constant, adsShown, false)"));
    QTRY_VERIFY_WITH_TIMEOUT(!blocker.compiling(), 5000);
    QVERIFY(blocker.cosmeticStyleSheet(page).isEmpty());
    blocker.setSiteEnabled(page, true);
    const auto css = blocker.cosmeticStyleSheet(page);
    QVERIFY(css.contains(QStringLiteral(".new-ad")));
    QVERIFY(!css.contains(QStringLiteral(".old-ad")));
    QVERIFY(blocker.scriptletSource(page).isEmpty());
    QVERIFY(blocker.genericCosmeticStyleSheet(page, {QStringLiteral("generic-ad")}, {}).isEmpty());
}

void ContentBlockerTest::userRulesCompileOffTheCallerPath()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    ContentBlocker blocker(root.path(), ContentBlocker::DefaultLists::None);
    QSignalSpy compiled(&blocker, &ContentBlocker::rulesChanged);

    blocker.setUserRules(QStringLiteral("||ads.example^\nexample.com##.sponsor"));
    QVERIFY(blocker.compiling());
    QTRY_VERIFY_WITH_TIMEOUT(!blocker.compiling(), 5000);
    QVERIFY(compiled.count() > 0);
    QVERIFY(blocker
            .checkRequest(QUrl(QStringLiteral("https://ads.example/ad.js")),
                QUrl(QStringLiteral("https://example.com/")), QStringLiteral("script"), space)
            .blocked);
    QVERIFY(blocker.cosmeticStyleSheet(QUrl(QStringLiteral("https://example.com/")))
            .contains(QStringLiteral(".sponsor")));
}

void ContentBlockerTest::disablingASiteBypassesMatchingAndCosmetics()
{
    QTemporaryDir root;
    ContentBlocker blocker(root.path(), ContentBlocker::DefaultLists::None);
    blocker.setUserRules(QStringLiteral("||ads.example^\nexample.com##.sponsor"));
    QTRY_VERIFY_WITH_TIMEOUT(!blocker.compiling(), 5000);

    blocker.setSiteEnabled(QUrl(QStringLiteral("https://example.com/page")), false);
    QVERIFY(!blocker.siteEnabled(QUrl(QStringLiteral("https://example.com/"))));
    QVERIFY(!blocker
            .checkRequest(QUrl(QStringLiteral("https://ads.example/ad.js")),
                QUrl(QStringLiteral("https://example.com/")), QStringLiteral("script"), space)
            .blocked);
    QVERIFY(blocker.cosmeticStyleSheet(QUrl(QStringLiteral("https://example.com/"))).isEmpty());
}

// A scriptlet is the one thing blocking does that runs code in the page, so
// "blocking off here" has to mean it too.
void ContentBlockerTest::disablingASiteRunsNoScriptlet()
{
    QTemporaryDir root;
    ContentBlocker blocker(root.path(), ContentBlocker::DefaultLists::None);
    blocker.setUserRules(QStringLiteral("example.com##+js(set-constant, adsShown, false)"));
    QTRY_VERIFY_WITH_TIMEOUT(!blocker.compiling(), 5000);
    const QUrl page(QStringLiteral("https://example.com/article"));
    QVERIFY(blocker.scriptletSource(page).contains(QStringLiteral("adsShown")));

    blocker.setSiteEnabled(page, false);
    QVERIFY(blocker.scriptletSource(page).isEmpty());
}

void ContentBlockerTest::subscriptionsExposeRequiredProvenanceAndUpdateStatus()
{
    QTemporaryDir root;
    QFile list(root.filePath(QStringLiteral("list.txt")));
    QVERIFY(list.open(QIODevice::WriteOnly));
    list.write("||tracker.example^\n");
    list.close();

    ContentBlocker blocker(root.path(), ContentBlocker::DefaultLists::None);
    const auto id = blocker.addSubscription(QStringLiteral("Test list"),
        QUrl(QStringLiteral("https://lists.example/about")), QStringLiteral("CC0-1.0"),
        QUrl::fromLocalFile(list.fileName()));
    QVERIFY(!id.isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!blocker.compiling(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(
        blocker.subscriptions().first().toMap().value(QStringLiteral("updateStatus")).toString(),
        QStringLiteral("current"), 5000);

    const auto subscription = blocker.subscriptions().first().toMap();
    QCOMPARE(subscription.value(QStringLiteral("source")).toUrl(),
        QUrl(QStringLiteral("https://lists.example/about")));
    QCOMPARE(subscription.value(QStringLiteral("license")).toString(), QStringLiteral("CC0-1.0"));
    QCOMPARE(subscription.value(QStringLiteral("updateAddress")).toUrl(),
        QUrl::fromLocalFile(list.fileName()));
    QVERIFY(blocker
            .checkRequest(QUrl(QStringLiteral("https://tracker.example/pixel")),
                QUrl(QStringLiteral("https://site.example/")), QStringLiteral("image"), space)
            .blocked);
}

void ContentBlockerTest::invalidSubscriptionUpdateKeepsTheActiveRules()
{
    QTemporaryDir root;
    QFile list(root.filePath(QStringLiteral("list.txt")));
    QVERIFY(list.open(QIODevice::WriteOnly));
    list.write("||tracker.example^\n");
    list.close();

    ContentBlocker blocker(root.path(), ContentBlocker::DefaultLists::None);
    const auto id = blocker.addSubscription(QStringLiteral("Test list"),
        QUrl(QStringLiteral("https://lists.example/about")), QStringLiteral("CC0-1.0"),
        QUrl::fromLocalFile(list.fileName()));
    QTRY_COMPARE_WITH_TIMEOUT(
        blocker.subscriptions().first().toMap().value(QStringLiteral("updateStatus")).toString(),
        QStringLiteral("current"), 5000);
    QVERIFY(blocker
            .checkRequest(QUrl(QStringLiteral("https://tracker.example/pixel")),
                QUrl(QStringLiteral("https://site.example/")), QStringLiteral("image"), space)
            .blocked);

    QVERIFY(list.open(QIODevice::WriteOnly | QIODevice::Truncate));
    list.write("||broken.example^$redirect=\n");
    list.close();
    blocker.updateSubscription(id);
    QTRY_VERIFY_WITH_TIMEOUT(blocker.subscriptions()
                                 .first()
                                 .toMap()
                                 .value(QStringLiteral("updateStatus"))
                                 .toString()
                                 .startsWith(QStringLiteral("failed:")),
        5000);
    QVERIFY(blocker
            .checkRequest(QUrl(QStringLiteral("https://tracker.example/pixel")),
                QUrl(QStringLiteral("https://site.example/")), QStringLiteral("image"), space)
            .blocked);
}

// A published list always carries rules outside this contract, and a list that
// fails as a whole over them ships blocking that never works.
void ContentBlockerTest::aListKeepsTheRulesThisContractParses()
{
    QTemporaryDir root;
    QFile list(root.filePath(QStringLiteral("list.txt")));
    QVERIFY(list.open(QIODevice::WriteOnly));
    list.write("||tracker.example^\n"
               "&popunder=$popup\n"
               "@@||google.com/recaptcha/$csp,subdocument\n");
    list.close();

    ContentBlocker blocker(root.path(), ContentBlocker::DefaultLists::None);
    blocker.addSubscription(QStringLiteral("Mixed list"),
        QUrl(QStringLiteral("https://lists.example/about")), QStringLiteral("CC0-1.0"),
        QUrl::fromLocalFile(list.fileName()));
    QTRY_COMPARE_WITH_TIMEOUT(
        blocker.subscriptions().first().toMap().value(QStringLiteral("updateStatus")).toString(),
        QStringLiteral("current"), 5000);
    QVERIFY(blocker
            .checkRequest(QUrl(QStringLiteral("https://tracker.example/pixel")),
                QUrl(QStringLiteral("https://site.example/")), QStringLiteral("image"), space)
            .blocked);
    QVERIFY(blocker.shouldBlockPopup(QUrl(QStringLiteral("https://ads.example/?&popunder=1")),
        QUrl(QStringLiteral("https://site.example/")), space));
}

// The tally means "requests this page did not get to make", and a window the
// page never got to open is one of them.
void ContentBlockerTest::aRefusedWindowCountsAsABlockedRequest()
{
    QTemporaryDir root;
    ContentBlocker blocker(root.path(), ContentBlocker::DefaultLists::None);
    blocker.setUserRules(QStringLiteral("||popads.example^$popup"));
    QTRY_VERIFY_WITH_TIMEOUT(!blocker.compiling(), 5000);
    const QUrl opener(QStringLiteral("https://site.example/article"));
    QObject view;
    blocker.showPage(&view, space, opener, 1);

    QVERIFY(!blocker.shouldBlockPopup(
        QUrl(QStringLiteral("https://pay.example/checkout")), opener, space));
    QVERIFY(blocker.shouldBlockPopup(
        QUrl(QStringLiteral("https://popads.example/win")), opener, space));
    QTRY_COMPARE_WITH_TIMEOUT(blocker.refusalTally(space, opener), 1, 5000);

    // A site the user turned blocking off for opens its windows either way.
    blocker.setSiteEnabled(opener, false);
    QVERIFY(!blocker.shouldBlockPopup(
        QUrl(QStringLiteral("https://popads.example/win")), opener, space));
}

// The behaviour the tally exists to deliver, and the one that used to live as a
// line in one adapter where no test could reach it (#142).
void ContentBlockerTest::aNewPageLoadStartsTheTallyAgain()
{
    QTemporaryDir root;
    const auto blocker = refusingBlocker(root);
    QTRY_VERIFY_WITH_TIMEOUT(!blocker->compiling(), 5000);
    const QUrl page(QStringLiteral("https://site.example/article"));
    QObject view;

    blocker->showPage(&view, space, page, 1);
    refuse(*blocker, page, space);
    refuse(*blocker, page, space);
    QTRY_COMPARE_WITH_TIMEOUT(blocker->refusalTally(space, page), 2, 5000);

    blocker->showPage(&view, space, page, 2);
    QCOMPARE(blocker->refusalTally(space, page), 0);
}

// A tally is shared by every tab on the address, so a load in one of them
// restarts what all of them read. With one tab the tally goes when the tab
// lets go of it, and starting it again would look like it worked either way.
void ContentBlockerTest::aReloadStartsTheTallyBothTabsReadAgain()
{
    QTemporaryDir root;
    const auto blocker = refusingBlocker(root);
    QTRY_VERIFY_WITH_TIMEOUT(!blocker->compiling(), 5000);
    const QUrl page(QStringLiteral("https://site.example/article"));
    QObject reloading;
    QObject watching;
    blocker->showPage(&reloading, space, page, 1);
    blocker->showPage(&watching, space, page, 1);

    refuse(*blocker, page, space);
    QTRY_COMPARE_WITH_TIMEOUT(blocker->refusalTally(space, page), 1, 5000);

    blocker->showPage(&reloading, space, page, 2);
    QCOMPARE(blocker->refusalTally(space, page), 0);
}

// A load that resolves to a redirect arrives somewhere else without a load of
// its own, so the view says so again at the same page load and the tally goes
// with it. Nothing carries over: what was refused on the way belonged to the
// address that was left.
void ContentBlockerTest::aRedirectCarriesTheTallyToTheAddressItResolvedTo()
{
    QTemporaryDir root;
    const auto blocker = refusingBlocker(root);
    QTRY_VERIFY_WITH_TIMEOUT(!blocker->compiling(), 5000);
    const QUrl asked(QStringLiteral("http://site.example/article"));
    const QUrl resolved(QStringLiteral("https://site.example/article"));
    QObject view;

    blocker->showPage(&view, space, asked, 1);
    blocker->showPage(&view, space, resolved, 1);
    refuse(*blocker, resolved, space);

    QTRY_COMPARE_WITH_TIMEOUT(blocker->refusalTally(space, resolved), 1, 5000);
    // Nobody is showing the address that was asked for any more.
    QCOMPARE(blocker->refusalTally(space, asked), 0);
}

// Refusals are batched, so a load can start while some are still pending. They
// were earned by the document being replaced and are credited to it, not to
// the document that follows.
void ContentBlockerTest::aRefusalPendingAtALoadCountsForTheOutgoingPage()
{
    QTemporaryDir root;
    const auto blocker = refusingBlocker(root);
    QTRY_VERIFY_WITH_TIMEOUT(!blocker->compiling(), 5000);
    const QUrl outgoing(QStringLiteral("https://site.example/article"));
    const QUrl arriving(QStringLiteral("https://site.example/other"));
    // A second tab on the outgoing page, so its tally is still there to be read
    // after the first tab has moved on.
    QObject leaving;
    QObject staying;
    blocker->showPage(&leaving, space, outgoing, 1);
    blocker->showPage(&staying, space, outgoing, 1);

    refuse(*blocker, outgoing, space);
    // Still pending: the batch has not been delivered yet, which is the moment
    // this test is about.
    QCOMPARE(blocker->refusalTally(space, outgoing), 0);

    blocker->showPage(&leaving, space, arriving, 2);
    QCOMPARE(blocker->refusalTally(space, outgoing), 1);
    QCOMPARE(blocker->refusalTally(space, arriving), 0);
}

// The case a reader sees. The tally used to be keyed by host, so every tab on
// a host reported everything that host refused (ADR 0037).
void ContentBlockerTest::twoPagesOnOneHostKeepSeparateTallies()
{
    QTemporaryDir root;
    const auto blocker = refusingBlocker(root);
    QTRY_VERIFY_WITH_TIMEOUT(!blocker->compiling(), 5000);
    const QUrl first(QStringLiteral("https://site.example/one"));
    const QUrl second(QStringLiteral("https://site.example/two"));
    QObject firstView;
    QObject secondView;
    blocker->showPage(&firstView, space, first, 1);
    blocker->showPage(&secondView, space, second, 1);

    refuse(*blocker, first, space);
    refuse(*blocker, first, space);
    refuse(*blocker, second, space);

    QTRY_COMPARE_WITH_TIMEOUT(blocker->refusalTally(space, first), 2, 5000);
    QCOMPARE(blocker->refusalTally(space, second), 1);
}

// A Space is a browsing identity, and the same address opened in two of them is
// two documents. Interception carries the Space for this reason alone.
void ContentBlockerTest::twoSpacesKeepSeparateTalliesForOneAddress()
{
    QTemporaryDir root;
    const auto blocker = refusingBlocker(root);
    QTRY_VERIFY_WITH_TIMEOUT(!blocker->compiling(), 5000);
    const QUrl page(QStringLiteral("https://site.example/article"));
    const auto work = QStringLiteral("space-work");
    const auto home = QStringLiteral("space-home");
    QObject atWork;
    QObject atHome;
    blocker->showPage(&atWork, work, page, 1);
    blocker->showPage(&atHome, home, page, 1);

    refuse(*blocker, page, work);

    QTRY_COMPARE_WITH_TIMEOUT(blocker->refusalTally(work, page), 1, 5000);
    QCOMPARE(blocker->refusalTally(home, page), 0);
}

// A fragment jump is the same document, so it is the same tally: the fragment
// is off the key, and a view that says so again at the same page load changes
// nothing.
void ContentBlockerTest::aJumpInsideTheDocumentLeavesTheTallyRunning()
{
    QTemporaryDir root;
    const auto blocker = refusingBlocker(root);
    QTRY_VERIFY_WITH_TIMEOUT(!blocker->compiling(), 5000);
    const QUrl page(QStringLiteral("https://site.example/article"));
    const QUrl jumped(QStringLiteral("https://site.example/article#comments"));
    QObject view;

    blocker->showPage(&view, space, page, 1);
    refuse(*blocker, page, space);
    QTRY_COMPARE_WITH_TIMEOUT(blocker->refusalTally(space, page), 1, 5000);

    blocker->showPage(&view, space, jumped, 1);
    QCOMPARE(blocker->refusalTally(space, page), 1);
    QCOMPARE(blocker->refusalTally(space, jumped), 1);

    refuse(*blocker, jumped, space);
    QTRY_COMPARE_WITH_TIMEOUT(blocker->refusalTally(space, page), 2, 5000);
}

// The live tallies are the page loads that are open. A tab that has gone takes
// its tally with it rather than leaving a record of where the reader has been.
void ContentBlockerTest::aPageNoViewIsShowingHasNoTally()
{
    QTemporaryDir root;
    const auto blocker = refusingBlocker(root);
    QTRY_VERIFY_WITH_TIMEOUT(!blocker->compiling(), 5000);
    const QUrl page(QStringLiteral("https://site.example/article"));

    {
        QObject view;
        blocker->showPage(&view, space, page, 1);
        refuse(*blocker, page, space);
        QTRY_COMPARE_WITH_TIMEOUT(blocker->refusalTally(space, page), 1, 5000);
    }

    QCOMPARE(blocker->refusalTally(space, page), 0);
    // And a refusal for a page nobody is showing lands nowhere.
    refuse(*blocker, page, space);
    QTest::qWait(500);
    QCOMPARE(blocker->refusalTally(space, page), 0);
}

void ContentBlockerTest::firstRunSubscribesToTheDefaultLists()
{
    QTemporaryDir root;
    ContentBlocker blocker(root.path());

    const auto subscriptions = blocker.subscriptions();
    QCOMPARE(subscriptions.size(), 2);
    QStringList titles;
    for (const auto &value : subscriptions) {
        const auto subscription = value.toMap();
        titles.append(subscription.value(QStringLiteral("title")).toString());
        QVERIFY(subscription.value(QStringLiteral("enabled")).toBool());
        QVERIFY(subscription.value(QStringLiteral("updateAddress")).toUrl().isValid());
        QVERIFY(!subscription.value(QStringLiteral("license")).toString().isEmpty());
    }
    QCOMPARE(titles, QStringList({QStringLiteral("EasyList"), QStringLiteral("EasyPrivacy")}));
    QVERIFY(QFile::exists(settingsPath(root)));
    // Recorded, so the next run reads a decision rather than inferring one.
    QVERIFY(storedSettings(root).value(QStringLiteral("seeded")).toBool());

    // A second run reads the stored subscriptions rather than seeding again.
    ContentBlocker resumed(root.path());
    QCOMPARE(resumed.subscriptions().size(), 2);
}

// A settings file that exists, parses, and lists no subscription is the state
// #43 was found in: file absence stood in for "never seeded", so such an
// install blocked nothing, for ever, without an error anywhere. The marker
// makes "never seeded" and "seeded, then emptied" two states rather than one,
// and a file written before the marker existed is the first of the two.
void ContentBlockerTest::aSettingsFileWithNoMarkerSeedsOnceMore()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    writeSettings(root, {{QStringLiteral("userRules"), QStringLiteral("||kept.example^")}});

    ContentBlocker blocker(root.path());
    QCOMPARE(blocker.subscriptions().size(), 2);
    // Seeding is a repair, not a reset: what the file did say is still said.
    QCOMPARE(blocker.userRules(), QStringLiteral("||kept.example^"));
    QVERIFY(storedSettings(root).value(QStringLiteral("seeded")).toBool());
}

// The other half of the marker: once an install has been seeded, an empty list
// is a decision. Reinstating subscriptions the reader deleted is worse than
// blocking nothing, so nothing comes back on its own. Asking twice does not
// subscribe twice either, because a list already there is left as it is.
void ContentBlockerTest::anEmptyListTheReaderChoseSurvivesTheNextRun()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    writeSettings(root, {{QStringLiteral("seeded"), true}});

    ContentBlocker blocker(root.path());
    QCOMPARE(blocker.subscriptions().size(), 0);

    blocker.restoreDefaultSubscriptions();
    QCOMPARE(blocker.subscriptions().size(), 2);
    blocker.restoreDefaultSubscriptions();
    QCOMPARE(blocker.subscriptions().size(), 2);
}

// Seeding writes the settings file, so the migration must not run on a file
// nobody could read. A truncated write leaves rules and per-site decisions
// still in there, recoverable by hand only for as long as the file survives.
void ContentBlockerTest::aSettingsFileThatDoesNotParseIsLeftAlone()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath(QStringLiteral("content-blocking")));
    const auto truncated
        = QByteArray("{\n  \"userRules\": \"||kept.example^\",\n  \"subscriptions\": [");
    QFile file(settingsPath(root));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(truncated) > 0);
    file.close();

    ContentBlocker blocker(root.path());
    QCOMPARE(blocker.subscriptions().size(), 0);

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), truncated);
}

QTEST_GUILESS_MAIN(ContentBlockerTest)

#include "tst_contentblocker.moc"
