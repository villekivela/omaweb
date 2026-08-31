#include "ContentBlocker.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using tanto::ContentBlocker;

class ContentBlockerTest final : public QObject {
    Q_OBJECT

private slots:
    void userRulesCompileOffTheCallerPath();
    void disablingASiteBypassesMatchingAndCosmetics();
    void disablingASiteRunsNoScriptlet();
    void subscriptionsExposeRequiredProvenanceAndUpdateStatus();
    void invalidSubscriptionUpdateKeepsTheActiveRules();
    void aListKeepsTheRulesThisContractParses();
    void aRefusedWindowCountsAsABlockedRequest();
    void firstRunSubscribesToTheDefaultLists();
};

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
    QVERIFY(blocker.shouldBlock(QUrl(QStringLiteral("https://ads.example/ad.js")),
        QUrl(QStringLiteral("https://example.com/")), QStringLiteral("script")));
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
    QVERIFY(!blocker.shouldBlock(QUrl(QStringLiteral("https://ads.example/ad.js")),
        QUrl(QStringLiteral("https://example.com/")), QStringLiteral("script")));
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
    QTRY_COMPARE_WITH_TIMEOUT(blocker.subscriptions().first().toMap()
        .value(QStringLiteral("updateStatus")).toString(), QStringLiteral("current"), 5000);

    const auto subscription = blocker.subscriptions().first().toMap();
    QCOMPARE(subscription.value(QStringLiteral("source")).toUrl(),
        QUrl(QStringLiteral("https://lists.example/about")));
    QCOMPARE(subscription.value(QStringLiteral("license")).toString(), QStringLiteral("CC0-1.0"));
    QCOMPARE(subscription.value(QStringLiteral("updateAddress")).toUrl(),
        QUrl::fromLocalFile(list.fileName()));
    QVERIFY(blocker.shouldBlock(QUrl(QStringLiteral("https://tracker.example/pixel")),
        QUrl(QStringLiteral("https://site.example/")), QStringLiteral("image")));
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
    QTRY_COMPARE_WITH_TIMEOUT(blocker.subscriptions().first().toMap()
        .value(QStringLiteral("updateStatus")).toString(), QStringLiteral("current"), 5000);
    QVERIFY(blocker.shouldBlock(QUrl(QStringLiteral("https://tracker.example/pixel")),
        QUrl(QStringLiteral("https://site.example/")), QStringLiteral("image")));

    QVERIFY(list.open(QIODevice::WriteOnly | QIODevice::Truncate));
    list.write("||broken.example^$redirect=\n");
    list.close();
    blocker.updateSubscription(id);
    QTRY_VERIFY_WITH_TIMEOUT(blocker.subscriptions().first().toMap()
        .value(QStringLiteral("updateStatus")).toString().startsWith(QStringLiteral("failed:")),
        5000);
    QVERIFY(blocker.shouldBlock(QUrl(QStringLiteral("https://tracker.example/pixel")),
        QUrl(QStringLiteral("https://site.example/")), QStringLiteral("image")));
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
    QTRY_COMPARE_WITH_TIMEOUT(blocker.subscriptions().first().toMap()
        .value(QStringLiteral("updateStatus")).toString(), QStringLiteral("current"), 5000);
    QVERIFY(blocker.shouldBlock(QUrl(QStringLiteral("https://tracker.example/pixel")),
        QUrl(QStringLiteral("https://site.example/")), QStringLiteral("image")));
    QVERIFY(blocker.shouldBlockPopup(QUrl(QStringLiteral("https://ads.example/?&popunder=1")),
        QUrl(QStringLiteral("https://site.example/"))));
}

// The count means "requests this page did not get to make", and a window the
// page never got to open is one of them.
void ContentBlockerTest::aRefusedWindowCountsAsABlockedRequest()
{
    QTemporaryDir root;
    ContentBlocker blocker(root.path(), ContentBlocker::DefaultLists::None);
    blocker.setUserRules(QStringLiteral("||popads.example^$popup"));
    QTRY_VERIFY_WITH_TIMEOUT(!blocker.compiling(), 5000);
    const QUrl opener(QStringLiteral("https://site.example/article"));

    QVERIFY(!blocker.shouldBlockPopup(QUrl(QStringLiteral("https://pay.example/checkout")),
        opener));
    QVERIFY(blocker.shouldBlockPopup(QUrl(QStringLiteral("https://popads.example/win")), opener));
    QTRY_COMPARE_WITH_TIMEOUT(blocker.blockedRequestCount(opener), 1, 5000);

    // A site the user turned blocking off for opens its windows either way.
    blocker.setSiteEnabled(opener, false);
    QVERIFY(!blocker.shouldBlockPopup(QUrl(QStringLiteral("https://popads.example/win")),
        opener));
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
    QVERIFY(QFile::exists(root.filePath(QStringLiteral("content-blocking/settings.json"))));

    // A second run reads the stored subscriptions rather than seeding again.
    ContentBlocker resumed(root.path());
    QCOMPARE(resumed.subscriptions().size(), 2);
}

QTEST_GUILESS_MAIN(ContentBlockerTest)

#include "tst_contentblocker.moc"
