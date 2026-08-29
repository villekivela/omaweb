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
    void subscriptionsExposeRequiredProvenanceAndUpdateStatus();
    void invalidSubscriptionUpdateKeepsTheActiveRules();
};

void ContentBlockerTest::userRulesCompileOffTheCallerPath()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    ContentBlocker blocker(root.path());
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
    ContentBlocker blocker(root.path());
    blocker.setUserRules(QStringLiteral("||ads.example^\nexample.com##.sponsor"));
    QTRY_VERIFY_WITH_TIMEOUT(!blocker.compiling(), 5000);

    blocker.setSiteEnabled(QUrl(QStringLiteral("https://example.com/page")), false);
    QVERIFY(!blocker.siteEnabled(QUrl(QStringLiteral("https://example.com/"))));
    QVERIFY(!blocker.shouldBlock(QUrl(QStringLiteral("https://ads.example/ad.js")),
        QUrl(QStringLiteral("https://example.com/")), QStringLiteral("script")));
    QVERIFY(blocker.cosmeticStyleSheet(QUrl(QStringLiteral("https://example.com/"))).isEmpty());
}

void ContentBlockerTest::subscriptionsExposeRequiredProvenanceAndUpdateStatus()
{
    QTemporaryDir root;
    QFile list(root.filePath(QStringLiteral("list.txt")));
    QVERIFY(list.open(QIODevice::WriteOnly));
    list.write("||tracker.example^\n");
    list.close();

    ContentBlocker blocker(root.path());
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

    ContentBlocker blocker(root.path());
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

QTEST_GUILESS_MAIN(ContentBlockerTest)

#include "tst_contentblocker.moc"
