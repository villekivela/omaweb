#include "BrowserController.h"
#include "PrivateSessionFixture.h"
#include "SessionFixture.h"
#include "SettingsFile.h"
#include "SpaceStorage.h"
#include "SqliteSessionStore.h"
#include "TabListModel.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using omaweb::BrowserController;
using omaweb::SpaceStorage;
using omaweb::TabListModel;
using omaweb::test::PrivateSessionFixture;
using omaweb::test::SessionFixture;
using omaweb::test::SessionSpec;
using omaweb::test::SpaceSpec;
using omaweb::test::TabSpec;

namespace {

constexpr qint64 hour = 60 * 60 * 1000;
// A fixed moment, so every age a test names is counted from a clock it owns.
// It is in 2100: the check a window makes as it starts reads the wall clock,
// which must never find a tab a test stamped old enough to put away.
constexpr qint64 start = 4'102'444'800'000;

QStringList tabUrls(BrowserController &controller)
{
    QStringList urls;
    auto *tabs = controller.tabs();
    for (int row = 0; row < tabs->rowCount(); ++row) {
        urls.append(tabs->data(tabs->index(row, 0), TabListModel::UrlRole).toUrl().toString());
    }
    return urls;
}

QStringList putAwayUrls(const BrowserController &controller)
{
    QStringList urls;
    for (const auto &entry : controller.putAwayTabs()) {
        urls.append(entry.toMap().value(QStringLiteral("url")).toUrl().toString());
    }
    return urls;
}

// A window whose clock reads `start`, so the tabs it opens left show then.
std::unique_ptr<BrowserController> openWindow(const QTemporaryDir &root)
{
    // A configuration root of its own, where the reader's limit is kept.
    auto controller = std::make_unique<BrowserController>(
        SpaceStorage(root.path(), QStringLiteral("test")), root.filePath(QStringLiteral("config")));
    controller->setNowForTests(start);
    return controller;
}

// The id of the tab at a row of the Space on show.
QString tabIdAt(BrowserController &controller, int row)
{
    auto *tabs = controller.tabs();
    return tabs->data(tabs->index(row, 0), TabListModel::IdRole).toString();
}

// A check a day after `start`, long past the twelve hours.
void checkADayLater(BrowserController &controller)
{
    controller.setNowForTests(start + 24 * hour);
    controller.putAwayUnusedTabs();
}

} // namespace

class PutAwayTabsTest final : public QObject {
    Q_OBJECT

private slots:
    void putsAwayATabNotShownForTwelveHoursAndNotBefore();
    void keepsTheListAndEachTabsLastShowingAcrossARestart();
    void neverPutsAwayThePageOnShow();
    void neverPutsAwayAPinnedTab();
    void neverPutsAwayAKeepActiveTab();
    void neverPutsAwayATabMakingSound();
    void neverPutsAwayAnAgentTabWhileAnAgentIsAttached();
    void neverPutsAwayEitherTabOfASplit();
    void neverPutsAwayATabTheReaderIsDragging();
    void putsAwayAtStartup();
    void countsATabStoredBeforeTheRecordFromTheFirstStart();
    void putsAwayInASpaceThatIsNotOnShow();
    void putsAwayOnASpaceSwitch();
    void putsAwayOnAPeriodicCheck();
    void reopensAPutAwayTabWhereItWasLeft();
    void keepsAPutAwayTabForThirtyDays();
    void clearsTheListWithTheSpacesHistory();
    void deletesTheListWithItsSpace();
    void neverPutsAwayInAPrivateWindow();
    void leavesTheRecentlyClosedStackToTheReadersCloses();
    void putsAwayAfterTheLimitTheReaderChose();
    void putsNothingAwayWhileTheSettingIsOff();
    void offersOnlyTheNamedLimits();
    void saysSoOnceTheFirstTimeTabsArePutAway();
    void readsTheReadersLimitAtStartup();
    void saysSoWhenTheFirstPutAwayIsAtStartup();
    void keepsATabsLastShowingThroughAWriteThatDoesNotKnowIt();
};

// Twelve hours is the default. A tab left one millisecond short of it stays,
// and at the limit it goes to the Space's put-away list.
void PutAwayTabsTest::putsAwayATabNotShownForTwelveHoursAndNotBefore()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.setNowForTests(start);
    controller.openInput(QStringLiteral("https://left.example"), false);
    controller.openInput(QStringLiteral("https://kept.example"), true);

    controller.setNowForTests(start + 12 * hour - 1);
    controller.putAwayUnusedTabs();
    QCOMPARE(tabUrls(controller),
        QStringList(
            {QStringLiteral("https://left.example"), QStringLiteral("https://kept.example")}));
    QVERIFY(controller.putAwayTabs().isEmpty());

    controller.setNowForTests(start + 12 * hour);
    controller.putAwayUnusedTabs();
    QCOMPARE(tabUrls(controller), QStringList {QStringLiteral("https://kept.example")});
    QCOMPARE(putAwayUrls(controller), QStringList {QStringLiteral("https://left.example")});
}

// The list is the Space's and written down, and so is when each tab left show:
// a restart neither forgets what was put away nor starts the count again.
void PutAwayTabsTest::keepsTheListAndEachTabsLastShowingAcrossARestart()
{
    QTemporaryDir root;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        controller.setNowForTests(start);
        controller.openInput(QStringLiteral("https://first.example"), false);
        controller.openInput(QStringLiteral("https://second.example"), true);
        controller.setNowForTests(start + 6 * hour);
        controller.openInput(QStringLiteral("https://third.example"), true);
        controller.setNowForTests(start + 12 * hour);
        controller.putAwayUnusedTabs();
        QCOMPARE(putAwayUrls(controller), QStringList {QStringLiteral("https://first.example")});
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(putAwayUrls(restored), QStringList {QStringLiteral("https://first.example")});
    restored.setNowForTests(start + 18 * hour - 1);
    restored.putAwayUnusedTabs();
    QCOMPARE(tabUrls(restored),
        QStringList(
            {QStringLiteral("https://second.example"), QStringLiteral("https://third.example")}));
    restored.setNowForTests(start + 18 * hour);
    restored.putAwayUnusedTabs();
    QCOMPARE(tabUrls(restored), QStringList {QStringLiteral("https://third.example")});
    QCOMPARE(putAwayUrls(restored),
        QStringList(
            {QStringLiteral("https://second.example"), QStringLiteral("https://first.example")}));
}

// However long the reader has been looking at it, the page on show is not
// replaced under them, and nor is the page an away Space will show.
void PutAwayTabsTest::neverPutsAwayThePageOnShow()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://reading.example"), false);
    controller->setNowForTests(start + 30 * 24 * hour);
    controller->putAwayUnusedTabs();
    QCOMPARE(tabUrls(*controller), QStringList {QStringLiteral("https://reading.example")});
    QVERIFY(controller->putAwayTabs().isEmpty());
    // A Space left for a month keeps the page it was left on, because that is
    // the page its switch shows.
    const auto personal = controller->activeSpaceId();
    const auto work = controller->createSpace(QStringLiteral("Work"));
    QVERIFY(controller->switchSpace(work));
    controller->openInput(QStringLiteral("https://work-left.example"), false);
    QVERIFY(controller->switchSpace(personal));
    controller->setNowForTests(start + 60 * 24 * hour);
    controller->putAwayUnusedTabs();
    QVERIFY(controller->switchSpace(work));
    QCOMPARE(controller->activeUrl(), QUrl(QStringLiteral("https://work-left.example")));
    QVERIFY(controller->putAwayTabs().isEmpty());
}

void PutAwayTabsTest::neverPutsAwayAPinnedTab()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://pinned.example"), false);
    controller->toggleActivePinned();
    controller->openInput(QStringLiteral("https://reading.example"), true);
    checkADayLater(*controller);
    QCOMPARE(tabUrls(*controller),
        QStringList(
            {QStringLiteral("https://pinned.example"), QStringLiteral("https://reading.example")}));
}

void PutAwayTabsTest::neverPutsAwayAKeepActiveTab()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://kept-active.example"), false);
    controller->toggleActivePinned();
    QVERIFY(controller->toggleActiveKeepActive());
    controller->openInput(QStringLiteral("https://reading.example"), true);
    checkADayLater(*controller);
    QCOMPARE(tabUrls(*controller),
        QStringList({QStringLiteral("https://kept-active.example"),
            QStringLiteral("https://reading.example")}));
}

// The Sounding tab is the one the reader is listening to. Once it is quiet it
// is as unused as it was, and the next check puts it away.
void PutAwayTabsTest::neverPutsAwayATabMakingSound()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://music.example"), false);
    controller->openInput(QStringLiteral("https://reading.example"), true);
    const auto music = tabIdAt(*controller, 0);
    controller->setTabAudible(music, true);
    checkADayLater(*controller);
    QCOMPARE(tabIdAt(*controller, 0), music);

    controller->setTabAudible(music, false);
    controller->putAwayUnusedTabs();
    QCOMPARE(tabUrls(*controller), QStringList {QStringLiteral("https://reading.example")});
}

// An Agent works in tabs the reader is not looking at, so an Agent tab is not
// unused while an Agent is attached to it.
void PutAwayTabsTest::neverPutsAwayAnAgentTabWhileAnAgentIsAttached()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://agent.example"), false);
    controller->openInput(QStringLiteral("https://reading.example"), true);
    const auto agentTab = tabIdAt(*controller, 0);
    controller->setAgentTabIds({agentTab});
    checkADayLater(*controller);
    QCOMPARE(tabIdAt(*controller, 0), agentTab);

    controller->setAgentTabIds({});
    controller->putAwayUnusedTabs();
    QCOMPARE(tabUrls(*controller), QStringList {QStringLiteral("https://reading.example")});
}

// A split is the reader's arrangement of two pages, and taking one half would
// end it, so neither half goes while the reader looks at another tab.
void PutAwayTabsTest::neverPutsAwayEitherTabOfASplit()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://left.example"), false);
    controller->openInput(QStringLiteral("https://right.example"), true);
    QVERIFY(controller->addSplit(tabIdAt(*controller, 0)));
    controller->openInput(QStringLiteral("https://reading.example"), true);
    QVERIFY(!controller->splitOnShow());
    checkADayLater(*controller);
    QCOMPARE(controller->tabs()->rowCount(), 3);
    QVERIFY(controller->putAwayTabs().isEmpty());
}

// A row the reader is holding is in use, whatever its age.
void PutAwayTabsTest::neverPutsAwayATabTheReaderIsDragging()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://dragged.example"), false);
    controller->openInput(QStringLiteral("https://reading.example"), true);
    const auto dragged = tabIdAt(*controller, 0);
    controller->setDraggedTab(dragged);
    checkADayLater(*controller);
    QCOMPARE(tabIdAt(*controller, 0), dragged);

    controller->setDraggedTab({});
    controller->putAwayUnusedTabs();
    QCOMPARE(tabUrls(*controller), QStringList {QStringLiteral("https://reading.example")});
}

// A window that starts after the reader has been away puts away what went
// unused meanwhile, before the first page is shown.
void PutAwayTabsTest::putsAwayAtStartup()
{
    const auto lastWeek = QDateTime::currentMSecsSinceEpoch() - 7 * 24 * hour;
    SessionFixture fixture(SessionSpec {
        .spaces = {SpaceSpec {
            .id = QStringLiteral("personal"),
            .name = QStringLiteral("Personal"),
            .tabs = {
                TabSpec {
                    .id = QStringLiteral("old"),
                    .url = QUrl(QStringLiteral("https://old.example")),
                    .lastShownAt = lastWeek,
                },
                TabSpec {
                    .id = QStringLiteral("reading"),
                    .url = QUrl(QStringLiteral("https://reading.example")),
                    .lastShownAt = lastWeek,
                },
            },
            .activeTabId = QStringLiteral("reading"),
        }},
    });
    QVERIFY_SESSION_READY(fixture);

    const auto controller = fixture.createController();
    QCOMPARE(tabUrls(*controller), QStringList {QStringLiteral("https://reading.example")});
    QCOMPARE(putAwayUrls(*controller), QStringList {QStringLiteral("https://old.example")});
}

// A tab stored before Omaweb kept the time counts from the first start that
// finds it, in every Space, so an upgrade puts nothing away.
void PutAwayTabsTest::countsATabStoredBeforeTheRecordFromTheFirstStart()
{
    SessionFixture fixture(SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = QStringLiteral("personal"),
                .name = QStringLiteral("Personal"),
                .tabs = {
                    TabSpec {
                        .id = QStringLiteral("stored"),
                        .url = QUrl(QStringLiteral("https://stored.example")),
                    },
                    TabSpec {
                        .id = QStringLiteral("reading"),
                        .url = QUrl(QStringLiteral("https://reading.example")),
                    },
                },
                .activeTabId = QStringLiteral("reading"),
            },
            SpaceSpec {
                .id = QStringLiteral("work"),
                .name = QStringLiteral("Work"),
                .tabs = {
                    TabSpec {
                        .id = QStringLiteral("work-stored"),
                        .url = QUrl(QStringLiteral("https://work-stored.example")),
                    },
                    TabSpec {
                        .id = QStringLiteral("work-reading"),
                        .url = QUrl(QStringLiteral("https://work-reading.example")),
                    },
                },
                .activeTabId = QStringLiteral("work-reading"),
            },
        },
        .activeSpaceId = QStringLiteral("personal"),
    });
    QVERIFY_SESSION_READY(fixture);

    const auto before = QDateTime::currentMSecsSinceEpoch();
    const auto controller = fixture.createController();
    const auto after = QDateTime::currentMSecsSinceEpoch();
    QCOMPARE(controller->tabs()->rowCount(), 2);
    QVERIFY(controller->putAwayTabs().isEmpty());

    controller->setNowForTests(before + 12 * hour - 1);
    controller->putAwayUnusedTabs();
    QCOMPARE(controller->tabs()->rowCount(), 2);

    controller->setNowForTests(after + 12 * hour);
    controller->putAwayUnusedTabs();
    QCOMPARE(putAwayUrls(*controller), QStringList {QStringLiteral("https://stored.example")});
    QVERIFY(controller->switchSpace(QStringLiteral("work")));
    QCOMPARE(putAwayUrls(*controller), QStringList {QStringLiteral("https://work-stored.example")});
}

// A Space that is not on show is read from its store, and its unused tabs go
// as the Space on show's do. The page its window still holds goes with them.
void PutAwayTabsTest::putsAwayInASpaceThatIsNotOnShow()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto personal = controller->activeSpaceId();
    const auto work = controller->createSpace(QStringLiteral("Work"));
    QVERIFY(controller->switchSpace(work));
    controller->openInput(QStringLiteral("https://work-old.example"), false);
    controller->openInput(QStringLiteral("https://work-reading.example"), true);
    const auto old = tabIdAt(*controller, 0);
    QVERIFY(controller->switchSpace(personal));

    QSignalSpy discarded(controller.get(), &BrowserController::awayTabDiscarded);
    checkADayLater(*controller);
    QCOMPARE(discarded.size(), 1);
    QCOMPARE(discarded.constFirst().constFirst().toString(), old);

    controller->setNowForTests(start);
    QVERIFY(controller->switchSpace(work));
    QCOMPARE(tabUrls(*controller), QStringList {QStringLiteral("https://work-reading.example")});
    QCOMPARE(putAwayUrls(*controller), QStringList {QStringLiteral("https://work-old.example")});
}

// The Space a switch arrives in is checked as it arrives, so it never shows a
// row that is about to go.
void PutAwayTabsTest::putsAwayOnASpaceSwitch()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto personal = controller->activeSpaceId();
    const auto work = controller->createSpace(QStringLiteral("Work"));
    QVERIFY(controller->switchSpace(work));
    controller->openInput(QStringLiteral("https://work-old.example"), false);
    controller->openInput(QStringLiteral("https://work-reading.example"), true);
    QVERIFY(controller->switchSpace(personal));

    controller->setNowForTests(start + 24 * hour);
    QVERIFY(controller->switchSpace(work));
    QCOMPARE(tabUrls(*controller), QStringList {QStringLiteral("https://work-reading.example")});
    QCOMPARE(putAwayUrls(*controller), QStringList {QStringLiteral("https://work-old.example")});
}

// A window left open checks again by itself.
void PutAwayTabsTest::putsAwayOnAPeriodicCheck()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://old.example"), false);
    controller->openInput(QStringLiteral("https://reading.example"), true);
    controller->setNowForTests(start + 24 * hour);
    controller->setPutAwayCheckIntervalForTests(10);
    QTRY_COMPARE(tabUrls(*controller), QStringList {QStringLiteral("https://reading.example")});
}

// Reopening is reopening a closed tab: a new tab at the address, at the zoom
// and with the muting it had, selected. It leaves the list.
void PutAwayTabsTest::reopensAPutAwayTabWhereItWasLeft()
{
    QTemporaryDir root;
    {
        auto controller = openWindow(root);
        controller->openInput(QStringLiteral("https://zoomed.example/page"), false);
        const auto zoomed = controller->activeTabId();
        controller->setTabZoom(zoomed, 1.5);
        controller->setTabMuted(zoomed, true);
        controller->openInput(QStringLiteral("https://reading.example"), true);
        checkADayLater(*controller);
        QCOMPARE(controller->putAwayTabs().size(), 1);
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto entry = restored.putAwayTabs().constFirst().toMap();
    QCOMPARE(entry.value(QStringLiteral("host")).toString(), QStringLiteral("zoomed.example"));
    QVERIFY(restored.reopenPutAwayTab(entry.value(QStringLiteral("id")).toString()));
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://zoomed.example/page")));
    QCOMPARE(restored.activeTabZoom(), 1.5);
    auto *tabs = restored.tabs();
    QVERIFY(tabs->data(tabs->index(tabs->rowCount() - 1, 0), TabListModel::MutedRole).toBool());
    QVERIFY(restored.putAwayTabs().isEmpty());
    QVERIFY(!restored.reopenPutAwayTab(entry.value(QStringLiteral("id")).toString()));
}

// Thirty days, then the entry goes, whether the window stayed open or not.
void PutAwayTabsTest::keepsAPutAwayTabForThirtyDays()
{
    QTemporaryDir root;
    {
        auto controller = openWindow(root);
        controller->openInput(QStringLiteral("https://old.example"), false);
        controller->openInput(QStringLiteral("https://reading.example"), true);
        controller->setNowForTests(start + 12 * hour);
        controller->putAwayUnusedTabs();
        controller->setNowForTests(start + 12 * hour + 30 * 24 * hour - 1);
        controller->putAwayUnusedTabs();
        QCOMPARE(putAwayUrls(*controller), QStringList {QStringLiteral("https://old.example")});
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    restored.setNowForTests(start + 12 * hour + 30 * 24 * hour);
    restored.putAwayUnusedTabs();
    QVERIFY(restored.putAwayTabs().isEmpty());
}

// The list says where the reader has been, so it is Browsing data: clearing
// the Space's History over a range takes what was put away in that range.
void PutAwayTabsTest::clearsTheListWithTheSpacesHistory()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://earlier.example"), false);
    controller->openInput(QStringLiteral("https://later.example"), true);
    controller->openInput(QStringLiteral("https://reading.example"), true);
    controller->setNowForTests(start + 16 * hour);
    controller->activateTab(tabIdAt(*controller, 1));
    controller->activateTab(tabIdAt(*controller, 2));
    controller->setNowForTests(start + 24 * hour);
    controller->putAwayUnusedTabs();
    QCOMPARE(controller->putAwayTabs().size(), 1);
    controller->setNowForTests(start + 36 * hour);
    controller->putAwayUnusedTabs();
    QCOMPARE(putAwayUrls(*controller),
        QStringList(
            {QStringLiteral("https://later.example"), QStringLiteral("https://earlier.example")}));

    QVERIFY(controller->clearBrowsingData({QStringLiteral("cookies")}, start + 30 * hour));
    QCOMPARE(controller->putAwayTabs().size(), 2);
    QVERIFY(controller->clearBrowsingData({QStringLiteral("history")}, start + 30 * hour));
    QCOMPARE(putAwayUrls(*controller), QStringList {QStringLiteral("https://earlier.example")});
    QVERIFY(controller->clearBrowsingData({QStringLiteral("history")}, 0));
    QVERIFY(controller->putAwayTabs().isEmpty());

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY(restored.putAwayTabs().isEmpty());
}

// Nothing of a deleted Space outlives it, and what it put away is no
// exception.
void PutAwayTabsTest::deletesTheListWithItsSpace()
{
    QTemporaryDir root;
    QString work;
    {
        auto controller = openWindow(root);
        const auto personal = controller->activeSpaceId();
        work = controller->createSpace(QStringLiteral("Work"));
        QVERIFY(controller->switchSpace(work));
        controller->openInput(QStringLiteral("https://work-old.example"), false);
        controller->openInput(QStringLiteral("https://work-reading.example"), true);
        checkADayLater(*controller);
        QCOMPARE(controller->putAwayTabs().size(), 1);
        QVERIFY(controller->switchSpace(personal));
        QVERIFY(controller->deleteSpace(work, QStringLiteral("Work")));
    }

    omaweb::SqliteSessionStore store(root.path());
    QVERIFY(store.open());
    QVERIFY(store.loadPutAwayTabs(work).isEmpty());
}

void PutAwayTabsTest::neverPutsAwayInAPrivateWindow()
{
    PrivateSessionFixture privateSession;
    auto controller = privateSession.createController();
    controller->setNowForTests(start);
    controller->openInput(QStringLiteral("https://private-old.example"), false);
    controller->openInput(QStringLiteral("https://private-reading.example"), true);
    checkADayLater(*controller);
    QCOMPARE(controller->tabs()->rowCount(), 2);
    QVERIFY(controller->putAwayTabs().isEmpty());
}

// Primary+Shift+T walks back what the reader closed, and nothing else: a tab
// Omaweb put away is not among them.
void PutAwayTabsTest::leavesTheRecentlyClosedStackToTheReadersCloses()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://old.example"), false);
    controller->openInput(QStringLiteral("https://reading.example"), true);
    checkADayLater(*controller);
    QCOMPARE(controller->putAwayTabs().size(), 1);
    QCOMPARE(controller->closedTabCount(), 0);
}

// The limit is the reader's, kept with the installation across a restart.
void PutAwayTabsTest::putsAwayAfterTheLimitTheReaderChose()
{
    QTemporaryDir root;
    {
        auto controller = openWindow(root);
        QCOMPARE(controller->putAwayAfterSeconds(), 12 * 60 * 60);
        QVERIFY(controller->setPutAwayAfterSeconds(60 * 60));
    }

    auto controller = openWindow(root);
    QCOMPARE(controller->putAwayAfterSeconds(), 60 * 60);
    controller->openInput(QStringLiteral("https://old.example"), false);
    controller->openInput(QStringLiteral("https://reading.example"), true);
    controller->setNowForTests(start + hour - 1);
    controller->putAwayUnusedTabs();
    QCOMPARE(controller->tabs()->rowCount(), 2);
    controller->setNowForTests(start + hour);
    controller->putAwayUnusedTabs();
    QCOMPARE(tabUrls(*controller), QStringList {QStringLiteral("https://reading.example")});
}

void PutAwayTabsTest::putsNothingAwayWhileTheSettingIsOff()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    QVERIFY(controller->setPutAwayAfterSeconds(0));
    controller->openInput(QStringLiteral("https://old.example"), false);
    controller->openInput(QStringLiteral("https://reading.example"), true);
    controller->setNowForTests(start + 29 * 24 * hour);
    controller->putAwayUnusedTabs();
    QCOMPARE(controller->tabs()->rowCount(), 2);
    QVERIFY(controller->putAwayTabs().isEmpty());
}

// Off, an hour, twelve hours, a day and a week: the choices Settings offers,
// and nothing between them.
void PutAwayTabsTest::offersOnlyTheNamedLimits()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    QSignalSpy changed(controller.get(), &BrowserController::putAwayAfterChanged);
    for (const auto seconds : {0, 60 * 60, 12 * 60 * 60, 24 * 60 * 60, 7 * 24 * 60 * 60}) {
        QVERIFY(controller->setPutAwayAfterSeconds(seconds));
        QCOMPARE(controller->putAwayAfterSeconds(), seconds);
    }
    QCOMPARE(changed.size(), 5);
    QVERIFY(!controller->setPutAwayAfterSeconds(5 * 60));
    QVERIFY(!controller->setPutAwayAfterSeconds(-1));
    QCOMPARE(controller->putAwayAfterSeconds(), 7 * 24 * 60 * 60);
}

// The first time tabs go, the reader is told, once for the installation:
// not again after it is dismissed, and not in a window started later.
void PutAwayTabsTest::saysSoOnceTheFirstTimeTabsArePutAway()
{
    QTemporaryDir root;
    {
        auto controller = openWindow(root);
        controller->openInput(QStringLiteral("https://first.example"), false);
        controller->openInput(QStringLiteral("https://second.example"), true);
        controller->openInput(QStringLiteral("https://reading.example"), true);
        QVERIFY(!controller->putAwayNotice());
        QSignalSpy noticed(controller.get(), &BrowserController::putAwayNoticeChanged);
        checkADayLater(*controller);
        QVERIFY(controller->putAwayNotice());
        QCOMPARE(noticed.size(), 1);

        controller->dismissPutAwayNotice();
        QVERIFY(!controller->putAwayNotice());
        controller->openInput(QStringLiteral("https://third.example"), true);
        controller->setNowForTests(start + 48 * hour);
        controller->putAwayUnusedTabs();
        QCOMPARE(controller->putAwayTabs().size(), 3);
        QVERIFY(!controller->putAwayNotice());
    }

    auto controller = openWindow(root);
    controller->openInput(QStringLiteral("https://fourth.example"), true);
    controller->setNowForTests(start + 72 * hour);
    controller->putAwayUnusedTabs();
    QVERIFY(!controller->putAwayNotice());
}

namespace {

// A Space whose one ordinary tab besides the active one was last shown a week
// ago, by the wall clock, so the check a window makes as it starts finds it.
SessionSpec spaceWithATabLastShownLastWeek()
{
    const auto lastWeek = QDateTime::currentMSecsSinceEpoch() - 7 * 24 * hour;
    return SessionSpec {
        .spaces = {SpaceSpec {
            .id = QStringLiteral("personal"),
            .name = QStringLiteral("Personal"),
            .tabs = {
                TabSpec {
                    .id = QStringLiteral("old"),
                    .url = QUrl(QStringLiteral("https://old.example")),
                    .lastShownAt = lastWeek,
                },
                TabSpec {
                    .id = QStringLiteral("reading"),
                    .url = QUrl(QStringLiteral("https://reading.example")),
                    .lastShownAt = lastWeek,
                },
            },
            .activeTabId = QStringLiteral("reading"),
        }},
    };
}

} // namespace

// The check a window makes as it starts reads the limit the reader chose, not
// the default: with it off, nothing goes however old.
void PutAwayTabsTest::readsTheReadersLimitAtStartup()
{
    QTemporaryDir config;
    QVERIFY(config.isValid());
    QVERIFY(
        omaweb::SettingsFile(config.path()).set(QStringLiteral("put-away-unused-tabs-after"), 0));
    SessionFixture fixture(spaceWithATabLastShownLastWeek(), config.path());
    QVERIFY_SESSION_READY(fixture);

    const auto controller = fixture.createController();
    QCOMPARE(controller->tabs()->rowCount(), 2);
    QVERIFY(controller->putAwayTabs().isEmpty());
}

// After an upgrade the first tabs to go are likely to go as the window starts,
// and the reader is told then as at any other time.
void PutAwayTabsTest::saysSoWhenTheFirstPutAwayIsAtStartup()
{
    SessionFixture fixture(spaceWithATabLastShownLastWeek());
    QVERIFY_SESSION_READY(fixture);
    const auto controller = fixture.createController();
    QCOMPARE(putAwayUrls(*controller), QStringList {QStringLiteral("https://old.example")});
    QVERIFY(controller->putAwayNotice());
}

// Sync writes a Space's tabs knowing nothing of when each was last on show. A
// write like that keeps the time the store had rather than starting the count
// again, or a Space that syncs often would never put a tab away.
void PutAwayTabsTest::keepsATabsLastShowingThroughAWriteThatDoesNotKnowIt()
{
    QTemporaryDir root;
    QString spaceId;
    {
        auto controller = openWindow(root);
        spaceId = controller->activeSpaceId();
        controller->openInput(QStringLiteral("https://old.example"), false);
        controller->openInput(QStringLiteral("https://reading.example"), true);
    }
    {
        omaweb::SqliteSessionStore store(root.path());
        QVERIFY(store.open());
        auto tabs = store.loadTabs(spaceId);
        QString activeTabId;
        for (auto &tab : tabs) {
            tab.lastShownAt = 0;
            if (tab.active) {
                activeTabId = tab.id;
            }
        }
        QVERIFY(store.saveTabs(spaceId, tabs, activeTabId));
    }

    auto controller = openWindow(root);
    controller->setNowForTests(start + 12 * hour - 1);
    controller->putAwayUnusedTabs();
    QVERIFY(controller->putAwayTabs().isEmpty());
    controller->setNowForTests(start + 12 * hour);
    controller->putAwayUnusedTabs();
    QCOMPARE(putAwayUrls(*controller), QStringList {QStringLiteral("https://old.example")});
}

QTEST_GUILESS_MAIN(PutAwayTabsTest)

#include "tst_putawaytabs.moc"
