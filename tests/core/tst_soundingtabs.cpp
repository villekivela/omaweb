#include "SoundingTabs.h"

#include <QSignalSpy>
#include <QTest>

using omaweb::SoundingTabs;

class SoundingTabsTest final : public QObject {
    Q_OBJECT

private slots:
    void announcesNothingUntilATabMakesSound();
    void namesTheTabWhenThePageDeclaresNothing();
    void prefersWhatThePageDeclares();
    void handsTheKeysToTheTabThatStartedLast();
    void keepsThePlayerWhileThePageIsPaused();
    void withdrawsWhenThePageStops();
    void withholdsWhatAPrivateTabIsPlaying();
    void forgetsAClosedTab();
};

namespace {

QVariantMap declaration(const QString &state, const QString &title = {})
{
    QVariantMap declared;
    declared.insert(QStringLiteral("state"), state);
    if (!title.isEmpty()) {
        declared.insert(QStringLiteral("title"), title);
    }
    return declared;
}

} // namespace

void SoundingTabsTest::announcesNothingUntilATabMakesSound()
{
    SoundingTabs tabs;
    QVERIFY(tabs.announcement().isEmpty());
    QVERIFY(tabs.soundingTabId().isEmpty());

    QSignalSpy changed(&tabs, &SoundingTabs::announcementChanged);
    tabs.reportSound(QStringLiteral("tab-a"), false, QStringLiteral("Quiet page"), false);
    QVERIFY(tabs.announcement().isEmpty());
    QCOMPARE(changed.count(), 0);
}

void SoundingTabsTest::namesTheTabWhenThePageDeclaresNothing()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Radio Helsinki"), false);

    const auto announcement = tabs.announcement();
    QCOMPARE(announcement.value(QStringLiteral("tabId")).toString(), QStringLiteral("tab-a"));
    QCOMPARE(
        announcement.value(QStringLiteral("title")).toString(), QStringLiteral("Radio Helsinki"));
    QVERIFY(announcement.value(QStringLiteral("playing")).toBool());
}

void SoundingTabsTest::prefersWhatThePageDeclares()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Tab title"), false);
    auto declared = declaration(QStringLiteral("playing"), QStringLiteral("Declared title"));
    declared.insert(QStringLiteral("artist"), QStringLiteral("Declared artist"));
    declared.insert(QStringLiteral("artwork"), QStringLiteral("https://example.test/cover.png"));
    declared.insert(QStringLiteral("canGoNext"), true);
    tabs.reportDeclared(QStringLiteral("tab-a"), declared);

    const auto announcement = tabs.announcement();
    QCOMPARE(
        announcement.value(QStringLiteral("title")).toString(), QStringLiteral("Declared title"));
    QCOMPARE(
        announcement.value(QStringLiteral("artist")).toString(), QStringLiteral("Declared artist"));
    QCOMPARE(announcement.value(QStringLiteral("artwork")).toString(),
        QStringLiteral("https://example.test/cover.png"));
    QVERIFY(announcement.value(QStringLiteral("canGoNext")).toBool());
    QVERIFY(!announcement.value(QStringLiteral("canGoPrevious")).toBool());
}

void SoundingTabsTest::handsTheKeysToTheTabThatStartedLast()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("First"), false);
    tabs.reportSound(QStringLiteral("tab-b"), true, QStringLiteral("Second"), false);
    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-b"));

    // A title arriving for the tab that was already playing is not a new start.
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("First, renamed"), false);
    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-b"));

    // When the newer one stops, the keys fall back rather than going nowhere.
    tabs.reportSound(QStringLiteral("tab-b"), false, QStringLiteral("Second"), false);
    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-a"));
    QCOMPARE(tabs.announcement().value(QStringLiteral("title")).toString(),
        QStringLiteral("First, renamed"));
}

void SoundingTabsTest::keepsThePlayerWhileThePageIsPaused()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Video"), false);
    tabs.reportDeclared(
        QStringLiteral("tab-a"), declaration(QStringLiteral("playing"), QStringLiteral("Episode")));

    // The desktop pauses it: the page stops making sound but is still what the
    // next key press is for.
    tabs.reportDeclared(
        QStringLiteral("tab-a"), declaration(QStringLiteral("paused"), QStringLiteral("Episode")));
    tabs.reportSound(QStringLiteral("tab-a"), false, QStringLiteral("Video"), false);

    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-a"));
    QVERIFY(!tabs.announcement().value(QStringLiteral("playing")).toBool());
    QCOMPARE(
        tabs.announcement().value(QStringLiteral("title")).toString(), QStringLiteral("Episode"));
}

void SoundingTabsTest::withdrawsWhenThePageStops()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Video"), false);
    tabs.reportDeclared(QStringLiteral("tab-a"), declaration(QStringLiteral("playing")));

    QSignalSpy changed(&tabs, &SoundingTabs::announcementChanged);
    tabs.reportDeclared(QStringLiteral("tab-a"), declaration(QStringLiteral("none")));
    tabs.reportSound(QStringLiteral("tab-a"), false, QStringLiteral("Video"), false);

    QVERIFY(tabs.announcement().isEmpty());
    QCOMPARE(changed.count(), 1);
}

void SoundingTabsTest::withholdsWhatAPrivateTabIsPlaying()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-p"), true, QStringLiteral("Tab title"), true);
    auto declared = declaration(QStringLiteral("playing"), QStringLiteral("Declared title"));
    declared.insert(QStringLiteral("artist"), QStringLiteral("Declared artist"));
    declared.insert(QStringLiteral("artwork"), QStringLiteral("https://example.test/cover.png"));
    declared.insert(QStringLiteral("canGoPrevious"), true);
    tabs.reportDeclared(QStringLiteral("tab-p"), declared);

    const auto announcement = tabs.announcement();
    QCOMPARE(announcement.value(QStringLiteral("tabId")).toString(), QStringLiteral("tab-p"));
    QVERIFY(announcement.value(QStringLiteral("playing")).toBool());
    QVERIFY(announcement.value(QStringLiteral("canGoPrevious")).toBool());
    QVERIFY(!announcement.contains(QStringLiteral("title")));
    QVERIFY(!announcement.contains(QStringLiteral("artist")));
    QVERIFY(!announcement.contains(QStringLiteral("artwork")));
}

void SoundingTabsTest::forgetsAClosedTab()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Playing"), false);
    tabs.forget(QStringLiteral("tab-a"));
    QVERIFY(tabs.announcement().isEmpty());
}

QTEST_GUILESS_MAIN(SoundingTabsTest)

#include "tst_soundingtabs.moc"
