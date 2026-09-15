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
    void leavesAPausedTabForOneThatIsStillPlaying();
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
    tabs.reportSound(QStringLiteral("tab-a"), false, QStringLiteral("Quiet page"), false, {});
    QVERIFY(tabs.announcement().isEmpty());
    QCOMPARE(changed.count(), 0);
}

void SoundingTabsTest::namesTheTabWhenThePageDeclaresNothing()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Radio Helsinki"), false, {});

    const auto announcement = tabs.announcement();
    QCOMPARE(announcement.value(QStringLiteral("tabId")).toString(), QStringLiteral("tab-a"));
    QCOMPARE(
        announcement.value(QStringLiteral("title")).toString(), QStringLiteral("Radio Helsinki"));
    QVERIFY(announcement.value(QStringLiteral("playing")).toBool());
}

void SoundingTabsTest::prefersWhatThePageDeclares()
{
    SoundingTabs tabs;
    auto declared = declaration(QStringLiteral("playing"), QStringLiteral("Declared title"));
    declared.insert(QStringLiteral("artist"), QStringLiteral("Declared artist"));
    declared.insert(QStringLiteral("artwork"), QStringLiteral("https://example.test/cover.png"));
    declared.insert(QStringLiteral("canGoNext"), true);
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Tab title"), false, declared);

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
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("First"), false, {});
    tabs.reportSound(QStringLiteral("tab-b"), true, QStringLiteral("Second"), false, {});
    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-b"));

    // A title arriving for the tab that was already playing is not a new start.
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("First, renamed"), false, {});
    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-b"));

    // When the newer one stops, the keys fall back rather than going nowhere.
    tabs.reportSound(QStringLiteral("tab-b"), false, QStringLiteral("Second"), false, {});
    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-a"));
    QCOMPARE(tabs.announcement().value(QStringLiteral("title")).toString(),
        QStringLiteral("First, renamed"));
}

void SoundingTabsTest::keepsThePlayerWhileThePageIsPaused()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Video"), false,
        declaration(QStringLiteral("playing"), QStringLiteral("Episode")));

    // The desktop pauses it: the page stops making sound but is still what the
    // next key press is for.
    tabs.reportSound(QStringLiteral("tab-a"), false, QStringLiteral("Video"), false,
        declaration(QStringLiteral("paused"), QStringLiteral("Episode")));

    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-a"));
    QVERIFY(!tabs.announcement().value(QStringLiteral("playing")).toBool());
    QCOMPARE(
        tabs.announcement().value(QStringLiteral("title")).toString(), QStringLiteral("Episode"));
}

void SoundingTabsTest::withdrawsWhenThePageStops()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Video"), false,
        declaration(QStringLiteral("playing")));

    QSignalSpy changed(&tabs, &SoundingTabs::announcementChanged);
    tabs.reportSound(QStringLiteral("tab-a"), false, QStringLiteral("Video"), false,
        declaration(QStringLiteral("none")));

    QVERIFY(tabs.announcement().isEmpty());
    QCOMPARE(changed.count(), 1);
}

void SoundingTabsTest::withholdsWhatAPrivateTabIsPlaying()
{
    SoundingTabs tabs;
    auto declared = declaration(QStringLiteral("playing"), QStringLiteral("Declared title"));
    declared.insert(QStringLiteral("artist"), QStringLiteral("Declared artist"));
    declared.insert(QStringLiteral("album"), QStringLiteral("Declared album"));
    declared.insert(QStringLiteral("artwork"), QStringLiteral("https://example.test/cover.png"));
    declared.insert(QStringLiteral("canGoPrevious"), true);
    tabs.reportSound(QStringLiteral("tab-p"), true, QStringLiteral("Tab title"), true, declared);

    const auto announcement = tabs.announcement();
    QCOMPARE(announcement.value(QStringLiteral("tabId")).toString(), QStringLiteral("tab-p"));
    QVERIFY(announcement.value(QStringLiteral("playing")).toBool());
    QVERIFY(announcement.value(QStringLiteral("canGoPrevious")).toBool());
    QVERIFY(!announcement.contains(QStringLiteral("title")));
    QVERIFY(!announcement.contains(QStringLiteral("artist")));
    QVERIFY(!announcement.contains(QStringLiteral("album")));
    QVERIFY(!announcement.contains(QStringLiteral("artwork")));
}

void SoundingTabsTest::forgetsAClosedTab()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Playing"), false, {});
    tabs.forget(QStringLiteral("tab-a"));
    QVERIFY(tabs.announcement().isEmpty());
}

// Story 9 read the other way round: the tab that started last is the one the
// keys reach, until the reader pauses it and something else is still playing.
void SoundingTabsTest::leavesAPausedTabForOneThatIsStillPlaying()
{
    SoundingTabs tabs;
    tabs.reportSound(QStringLiteral("tab-a"), true, QStringLiteral("Music"), false, {});
    tabs.reportSound(QStringLiteral("tab-b"), true, QStringLiteral("Video"), false,
        declaration(QStringLiteral("playing"), QStringLiteral("Episode")));
    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-b"));

    // The reader pauses the video. The music is still playing, so the keys go
    // to the music rather than staying on a page that is not making a sound.
    tabs.reportSound(QStringLiteral("tab-b"), false, QStringLiteral("Video"), false,
        declaration(QStringLiteral("paused"), QStringLiteral("Episode")));
    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-a"));

    // Starting the video again takes them back.
    tabs.reportSound(QStringLiteral("tab-b"), true, QStringLiteral("Video"), false,
        declaration(QStringLiteral("playing"), QStringLiteral("Episode")));
    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-b"));

    // With both paused there is still one player, and it is the one that
    // started last rather than nothing at all.
    tabs.reportSound(QStringLiteral("tab-a"), false, QStringLiteral("Music"), false,
        declaration(QStringLiteral("paused")));
    tabs.reportSound(QStringLiteral("tab-b"), false, QStringLiteral("Video"), false,
        declaration(QStringLiteral("paused"), QStringLiteral("Episode")));
    QCOMPARE(tabs.soundingTabId(), QStringLiteral("tab-b"));
    QVERIFY(!tabs.announcement().value(QStringLiteral("playing")).toBool());
}

QTEST_GUILESS_MAIN(SoundingTabsTest)

#include "tst_soundingtabs.moc"
