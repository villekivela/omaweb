#include "BrowserController.h"
#include "PrivateSessionFixture.h"
#include "SessionFixture.h"
#include "SpaceStorage.h"
#include "TabListModel.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::BrowserController;
using omaweb::SpaceStorage;
using omaweb::TabListModel;
using omaweb::test::PrivateSessionFixture;
using omaweb::test::SessionFixture;
using omaweb::test::SessionSpec;
using omaweb::test::SpaceSpec;
using omaweb::test::TabSpec;

namespace {

QString tabIdAt(BrowserController &controller, int row)
{
    auto *tabs = controller.tabs();
    return tabs->data(tabs->index(row, 0), TabListModel::IdRole).toString();
}

QStringList tabWindowIds(const BrowserController &controller)
{
    QStringList ids;
    for (const auto &entry : controller.tabWindows()) {
        ids.append(entry.toMap().value(QStringLiteral("tabId")).toString());
    }
    return ids;
}

} // namespace

class TabWindowsTest final : public QObject {
    Q_OBJECT

private slots:
    void popsTheActiveTabOutAndShowsTheNextOne();
    void bringsTabWindowsBackAfterARestartWithTheirStrips();
    void raisesTheTabWindowInsteadOfSelectingItsTab();
    void keepsATabWindowsPageRunningWhileAnotherSpaceIsOnShow();
    void popsAPinnedTabOutWithItsPin();
    void endsTheSplitOfATabPoppedOut();
    void restsTheSpaceWhenItsLastPageIsPoppedOut();
    void popsOutNoBlankTab();
    void putsATabBackInTheSidebar();
    void putsATabOfAnotherSpaceBackAndShowsIt();
    void closesAPoppedOutTabOntoTheRecentlyClosedStack();
    void closesTheTabWindowsOfADeletedSpace();
    void namesTheSpaceOfEachTabWindow();
    void keepsWhatAnAwayTabWindowsPageReports();
    void keepsAPoppedOutTabInItsSpace();
};

// Popping out the tab on show moves it to a Tab window of its own. It stays a
// tab of its Space, marked in the sidebar, and the main window shows the tab
// closing it would have shown.
void TabWindowsTest::popsTheActiveTabOutAndShowsTheNextOne()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://first.example"), false);
    const auto firstId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://second.example"), true);
    const auto secondId = controller.activeTabId();

    QSignalSpy windowsSpy(&controller, &BrowserController::tabWindowsChanged);
    QVERIFY(controller.popOutTab(secondId));

    QCOMPARE(controller.activeTabId(), firstId);
    QCOMPARE(controller.tabs()->rowCount(), 2);
    QCOMPARE(tabIdAt(controller, 1), secondId);
    auto *tabs = controller.tabs();
    QVERIFY(tabs->data(tabs->index(1, 0), TabListModel::PoppedOutRole).toBool());
    QVERIFY(!tabs->data(tabs->index(0, 0), TabListModel::PoppedOutRole).toBool());
    QVERIFY(controller.tabPoppedOut(secondId));
    QCOMPARE(windowsSpy.count(), 1);
    QCOMPARE(tabWindowIds(controller), QStringList {secondId});
    const auto window = controller.tabWindows().first().toMap();
    QCOMPARE(window.value(QStringLiteral("spaceId")).toString(), controller.activeSpaceId());
    QCOMPARE(window.value(QStringLiteral("url")).toUrl(),
        QUrl(QStringLiteral("https://second.example")));

    // Asked twice, it is already out.
    QVERIFY(!controller.popOutTab(secondId));
}

// Being popped out, and a hidden strip, are kept with the session, so a
// restart brings the Tab window back as it was left.
void TabWindowsTest::bringsTabWindowsBackAfterARestartWithTheirStrips()
{
    QTemporaryDir root;
    QString poppedId;
    QString shownId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        controller.openInput(QStringLiteral("https://shown.example"), false);
        shownId = controller.activeTabId();
        controller.openInput(QStringLiteral("https://popped.example"), true);
        poppedId = controller.activeTabId();
        QVERIFY(controller.popOutTab(poppedId));
        QVERIFY(controller.setTabStripHidden(poppedId, true));
    }

    BrowserController restarted(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(restarted.activeTabId(), shownId);
    QCOMPARE(tabWindowIds(restarted), QStringList {poppedId});
    QVERIFY(restarted.tabWindows().first().toMap().value(QStringLiteral("stripHidden")).toBool());
    auto *tabs = restarted.tabs();
    QVERIFY(tabs->data(tabs->index(1, 0), TabListModel::PoppedOutRole).toBool());

    // Shown again, the strip stays shown through the next restart.
    QVERIFY(restarted.setTabStripHidden(poppedId, false));
    QVERIFY(!restarted.tabWindows().first().toMap().value(QStringLiteral("stripHidden")).toBool());
}

// The main window never shows a popped-out tab. Selecting its row raises the
// Tab window and leaves the main window as it was, and stepping through the
// list passes over it.
void TabWindowsTest::raisesTheTabWindowInsteadOfSelectingItsTab()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://first.example"), false);
    const auto firstId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://popped.example"), true);
    const auto poppedId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://third.example"), true);
    const auto thirdId = controller.activeTabId();
    QVERIFY(controller.popOutTab(poppedId));
    QCOMPARE(controller.activeTabId(), thirdId);

    QSignalSpy raiseSpy(&controller, &BrowserController::tabWindowRaiseRequested);
    QSignalSpy activeSpy(&controller, &BrowserController::activeTabChanged);
    controller.activateTab(poppedId);
    QCOMPARE(raiseSpy.count(), 1);
    QCOMPARE(raiseSpy.first().first().toString(), poppedId);
    QCOMPARE(controller.activeTabId(), thirdId);
    QCOMPARE(activeSpy.count(), 0);

    controller.stepTab(-1);
    QCOMPARE(controller.activeTabId(), firstId);
    controller.stepTab(1);
    QCOMPARE(controller.activeTabId(), thirdId);

    // A jump back passes over it too.
    QVERIFY(controller.jumpBack());
    QCOMPARE(controller.activeTabId(), firstId);

    // Nor can it join a split while it is out.
    QVERIFY(!controller.splittableTabIds().contains(poppedId));
    QVERIFY(!controller.addSplit(poppedId));
}

// A popped-out tab's page runs whichever Space the main window shows. It is
// named among the retained tabs as a Keep active tab is, and stopping it there
// is refused: the window showing it is the reader's to close.
void TabWindowsTest::keepsATabWindowsPageRunningWhileAnotherSpaceIsOnShow()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalId = controller.activeSpaceId();
    controller.openInput(QStringLiteral("https://popped.example"), false);
    const auto poppedId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://other.example"), true);
    QVERIFY(controller.popOutTab(poppedId));
    QCOMPARE(controller.retainedTabIds(), QStringList {poppedId});

    const auto workId = controller.createSpace(QStringLiteral("Work"));
    QSignalSpy suspendedSpy(&controller, &BrowserController::spaceSuspended);
    QVERIFY(controller.switchSpace(workId));
    QCOMPARE(suspendedSpy.count(), 1);
    QCOMPARE(suspendedSpy.first().at(1).toStringList(), QStringList {poppedId});
    QCOMPARE(controller.retainedTabs().size(), 1);
    const auto retained = controller.retainedTabs().first().toMap();
    QCOMPARE(retained.value(QStringLiteral("tabId")).toString(), poppedId);
    QCOMPARE(retained.value(QStringLiteral("spaceId")).toString(), personalId);
    QVERIFY(retained.value(QStringLiteral("poppedOut")).toBool());
    QVERIFY(!controller.releaseRetainedTab(poppedId));

    // Still listed with its window after the switch.
    QCOMPARE(tabWindowIds(controller), QStringList {poppedId});
    const auto window = controller.tabWindows().first().toMap();
    QCOMPARE(window.value(QStringLiteral("spaceName")).toString(), QStringLiteral("Personal"));
}

void TabWindowsTest::popsAPinnedTabOutWithItsPin()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://pinned.example"), false);
    const auto pinnedId = controller.activeTabId();
    controller.toggleActivePinned();
    controller.openInput(QStringLiteral("https://reading.example"), true);
    const auto readingId = controller.activeTabId();
    controller.activateTab(pinnedId);

    QVERIFY(controller.popOutTab(pinnedId));
    QVERIFY(controller.tabPinned(pinnedId));
    QCOMPARE(controller.activeTabId(), readingId);
    QVERIFY(controller.tabWindows().first().toMap().value(QStringLiteral("pinned")).toBool());
}

// Closing either half of a split shows the other, so popping one out does
// the same, and leaves the two as ordinary rows.
void TabWindowsTest::endsTheSplitOfATabPoppedOut()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://left.example"), false);
    const auto leftId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://elsewhere.example"), true);
    controller.openInput(QStringLiteral("https://right.example"), true);
    const auto rightId = controller.activeTabId();
    QVERIFY(controller.addSplit(leftId));
    controller.activateTab(rightId);
    QVERIFY(controller.splitOnShow());

    QVERIFY(controller.popOutTab(rightId));
    QVERIFY(!controller.splitOnShow());
    QVERIFY(!controller.tabInSplit(leftId));
    QVERIFY(!controller.tabInSplit(rightId));
    QCOMPARE(controller.activeTabId(), leftId);
}

// With nothing left in the main window to show, the Space rests on a blank tab
// as it does when its last page closes. The popped-out tab is still listed.
void TabWindowsTest::restsTheSpaceWhenItsLastPageIsPoppedOut()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://only.example"), false);
    const auto onlyId = controller.activeTabId();
    QVERIFY(!controller.atRest());

    QVERIFY(controller.popOutTab(onlyId));
    QVERIFY(controller.activeTabId() != onlyId);
    QVERIFY(controller.activeTabBlank());
    QVERIFY(controller.atRest());
    QCOMPARE(controller.tabs()->rowCount(), 2);
    QCOMPARE(tabIdAt(controller, 0), onlyId);
}

void TabWindowsTest::popsOutNoBlankTab()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY(controller.activeTabBlank());
    QVERIFY(!controller.popOutTab(controller.activeTabId()));
    QVERIFY(!controller.popOutTab(QStringLiteral("no-such-tab")));
    QVERIFY(controller.tabWindows().isEmpty());
}

// Closing the window puts the tab back where it is listed and leaves the main
// window as it was; put-back-tab shows it there too.
void TabWindowsTest::putsATabBackInTheSidebar()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://popped.example"), false);
    const auto poppedId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://reading.example"), true);
    const auto readingId = controller.activeTabId();
    QVERIFY(controller.popOutTab(poppedId));

    QSignalSpy windowsSpy(&controller, &BrowserController::tabWindowsChanged);
    QVERIFY(controller.putBackTab(poppedId, false));
    QCOMPARE(windowsSpy.count(), 1);
    QVERIFY(controller.tabWindows().isEmpty());
    QVERIFY(!controller.tabPoppedOut(poppedId));
    auto *tabs = controller.tabs();
    QVERIFY(!tabs->data(tabs->index(0, 0), TabListModel::PoppedOutRole).toBool());
    QCOMPARE(controller.activeTabId(), readingId);
    QVERIFY(!controller.putBackTab(poppedId, false));

    QVERIFY(controller.popOutTab(poppedId));
    QVERIFY(controller.putBackTab(poppedId, true));
    QCOMPARE(controller.activeTabId(), poppedId);
}

void TabWindowsTest::putsATabOfAnotherSpaceBackAndShowsIt()
{
    QTemporaryDir root;
    QString poppedId;
    QString personalId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        personalId = controller.activeSpaceId();
        controller.openInput(QStringLiteral("https://popped.example"), false);
        poppedId = controller.activeTabId();
        controller.openInput(QStringLiteral("https://reading.example"), true);
        QVERIFY(controller.popOutTab(poppedId));
        const auto workId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workId));

        QVERIFY(controller.putBackTab(poppedId, true));
        QCOMPARE(controller.activeSpaceId(), personalId);
        QCOMPARE(controller.activeTabId(), poppedId);
        QVERIFY(controller.tabWindows().isEmpty());
        QVERIFY(controller.retainedTabs().isEmpty());

        // Put back from away without showing it, the main window stays.
        QVERIFY(controller.popOutTab(poppedId));
        QVERIFY(controller.switchSpace(workId));
        QVERIFY(controller.putBackTab(poppedId, false));
        QCOMPARE(controller.activeSpaceId(), workId);
        QVERIFY(controller.tabWindows().isEmpty());
        QVERIFY(controller.retainedTabs().isEmpty());
    }
    BrowserController restarted(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY(restarted.tabWindows().isEmpty());
}

// Close-tab in the Tab window closes the tab, which the Space can take back,
// whether its Space is on show or not. Taken back, it is in the main window.
void TabWindowsTest::closesAPoppedOutTabOntoTheRecentlyClosedStack()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalId = controller.activeSpaceId();
    controller.openInput(QStringLiteral("https://first.example"), false);
    const auto firstId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://second.example"), true);
    const auto secondId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://reading.example"), true);
    QVERIFY(controller.popOutTab(firstId));
    QVERIFY(controller.popOutTab(secondId));

    controller.closeTab(firstId);
    QCOMPARE(tabWindowIds(controller), QStringList {secondId});
    QCOMPARE(controller.closedTabCount(), 1);

    const auto workId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workId));
    QVERIFY(controller.closeTabInSpace(secondId));
    QVERIFY(controller.tabWindows().isEmpty());
    QVERIFY(controller.retainedTabs().isEmpty());

    QVERIFY(controller.switchSpace(personalId));
    QCOMPARE(controller.closedTabCount(), 2);
    controller.reopenClosedTab();
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://second.example")));
    QVERIFY(controller.tabWindows().isEmpty());
}

void TabWindowsTest::closesTheTabWindowsOfADeletedSpace()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalId = controller.activeSpaceId();
    const auto workId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workId));
    controller.openInput(QStringLiteral("https://popped.example"), false);
    const auto poppedId = controller.activeTabId();
    QVERIFY(controller.popOutTab(poppedId));
    QVERIFY(controller.switchSpace(personalId));
    QCOMPARE(tabWindowIds(controller), QStringList {poppedId});

    QVERIFY(controller.deleteSpace(workId, QStringLiteral("Work")));
    QVERIFY(controller.tabWindows().isEmpty());
    QVERIFY(controller.retainedTabs().isEmpty());
}

void TabWindowsTest::namesTheSpaceOfEachTabWindow()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalId = controller.activeSpaceId();
    controller.openInput(QStringLiteral("https://popped.example"), false);
    QVERIFY(controller.popOutTab(controller.activeTabId()));
    const auto workId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workId));

    QVERIFY(controller.renameSpace(personalId, QStringLiteral("Home")));
    QVERIFY(controller.setSpaceColour(personalId, QStringLiteral("violet")));
    const auto window = controller.tabWindows().first().toMap();
    QCOMPARE(window.value(QStringLiteral("spaceName")).toString(), QStringLiteral("Home"));
    QCOMPARE(window.value(QStringLiteral("spaceColor")).toString(), QStringLiteral("violet"));
}

// The page in a Tab window goes on reporting while its Space is away, and the
// tab keeps what it reports, as it would in the sidebar: the address, the title
// and the zoom are what its Space saves and syncs.
void TabWindowsTest::keepsWhatAnAwayTabWindowsPageReports()
{
    QTemporaryDir root;
    QString poppedId;
    QString personalId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        personalId = controller.activeSpaceId();
        controller.openInput(QStringLiteral("https://popped.example"), false);
        poppedId = controller.activeTabId();
        QVERIFY(controller.popOutTab(poppedId));
        const auto workId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workId));

        controller.reportTabPageState(poppedId, QUrl(QStringLiteral("https://moved.example/page")),
            QStringLiteral("Moved"), {}, false, false);
        controller.setTabZoom(poppedId, 1.25);
        const auto window = controller.tabWindows().first().toMap();
        QCOMPARE(window.value(QStringLiteral("url")).toUrl(),
            QUrl(QStringLiteral("https://moved.example/page")));
        QCOMPARE(window.value(QStringLiteral("title")).toString(), QStringLiteral("Moved"));
        QCOMPARE(controller.history(QStringLiteral("moved")).size(), 0);

        // A finished load is a visit of its own Space, not of the one on show.
        controller.reportTabPageState(poppedId, QUrl(QStringLiteral("https://moved.example/page")),
            QStringLiteral("Moved"), {}, true, false);
        controller.reportTabPageState(poppedId, QUrl(QStringLiteral("https://moved.example/page")),
            QStringLiteral("Moved"), {}, false, false);
        QCOMPARE(controller.history(QStringLiteral("moved")).size(), 0);
        QVERIFY(controller.switchSpace(personalId));
        QCOMPARE(controller.history(QStringLiteral("moved")).size(), 1);
    }
    BrowserController restarted(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto tab = restarted.findTab(poppedId);
    QVERIFY(tab.has_value());
    QCOMPARE(tab->url, QUrl(QStringLiteral("https://moved.example/page")));
    QCOMPARE(tab->title, QStringLiteral("Moved"));
    QCOMPARE(tab->zoom, 1.25);
    QVERIFY(tab->poppedOut);
}

// Moving a tab to another Space is a main window command about its sidebar,
// and a tab shown in a Tab window is put back before it can move.
void TabWindowsTest::keepsAPoppedOutTabInItsSpace()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://popped.example"), false);
    const auto poppedId = controller.activeTabId();
    QVERIFY(controller.popOutTab(poppedId));
    const auto workId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(!controller.requestTabMoveToSpace(poppedId, workId, false));
    QVERIFY(!controller.confirmTabMoveToSpace(poppedId, workId));
    QVERIFY(controller.tabPoppedOut(poppedId));
}

QTEST_GUILESS_MAIN(TabWindowsTest)

#include "tst_tabwindows.moc"
