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
    void answersTheTabWindowsOwnCommandsInAnySpace();
    void keepsATabWindowsPermissionAnswersInItsOwnSpace();
    void opensANewTabWindowInTheSpaceOfTheOneThatAsked();
    void popsOutATabOfASpaceNotOnShow();
    void showsAnotherTabWhenTheSessionWasLeftOnAPoppedOutOne();
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

    // Chosen from another Space, as the Omnibar or a notification chooses a
    // tab, the main window stays on its Space and the Tab window comes up.
    const auto workId = controller.createSpace(QStringLiteral("Work"));
    const auto personalId = controller.activeSpaceId();
    QVERIFY(controller.switchSpace(workId));
    raiseSpy.clear();
    QVERIFY(controller.activateTabInSpace(personalId, poppedId));
    QCOMPARE(controller.activeSpaceId(), workId);
    QCOMPARE(raiseSpy.count(), 1);
    QVERIFY(controller.switchSpace(personalId));
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
    // The window stays open through the switch: its tab is never, even for a
    // moment, missing from the list.
    QSignalSpy windowsSpy(&controller, &BrowserController::tabWindowsChanged);
    QVERIFY(controller.switchSpace(workId));
    QCOMPARE(windowsSpy.count(), 0);
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

// Zoom and the inspector are the Tab window's tab's own, whichever Space the
// main window shows, and what the Omnibar there opens is the same address it
// would open in the main window.
void TabWindowsTest::answersTheTabWindowsOwnCommandsInAnySpace()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://popped.example"), false);
    const auto poppedId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://reading.example"), true);
    const auto readingId = controller.activeTabId();
    QVERIFY(controller.popOutTab(poppedId));

    controller.stepTabZoom(poppedId, 1);
    QCOMPARE(controller.tabWindows().first().toMap().value(QStringLiteral("zoom")).toDouble(), 1.1);
    QCOMPARE(controller.activeTabZoom(), 1.0);

    const auto workId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workId));
    controller.stepTabZoom(poppedId, 1);
    QCOMPARE(
        controller.tabWindows().first().toMap().value(QStringLiteral("zoom")).toDouble(), 1.25);
    controller.resetTabZoom(poppedId);
    QCOMPARE(controller.tabWindows().first().toMap().value(QStringLiteral("zoom")).toDouble(), 1.0);

    controller.toggleTabDeveloperTools(poppedId);
    QCOMPARE(controller.developerToolsTabId(), poppedId);
    QVERIFY(!controller.activeTabInspected());
    controller.toggleTabDeveloperTools(poppedId);
    QVERIFY(controller.developerToolsTabId().isEmpty());
    // Only a Tab window's tab is inspected this way.
    controller.toggleTabDeveloperTools(readingId);
    QVERIFY(controller.developerToolsTabId().isEmpty());

    QCOMPARE(controller.addressFor(QStringLiteral("example.com")),
        QUrl(QStringLiteral("https://example.com")));
}

// A Tab window's page may belong to a Space that is not on show. What the
// reader answers there is that Space's to keep, as Spaces keep their logins
// apart, and the page is asked of that Space's answers.
void TabWindowsTest::keepsATabWindowsPermissionAnswersInItsOwnSpace()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalId = controller.activeSpaceId();
    const auto workId = controller.createSpace(QStringLiteral("Work"));
    const QUrl site(QStringLiteral("https://camera.example"));

    QVERIFY(controller.setPermissionDecision(
        site, QStringLiteral("camera"), BrowserController::Block, workId));
    QCOMPARE(controller.permissionDecision(site, QStringLiteral("camera")), BrowserController::Ask);
    QCOMPARE(controller.permissionDecision(site, QStringLiteral("camera"), workId),
        BrowserController::Block);
    QCOMPARE(controller.permissionDecision(site, QStringLiteral("camera"), personalId),
        BrowserController::Ask);

    QVERIFY(controller.switchSpace(workId));
    QCOMPARE(
        controller.permissionDecision(site, QStringLiteral("camera")), BrowserController::Block);
}

// A link in a Tab window that asks for a window of its own gets a new Tab
// window, in the same Space, and the main window neither switches Space nor
// changes the tab it shows.
void TabWindowsTest::opensANewTabWindowInTheSpaceOfTheOneThatAsked()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalId = controller.activeSpaceId();
    controller.openInput(QStringLiteral("https://reading.example"), false);
    const auto readingId = controller.activeTabId();
    const auto workId = controller.createSpace(QStringLiteral("Work"));

    const auto openedHere
        = controller.openTabWindow(personalId, QUrl(QStringLiteral("https://here.example")));
    QVERIFY(!openedHere.isEmpty());
    QCOMPARE(controller.activeTabId(), readingId);
    QVERIFY(controller.tabPoppedOut(openedHere));

    const auto openedAway
        = controller.openTabWindow(workId, QUrl(QStringLiteral("https://away.example")));
    QVERIFY(!openedAway.isEmpty());
    QCOMPARE(controller.activeSpaceId(), personalId);
    QCOMPARE(controller.activeTabId(), readingId);
    QCOMPARE(tabWindowIds(controller), QStringList({openedHere, openedAway}));
    QCOMPARE(
        controller.tabWindows().at(1).toMap().value(QStringLiteral("spaceId")).toString(), workId);
    QCOMPARE(controller.retainedTabs().size(), 1);

    QVERIFY(controller
            .openTabWindow(
                QStringLiteral("no-such-space"), QUrl(QStringLiteral("https://nowhere.example")))
            .isEmpty());
    QVERIFY(controller.openTabWindow(personalId, QUrl(QStringLiteral("about:blank"))).isEmpty());

    PrivateSessionFixture privateSession;
    auto privateController = privateSession.createController();
    QVERIFY(privateController
            ->openTabWindow(
                privateController->activeSpaceId(), QUrl(QStringLiteral("https://private.example")))
            .isEmpty());
}

// `omaweb run pop-out-tab` names a tab of any Space. One whose Space is not on
// show pops out where it is, and that Space is left on another tab, as it
// would be had it been on show.
void TabWindowsTest::popsOutATabOfASpaceNotOnShow()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalId = controller.activeSpaceId();
    const auto workId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workId));
    controller.openInput(QStringLiteral("https://first.example"), false);
    const auto firstId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://second.example"), true);
    const auto secondId = controller.activeTabId();
    QVERIFY(controller.switchSpace(personalId));
    const auto shownId = controller.activeTabId();

    QVERIFY(controller.popOutTab(secondId));
    QCOMPARE(controller.activeSpaceId(), personalId);
    QCOMPARE(controller.activeTabId(), shownId);
    QCOMPARE(tabWindowIds(controller), QStringList {secondId});
    QCOMPARE(controller.retainedTabs().size(), 1);
    QVERIFY(!controller.popOutTab(secondId));

    QVERIFY(controller.switchSpace(workId));
    QCOMPARE(controller.activeTabId(), firstId);
    QVERIFY(controller.tabPoppedOut(secondId));
}

// A session can name a popped-out tab as its Space's active one, as a Sync
// apply that knew nothing of Tab windows may leave it. The main window shows
// another tab, or rests, and the Tab window still has its tab.
void TabWindowsTest::showsAnotherTabWhenTheSessionWasLeftOnAPoppedOutOne()
{
    SessionFixture fixture(SessionSpec {
        .spaces = {SpaceSpec {
            .id = QStringLiteral("personal"),
            .name = QStringLiteral("Personal"),
            .tabs = {TabSpec {
                         .id = QStringLiteral("reading"),
                         .url = QUrl(QStringLiteral("https://reading.example")),
                     },
                TabSpec {
                    .id = QStringLiteral("popped"),
                    .url = QUrl(QStringLiteral("https://popped.example")),
                    .poppedOut = true,
                }},
            .activeTabId = QStringLiteral("popped"),
        }},
        .activeSpaceId = QStringLiteral("personal"),
    });
    QVERIFY_SESSION_READY(fixture);
    const auto controller = fixture.createController();
    QCOMPARE(controller->activeTabId(), QStringLiteral("reading"));
    QCOMPARE(tabWindowIds(*controller), QStringList {QStringLiteral("popped")});

    SessionFixture alone(SessionSpec {
        .spaces = {SpaceSpec {
            .id = QStringLiteral("personal"),
            .name = QStringLiteral("Personal"),
            .tabs = {TabSpec {
                .id = QStringLiteral("popped"),
                .url = QUrl(QStringLiteral("https://popped.example")),
                .poppedOut = true,
            }},
            .activeTabId = QStringLiteral("popped"),
        }},
        .activeSpaceId = QStringLiteral("personal"),
    });
    QVERIFY_SESSION_READY(alone);
    const auto resting = alone.createController();
    QVERIFY(resting->activeTabId() != QStringLiteral("popped"));
    QVERIFY(resting->atRest());
    QCOMPARE(tabWindowIds(*resting), QStringList {QStringLiteral("popped")});
}

QTEST_GUILESS_MAIN(TabWindowsTest)

#include "tst_tabwindows.moc"
