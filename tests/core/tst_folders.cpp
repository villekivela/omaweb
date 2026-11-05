#include "BrowserController.h"
#include "PrivateSessionFixture.h"
#include "SidebarRowModel.h"
#include "SpaceStorage.h"
#include "SqliteSessionStore.h"
#include "TabListModel.h"

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using omaweb::BrowserController;
using omaweb::FolderState;
using omaweb::SidebarRowModel;
using omaweb::SpaceStorage;
using omaweb::SqliteSessionStore;
using omaweb::TabListModel;
using omaweb::test::PrivateSessionFixture;

namespace {

constexpr qint64 hour = 60 * 60 * 1000;
// A fixed moment in 2100, so the check a window makes as it starts reads no
// tab here as old.
constexpr qint64 start = 4'102'444'800'000;

std::unique_ptr<BrowserController> openWindow(const QTemporaryDir &root)
{
    auto controller = std::make_unique<BrowserController>(
        SpaceStorage(root.path(), QStringLiteral("test")), root.filePath(QStringLiteral("config")));
    controller->setNowForTests(start);
    return controller;
}

QString open(BrowserController &controller, const char *host, bool inNewTab = true)
{
    controller.openInput(QStringLiteral("https://%1.example").arg(QLatin1String(host)), inNewTab);
    return controller.activeTabId();
}

// The sidebar's list, read as the reader reads it: a Folder by its name, a tab
// by its host's first word, the hairline as a line. A row a collapsed Folder
// keeps out of the list is left out.
QStringList rows(BrowserController &controller)
{
    QStringList read;
    auto *model = controller.sidebarRows();
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto index = model->index(row, 0);
        const auto kind = model->data(index, SidebarRowModel::RowKindRole).toString();
        if (kind == QStringLiteral("folder")) {
            read.append(QStringLiteral("[%1]").arg(
                model->data(index, SidebarRowModel::FolderNameRole).toString()));
        } else if (kind == QStringLiteral("divider")) {
            read.append(QStringLiteral("---"));
        } else if (!model->data(index, TabListModel::FolderHiddenRole).toBool()) {
            const auto host = model->data(index, TabListModel::UrlRole).toUrl().host();
            const auto inFolder
                = !model->data(index, TabListModel::FolderIdRole).toString().isEmpty();
            read.append((inFolder ? QStringLiteral("  ") : QString {}) + host.section(u'.', 0, 0));
        }
    }
    return read;
}

int memberCount(BrowserController &controller, const QString &name)
{
    auto *model = controller.sidebarRows();
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto index = model->index(row, 0);
        if (model->data(index, SidebarRowModel::FolderNameRole).toString() == name) {
            return model->data(index, SidebarRowModel::FolderMemberCountRole).toInt();
        }
    }
    return -1;
}

} // namespace

class FoldersTest final : public QObject {
    Q_OBJECT

private slots:
    void aFolderStandsBetweenThePinsAndTheOrdinaryTabs();
    void theHairlineIsDrawnOnlyWhenBothAreThere();
    void aFolderIsRefusedAnEmptyNameAndMayShareOne();
    void renamingRefusesAnEmptyName();
    void foldersReorderWithinTheirBlock();
    void aFolderOutlivesItsTabs();
    void aSplitsRowIsOneMember();
    void pinnedTabsStayInTheirSection();
    void aNewTabJoinsNoFolder();
    void aDuplicatedTabJoinsNoFolder();
    void aReopenedTabJoinsNoFolder();
    void pinningATabTakesItOutOfItsFolder();
    void aPoppedOutTabKeepsItsRowInTheFolder();
    void addingASplitBringsBothIntoTheRowsFolder();
    void addingASplitWithAFolderTabBringsTheActiveTabIn();
    void aBlankSplitStaysBesideItsTabInTheFolder();
    void separatingLeavesBothInTheFolder();
    void closeOtherTabsNeverTouchesAFoldersTabs();
    void closeTabsBelowNeverTouchesAFoldersTabs();
    void aSpaceWithOnlyFolderTabsLeftComesToRest();
    void closingTheLastTabOfASpaceLeavesTheFolder();
    void closingAFolderTabShowsItsNeighbourInTheFolder();
    void droppingOnAFolderAddsAtItsEndAndLeavesItAsItWas();
    void rowsMoveInAndOutOfAFolder();
    void aSplitsRowEntersAndLeavesWhole();
    void theMoveKeysCarryARowInAndOut();
    void theMoveKeysCarryASplitAcrossAndNoFurther();
    void collapsedTabsLeaveTheList();
    void aCollapsedFolderStaysCollapsedWhenItsTabComesOnShow();
    void bothHalvesOfASplitOnShowStayInTheList();
    void nextAndPreviousTabPassOverACollapsedFolder();
    void theTabKeysCountWhatIsOnShow();
    void removingAFolderLeavesItsTabs();
    void closingAFolderClosesItsTabsIntoTheStack();
    void closingAFolderOnShowComesToRest();
    void movingAFolderTakesItToTheEndOfThatSpacesBlock();
    void movingAFolderOnShowShowsAnotherTab();
    void movingAFolderAsksAboutThePageOnShow();
    void aTabWindowStaysBehindWhenItsFolderMoves();
    void movingATabToASpaceTakesItOutOfItsFolder();
    void aRestartRestoresFolders();
    void aRestartKeepsAFolderCollapsed();
    void aTabIdThisMachineLacksIsIgnored();
    void aRunningWindowTakesFoldersFromSync();
    void eachSpaceHasItsOwnFolders();
    void aPrivateWindowHasNoFolders();
    void anAgentSpaceHasNoFoldersUntilItIsTakenOver();
    void aTabInAFolderIsNeverPutAway();
    void aFolderTabPastTheAgeHasItsPageReleased();
    void aFolderTabInAnAwaySpaceHasItsPageReleased();
    void theTabsPutAwaySparesAreSparedRelease();
    void withTheSettingOffNoPageIsReleased();
    void unloadReleasesEveryPageButTheOneOnShowAndTheSpared();
};

void FoldersTest::aFolderStandsBetweenThePinsAndTheOrdinaryTabs()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto pin = open(*controller, "pin", false);
    QVERIFY(controller->setTabPinned(pin, true));
    const auto kept = open(*controller, "kept");
    open(*controller, "loose");

    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(!folder.isEmpty());
    QCOMPARE(rows(*controller),
        QStringList({QStringLiteral("[Reading]"), QStringLiteral("  kept"), QStringLiteral("---"),
            QStringLiteral("loose")}));
    QCOMPARE(controller->tabFolder(kept), folder);
    // The tabs stand in the model as they stand in the sidebar.
    QCOMPARE(controller->tabSectionIndex(kept), 0);
}

void FoldersTest::theHairlineIsDrawnOnlyWhenBothAreThere()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto loose = open(*controller, "loose");
    QVERIFY(!rows(*controller).contains(QStringLiteral("---")));

    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(rows(*controller).contains(QStringLiteral("---")));

    QVERIFY(controller->placeTabs({loose}, folder));
    QVERIFY(!rows(*controller).contains(QStringLiteral("---")));
}

void FoldersTest::aFolderIsRefusedAnEmptyNameAndMayShareOne()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    QVERIFY(controller->createFolder({}).isEmpty());
    QVERIFY(controller->createFolder(QStringLiteral("   ")).isEmpty());
    QCOMPARE(controller->folderCount(), 0);

    const auto first = controller->createFolder(QStringLiteral("Same"));
    const auto second = controller->createFolder(QStringLiteral("Same"));
    QVERIFY(!first.isEmpty() && !second.isEmpty());
    QVERIFY(first != second);
    QCOMPARE(controller->folderCount(), 2);
}

void FoldersTest::renamingRefusesAnEmptyName()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto folder = controller->createFolder(QStringLiteral("Before"));
    QVERIFY(!controller->renameFolder(folder, QStringLiteral(" ")));
    QCOMPARE(controller->folderName(folder), QStringLiteral("Before"));
    QVERIFY(controller->renameFolder(folder, QStringLiteral("After")));
    QCOMPARE(controller->folderName(folder), QStringLiteral("After"));
}

void FoldersTest::foldersReorderWithinTheirBlock()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto a = open(*controller, "a", false);
    const auto b = open(*controller, "b");
    open(*controller, "loose");
    const auto first = controller->createFolder(QStringLiteral("First"), {a});
    const auto second = controller->createFolder(QStringLiteral("Second"), {b});

    QVERIFY(controller->moveFolder(second, 0));
    QCOMPARE(rows(*controller),
        QStringList({QStringLiteral("[Second]"), QStringLiteral("  b"), QStringLiteral("[First]"),
            QStringLiteral("  a"), QStringLiteral("---"), QStringLiteral("loose")}));
    QCOMPARE(controller->tabSectionIndex(b), 0);

    QVERIFY(controller->moveFolderBy(second, 1));
    QCOMPARE(rows(*controller).constFirst(), QStringLiteral("[First]"));
    QVERIFY(!controller->moveFolderBy(second, 1));
    QVERIFY(!controller->moveFolder(first, 2));
}

void FoldersTest::aFolderOutlivesItsTabs()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    controller->closeTab(kept);
    QCOMPARE(controller->folderName(folder), QStringLiteral("Reading"));
    QVERIFY(controller->folderTabIds(folder).isEmpty());
    QCOMPARE(memberCount(*controller, QStringLiteral("Reading")), 0);
}

void FoldersTest::aSplitsRowIsOneMember()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto left = open(*controller, "left", false);
    const auto right = open(*controller, "right");
    const auto other = open(*controller, "other");
    controller->activateTab(left);
    QVERIFY(controller->addSplit(right));

    const auto folder = controller->createFolder(QStringLiteral("Pair"), {left, other});
    QCOMPARE(controller->folderTabIds(folder), QStringList({left, right, other}));
    QCOMPARE(memberCount(*controller, QStringLiteral("Pair")), 2);
}

void FoldersTest::pinnedTabsStayInTheirSection()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto pin = open(*controller, "pin", false);
    QVERIFY(controller->setTabPinned(pin, true));
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"));
    QVERIFY(!controller->placeTabs({pin}, folder));
    QVERIFY(controller->folderTabIds(folder).isEmpty());
    QVERIFY(controller->tabPinned(pin));
    QVERIFY(controller->createFolder(QStringLiteral("Pins"), {pin}).isEmpty());
}

void FoldersTest::aNewTabJoinsNoFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    const auto fresh = open(*controller, "fresh");
    QVERIFY(controller->tabFolder(fresh).isEmpty());
    QCOMPARE(controller->folderTabIds(folder), QStringList {kept});
    QCOMPARE(rows(*controller).constLast(), QStringLiteral("fresh"));
}

void FoldersTest::aDuplicatedTabJoinsNoFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto other = open(*controller, "other");
    const auto loose = open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept, other});

    const auto copy = controller->duplicateTab(kept);
    QVERIFY(controller->tabFolder(copy).isEmpty());
    QCOMPARE(controller->folderTabIds(folder), QStringList({kept, other}));
    QCOMPARE(rows(*controller),
        QStringList(
            {QStringLiteral("[Reading]"), QStringLiteral("  kept"), QStringLiteral("  other"),
                QStringLiteral("---"), QStringLiteral("kept"), QStringLiteral("loose")}));
    QVERIFY(controller->tabFolder(loose).isEmpty());
}

void FoldersTest::aReopenedTabJoinsNoFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    controller->closeTab(kept);
    controller->reopenClosedTab();
    QVERIFY(controller->tabFolder(controller->activeTabId()).isEmpty());
    QVERIFY(controller->folderTabIds(folder).isEmpty());
}

void FoldersTest::pinningATabTakesItOutOfItsFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(controller->setTabPinned(kept, true));
    QVERIFY(controller->folderTabIds(folder).isEmpty());
    QVERIFY(controller->tabFolder(kept).isEmpty());

    // Unpinned, it is an ordinary tab, in no Folder and below the block.
    QVERIFY(controller->setTabPinned(kept, false));
    QVERIFY(controller->tabFolder(kept).isEmpty());
    QCOMPARE(rows(*controller),
        QStringList({QStringLiteral("[Reading]"), QStringLiteral("---"), QStringLiteral("kept"),
            QStringLiteral("loose")}));
}

void FoldersTest::aPoppedOutTabKeepsItsRowInTheFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(controller->popOutTab(kept));
    QCOMPARE(controller->tabFolder(kept), folder);
    QVERIFY(controller->tabPoppedOut(kept));
    QVERIFY(rows(*controller).contains(QStringLiteral("  kept")));
}

void FoldersTest::addingASplitBringsBothIntoTheRowsFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto outside = open(*controller, "outside");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    controller->activateTab(kept);
    QVERIFY(controller->addSplit(outside));
    QCOMPARE(controller->folderTabIds(folder), QStringList({kept, outside}));
    QCOMPARE(memberCount(*controller, QStringLiteral("Reading")), 1);
}

void FoldersTest::addingASplitWithAFolderTabBringsTheActiveTabIn()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto outside = open(*controller, "outside");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    controller->activateTab(outside);
    QVERIFY(controller->addSplit(kept));
    QCOMPARE(controller->tabFolder(outside), folder);
    QCOMPARE(controller->tabFolder(kept), folder);
    QCOMPARE(memberCount(*controller, QStringLiteral("Reading")), 1);
}

void FoldersTest::aBlankSplitStaysBesideItsTabInTheFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    controller->activateTab(kept);
    QVERIFY(controller->addSplit());
    const auto blank = controller->activeTabId();
    QCOMPARE(controller->folderTabIds(folder), QStringList({kept, blank}));
}

void FoldersTest::separatingLeavesBothInTheFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto outside = open(*controller, "outside");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    controller->activateTab(kept);
    QVERIFY(controller->addSplit(outside));
    QVERIFY(controller->separateSplit(kept));
    QCOMPARE(controller->folderTabIds(folder), QStringList({kept, outside}));
    QCOMPARE(memberCount(*controller, QStringLiteral("Reading")), 2);
}

void FoldersTest::closeOtherTabsNeverTouchesAFoldersTabs()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto stays = open(*controller, "stays");
    open(*controller, "goes");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    controller->closeOtherTabs(stays);
    QCOMPARE(controller->folderTabIds(folder), QStringList {kept});
    QVERIFY(!rows(*controller).contains(QStringLiteral("goes")));
    QVERIFY(rows(*controller).contains(QStringLiteral("stays")));
}

void FoldersTest::closeTabsBelowNeverTouchesAFoldersTabs()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto other = open(*controller, "other");
    open(*controller, "below");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept, other});

    // Asked on a Folder's tab, the Folder's own tabs below it stay.
    controller->closeTabsBelow(kept);
    QCOMPARE(controller->folderTabIds(folder), QStringList({kept, other}));
    QVERIFY(!rows(*controller).contains(QStringLiteral("below")));
}

void FoldersTest::aSpaceWithOnlyFolderTabsLeftComesToRest()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto loose = open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(!controller->atRest());

    controller->closeTab(loose);
    QVERIFY(controller->atRest());
    QVERIFY(controller->activeTabBlank());
    QCOMPARE(controller->folderTabIds(folder), QStringList {kept});
    // The blank tab holding the Space at rest is no row, and so no hairline.
    QCOMPARE(rows(*controller).first(2),
        QStringList({QStringLiteral("[Reading]"), QStringLiteral("  kept")}));
    QVERIFY(!rows(*controller).contains(QStringLiteral("---")));
}

void FoldersTest::closingTheLastTabOfASpaceLeavesTheFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    controller->closeTab(kept);
    QVERIFY(controller->folderTabIds(folder).isEmpty());
    QVERIFY(controller->atRest());
    QCOMPARE(controller->closedTabCount(), 1);
}

void FoldersTest::closingAFolderTabShowsItsNeighbourInTheFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto first = open(*controller, "first", false);
    const auto second = open(*controller, "second");
    const auto loose = open(*controller, "loose");
    controller->createFolder(QStringLiteral("Reading"), {first, second});
    controller->activateTab(first);
    controller->closeTab(first);
    QCOMPARE(controller->activeTabId(), second);

    // With the Folder's last tab gone the page falls to the ordinary tabs.
    controller->closeTab(second);
    QCOMPARE(controller->activeTabId(), loose);
}

void FoldersTest::droppingOnAFolderAddsAtItsEndAndLeavesItAsItWas()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto dropped = open(*controller, "dropped");
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(controller->setFolderCollapsed(folder, true));

    QVERIFY(controller->placeTabs({dropped}, folder));
    QCOMPARE(controller->folderTabIds(folder), QStringList({kept, dropped}));
    QVERIFY(controller->folderCollapsed(folder));
}

void FoldersTest::rowsMoveInAndOutOfAFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto a = open(*controller, "a", false);
    const auto b = open(*controller, "b");
    const auto c = open(*controller, "c");
    const auto d = open(*controller, "d");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {a, b});

    QVERIFY(controller->placeTabs({c}, folder, b));
    QCOMPARE(controller->folderTabIds(folder), QStringList({a, c, b}));
    QVERIFY(controller->placeTabs({a}, {}, d));
    QCOMPARE(controller->folderTabIds(folder), QStringList({c, b}));
    QCOMPARE(rows(*controller),
        QStringList({QStringLiteral("[Reading]"), QStringLiteral("  c"), QStringLiteral("  b"),
            QStringLiteral("---"), QStringLiteral("a"), QStringLiteral("d")}));
    QVERIFY(controller->placeTabs({c}, {}));
    QCOMPARE(rows(*controller).constLast(), QStringLiteral("c"));
    // Within the Folder, too, and to a Folder that is not there not at all.
    QVERIFY(!controller->placeTabs({b}, QStringLiteral("gone")));
    QCOMPARE(controller->tabFolder(b), folder);
}

void FoldersTest::aSplitsRowEntersAndLeavesWhole()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto left = open(*controller, "left", false);
    const auto right = open(*controller, "right");
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"));
    controller->activateTab(left);
    QVERIFY(controller->addSplit(right));

    QVERIFY(controller->placeTabs({right}, folder));
    QCOMPARE(controller->folderTabIds(folder), QStringList({left, right}));
    QVERIFY(controller->tabInSplit(left));
    QCOMPARE(memberCount(*controller, QStringLiteral("Reading")), 1);

    QVERIFY(controller->placeTabs({left}, {}));
    QVERIFY(controller->folderTabIds(folder).isEmpty());
    QVERIFY(controller->tabInSplit(right));
    QCOMPARE(rows(*controller).mid(rows(*controller).size() - 2),
        QStringList({QStringLiteral("left"), QStringLiteral("right")}));
}

void FoldersTest::theMoveKeysCarryARowInAndOut()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto a = open(*controller, "a", false);
    const auto b = open(*controller, "b");
    const auto c = open(*controller, "c");
    open(*controller, "d");
    const auto first = controller->createFolder(QStringLiteral("First"), {a});
    const auto second = controller->createFolder(QStringLiteral("Second"), {b});

    // Up from the first ordinary row, into the Folder above, at its end.
    QVERIFY(controller->moveTabBy(c, -1));
    QCOMPARE(controller->folderTabIds(second), QStringList({b, c}));
    // Within the Folder.
    QVERIFY(controller->moveTabBy(c, -1));
    QCOMPARE(controller->folderTabIds(second), QStringList({c, b}));
    // Across to the Folder above.
    QVERIFY(controller->moveTabBy(c, -1));
    QCOMPARE(controller->folderTabIds(first), QStringList({a, c}));
    // The top of the block is as far as a row goes.
    QVERIFY(controller->moveTabBy(c, -1));
    QVERIFY(!controller->moveTabBy(c, -1));
    QCOMPARE(controller->folderTabIds(first), QStringList({c, a}));
    // Down, across, and out below the block.
    QVERIFY(controller->moveTabBy(c, 1));
    QVERIFY(controller->moveTabBy(c, 1));
    QCOMPARE(controller->folderTabIds(second), QStringList({c, b}));
    QVERIFY(controller->moveTabBy(c, 1));
    QVERIFY(controller->moveTabBy(c, 1));
    QVERIFY(controller->tabFolder(c).isEmpty());
    QCOMPARE(rows(*controller).mid(rows(*controller).indexOf(QStringLiteral("---")) + 1),
        QStringList({QStringLiteral("c"), QStringLiteral("d")}));
}

void FoldersTest::theMoveKeysCarryASplitAcrossAndNoFurther()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto left = open(*controller, "left");
    const auto right = open(*controller, "right");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    controller->activateTab(left);
    QVERIFY(controller->addSplit(right));

    QVERIFY(controller->moveTabBy(left, -1));
    QCOMPARE(controller->folderTabIds(folder), QStringList({kept, left, right}));
    // A split's tabs stay where they are among the rows of their own list.
    QVERIFY(!controller->moveTabBy(left, -1));
    QCOMPARE(controller->folderTabIds(folder), QStringList({kept, left, right}));
}

void FoldersTest::collapsedTabsLeaveTheList()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(controller->setFolderCollapsed(folder, true));
    QCOMPARE(rows(*controller),
        QStringList({QStringLiteral("[Reading]"), QStringLiteral("---"), QStringLiteral("loose")}));
    QVERIFY(controller->setFolderCollapsed(folder, false));
    QVERIFY(rows(*controller).contains(QStringLiteral("  kept")));
}

void FoldersTest::aCollapsedFolderStaysCollapsedWhenItsTabComesOnShow()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto other = open(*controller, "other");
    const auto loose = open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept, other});
    QVERIFY(controller->setFolderCollapsed(folder, true));

    controller->activateTab(kept);
    QVERIFY(controller->folderCollapsed(folder));
    QCOMPARE(rows(*controller),
        QStringList({QStringLiteral("[Reading]"), QStringLiteral("  kept"), QStringLiteral("---"),
            QStringLiteral("loose")}));

    controller->activateTab(loose);
    QVERIFY(!rows(*controller).contains(QStringLiteral("  kept")));

    // Collapsing leaves the tab on show in view.
    QVERIFY(controller->setFolderCollapsed(folder, false));
    controller->activateTab(other);
    QVERIFY(controller->setFolderCollapsed(folder, true));
    QVERIFY(rows(*controller).contains(QStringLiteral("  other")));
    QVERIFY(!rows(*controller).contains(QStringLiteral("  kept")));
}

void FoldersTest::bothHalvesOfASplitOnShowStayInTheList()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto left = open(*controller, "left", false);
    const auto right = open(*controller, "right");
    const auto loose = open(*controller, "loose");
    controller->activateTab(left);
    QVERIFY(controller->addSplit(right));
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {left});
    QVERIFY(controller->setFolderCollapsed(folder, true));
    QVERIFY(rows(*controller).contains(QStringLiteral("  left")));
    QVERIFY(rows(*controller).contains(QStringLiteral("  right")));
    controller->activateTab(loose);
    QVERIFY(!rows(*controller).contains(QStringLiteral("  right")));
}

void FoldersTest::nextAndPreviousTabPassOverACollapsedFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto first = open(*controller, "first");
    const auto second = open(*controller, "second");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(controller->setFolderCollapsed(folder, true));

    controller->activateTab(second);
    controller->stepTab(1);
    QCOMPARE(controller->activeTabId(), first);
    controller->stepTab(-1);
    QCOMPARE(controller->activeTabId(), second);

    // A tab on show in a collapsed Folder is a stop, and the walk leaves it.
    controller->activateTab(kept);
    controller->stepTab(1);
    QCOMPARE(controller->activeTabId(), first);
}

void FoldersTest::theTabKeysCountWhatIsOnShow()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    const auto loose = open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QCOMPARE(controller->countedTabIds(), QStringList({kept, loose}));
    QVERIFY(controller->setFolderCollapsed(folder, true));
    QCOMPARE(controller->countedTabIds(), QStringList {loose});
}

void FoldersTest::removingAFolderLeavesItsTabs()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(controller->setFolderCollapsed(folder, true));

    QVERIFY(controller->removeFolder(folder));
    QCOMPARE(controller->folderCount(), 0);
    QCOMPARE(rows(*controller), QStringList({QStringLiteral("kept"), QStringLiteral("loose")}));
    QCOMPARE(controller->closedTabCount(), 0);
}

void FoldersTest::closingAFolderClosesItsTabsIntoTheStack()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto a = open(*controller, "a", false);
    const auto b = open(*controller, "b");
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {a, b});

    QVERIFY(controller->closeFolder(folder));
    QCOMPARE(controller->folderCount(), 0);
    QCOMPARE(controller->closedTabCount(), 2);
    QCOMPARE(rows(*controller), QStringList {QStringLiteral("loose")});
}

void FoldersTest::closingAFolderOnShowComesToRest()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto a = open(*controller, "a", false);
    const auto b = open(*controller, "b");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {a, b});
    QVERIFY(controller->closeFolder(folder));
    QCOMPARE(controller->folderCount(), 0);
    QVERIFY(controller->activeTabBlank());
    QVERIFY(controller->atRest());
    QCOMPARE(controller->closedTabCount(), 2);
}

void FoldersTest::movingAFolderTakesItToTheEndOfThatSpacesBlock()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto home = controller->activeSpaceId();
    const auto away = controller->createSpace(QStringLiteral("Away"));
    QVERIFY(controller->switchSpace(away));
    const auto there = open(*controller, "there", false);
    controller->createFolder(QStringLiteral("Already"), {there});
    open(*controller, "stays");
    QVERIFY(controller->switchSpace(home));
    const auto a = open(*controller, "a", false);
    const auto b = open(*controller, "b");
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Moving"), {a, b});
    QVERIFY(controller->setFolderCollapsed(folder, true));

    QVERIFY(controller->requestFolderMoveToSpace(folder, away, false));
    QCOMPARE(controller->folderCount(), 0);
    QCOMPARE(rows(*controller), QStringList {QStringLiteral("loose")});

    QVERIFY(controller->switchSpace(away));
    QCOMPARE(rows(*controller),
        QStringList({QStringLiteral("[Already]"), QStringLiteral("  there"),
            QStringLiteral("[Moving]"), QStringLiteral("---"), QStringLiteral("stays")}));
    QCOMPARE(controller->folderTabIds(folder), QStringList({a, b}));
    QVERIFY(controller->folderCollapsed(folder));
}

void FoldersTest::movingAFolderOnShowShowsAnotherTab()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto away = controller->createSpace(QStringLiteral("Away"));
    const auto a = open(*controller, "a", false);
    const auto folder = controller->createFolder(QStringLiteral("Moving"), {a});
    QVERIFY(controller->requestFolderMoveToSpace(folder, away, false));
    QVERIFY(controller->activeTabBlank());
    QVERIFY(controller->atRest());
}

void FoldersTest::movingAFolderAsksAboutThePageOnShow()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto away = controller->createSpace(QStringLiteral("Away"));
    const auto a = open(*controller, "a", false);
    const auto folder = controller->createFolder(QStringLiteral("Moving"), {a});
    QSignalSpy asked(controller.get(), &BrowserController::folderMoveConfirmationRequested);
    QVERIFY(controller->requestFolderMoveToSpace(folder, away, true));
    QCOMPARE(asked.size(), 1);
    QCOMPARE(controller->folderCount(), 1);
    QVERIFY(controller->confirmFolderMoveToSpace(folder, away));
    QCOMPARE(controller->folderCount(), 0);
}

void FoldersTest::aTabWindowStaysBehindWhenItsFolderMoves()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto away = controller->createSpace(QStringLiteral("Away"));
    const auto stays = open(*controller, "stays", false);
    const auto goes = open(*controller, "goes");
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Moving"), {stays, goes});
    QVERIFY(controller->popOutTab(stays));
    QVERIFY(controller->requestFolderMoveToSpace(folder, away, false));
    QVERIFY(controller->tabPoppedOut(stays));
    QVERIFY(controller->tabFolder(stays).isEmpty());
    QVERIFY(rows(*controller).contains(QStringLiteral("stays")));
}

void FoldersTest::movingATabToASpaceTakesItOutOfItsFolder()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto away = controller->createSpace(QStringLiteral("Away"));
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    const auto folder = controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(controller->requestTabMoveToSpace(kept, away, false));
    QVERIFY(controller->folderTabIds(folder).isEmpty());
}

void FoldersTest::aRestartRestoresFolders()
{
    QTemporaryDir root;
    QString folder;
    QString kept;
    {
        auto controller = openWindow(root);
        kept = open(*controller, "kept", false);
        const auto other = open(*controller, "other");
        open(*controller, "loose");
        folder = controller->createFolder(QStringLiteral("Reading"), {other, kept});
        controller->createFolder(QStringLiteral("Empty"));
    }
    auto restarted = openWindow(root);
    QCOMPARE(rows(*restarted),
        QStringList(
            {QStringLiteral("[Reading]"), QStringLiteral("  other"), QStringLiteral("  kept"),
                QStringLiteral("[Empty]"), QStringLiteral("---"), QStringLiteral("loose")}));
    QCOMPARE(restarted->tabFolder(kept), folder);
}

void FoldersTest::aRestartKeepsAFolderCollapsed()
{
    QTemporaryDir root;
    QString folder;
    {
        auto controller = openWindow(root);
        const auto kept = open(*controller, "kept", false);
        open(*controller, "loose");
        folder = controller->createFolder(QStringLiteral("Reading"), {kept});
        QVERIFY(controller->setFolderCollapsed(folder, true));
    }
    auto restarted = openWindow(root);
    QVERIFY(restarted->folderCollapsed(folder));
    QVERIFY(!rows(*restarted).contains(QStringLiteral("  kept")));
}

void FoldersTest::aTabIdThisMachineLacksIsIgnored()
{
    QTemporaryDir root;
    QString spaceId;
    QString kept;
    {
        auto controller = openWindow(root);
        spaceId = controller->activeSpaceId();
        kept = open(*controller, "kept", false);
        open(*controller, "loose");
    }
    {
        SqliteSessionStore store(root.path());
        QVERIFY(store.open());
        QVERIFY(store.saveFolders(spaceId,
            {FolderState {.id = QStringLiteral("synced"),
                .spaceId = spaceId,
                .name = QStringLiteral("Synced"),
                .tabIds = {QStringLiteral("elsewhere"), kept}}}));
    }
    auto restarted = openWindow(root);
    QCOMPARE(restarted->folderTabIds(QStringLiteral("synced")), QStringList {kept});
    QCOMPARE(memberCount(*restarted, QStringLiteral("Synced")), 1);
}

// Sync writes to the store behind the window, and then asks it to read again.
void FoldersTest::aRunningWindowTakesFoldersFromSync()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    const auto spaceId = controller->activeSpaceId();
    // Sync lands the window's coalesced tab writes before it reads the store.
    controller->landPendingTabs();
    QSignalSpy changed(controller.get(), &BrowserController::foldersChanged);
    SqliteSessionStore store(root.path());
    QVERIFY(store.open());
    QVERIFY(store.saveFolders(spaceId,
        {FolderState {.id = QStringLiteral("synced"),
            .spaceId = spaceId,
            .name = QStringLiteral("Synced"),
            .tabIds = {kept}}}));

    controller->reloadSyncedState();
    QVERIFY(changed.size() > 0);
    QCOMPARE(rows(*controller),
        QStringList({QStringLiteral("[Synced]"), QStringLiteral("  kept"), QStringLiteral("---"),
            QStringLiteral("loose")}));

    QVERIFY(store.saveFolders(spaceId, {}));
    controller->reloadSyncedState();
    QCOMPARE(controller->folderCount(), 0);
    QVERIFY(controller->tabFolder(kept).isEmpty());
}

void FoldersTest::eachSpaceHasItsOwnFolders()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto home = controller->activeSpaceId();
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    controller->createFolder(QStringLiteral("Reading"), {kept});
    const auto away = controller->createSpace(QStringLiteral("Away"));
    QVERIFY(controller->switchSpace(away));
    QCOMPARE(controller->folderCount(), 0);
    QVERIFY(controller->switchSpace(home));
    QCOMPARE(controller->folderCount(), 1);
    QVERIFY(rows(*controller).contains(QStringLiteral("  kept")));
}

void FoldersTest::aPrivateWindowHasNoFolders()
{
    PrivateSessionFixture privateSession;
    auto controller = privateSession.createController();
    QVERIFY(!controller->foldersAvailable());
    QVERIFY(controller->createFolder(QStringLiteral("Reading")).isEmpty());
    QCOMPARE(controller->folderCount(), 0);
}

void FoldersTest::anAgentSpaceHasNoFoldersUntilItIsTakenOver()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    QVERIFY(controller->foldersAvailable());
    const auto agentSpace
        = controller->createAgentSpace(QStringLiteral("Research"), QStringLiteral("agent"));
    QVERIFY(!agentSpace.isEmpty());
    QVERIFY(controller->switchSpace(agentSpace));
    QVERIFY(!controller->foldersAvailable());
    QVERIFY(controller->createFolder(QStringLiteral("Reading")).isEmpty());

    QVERIFY(controller->takeOverSpace(agentSpace));
    QVERIFY(controller->foldersAvailable());
    QVERIFY(!controller->createFolder(QStringLiteral("Reading")).isEmpty());
}

void FoldersTest::aTabInAFolderIsNeverPutAway()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "loose");
    open(*controller, "shown");
    controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(controller->setPutAwayAfterSeconds(3600));
    controller->setNowForTests(start + 2 * hour);
    controller->putAwayUnusedTabs();
    QVERIFY(rows(*controller).contains(QStringLiteral("  kept")));
    QVERIFY(!rows(*controller).contains(QStringLiteral("loose")));
    QCOMPARE(controller->putAwayTabs().size(), 1);
}

void FoldersTest::aFolderTabPastTheAgeHasItsPageReleased()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto old = open(*controller, "old", false);
    const auto recent = open(*controller, "recent");
    const auto shown = open(*controller, "shown");
    controller->createFolder(QStringLiteral("Reading"), {old, recent, shown});
    QVERIFY(controller->setPutAwayAfterSeconds(3600));
    // Each tab leaves show when the next comes on, so the age counts from here.
    controller->setNowForTests(start + 90 * 60 * 1000);
    controller->activateTab(recent);
    controller->activateTab(shown);
    controller->setNowForTests(start + 2 * hour);
    QSignalSpy released(controller.get(), &BrowserController::tabPagesReleased);
    controller->putAwayUnusedTabs();
    // The old tab's row stays. The recent tab is young, and the tab on show in
    // use whatever its age.
    QCOMPARE(released.size(), 1);
    QCOMPARE(released.constFirst().constFirst().toStringList(), QStringList {old});
    QVERIFY(rows(*controller).contains(QStringLiteral("  old")));
    QCOMPARE(controller->putAwayTabs().size(), 0);
}

void FoldersTest::aFolderTabInAnAwaySpaceHasItsPageReleased()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "shown");
    controller->createFolder(QStringLiteral("Reading"), {kept});
    const auto away = controller->createSpace(QStringLiteral("Away"));
    QVERIFY(controller->setPutAwayAfterSeconds(3600));
    QVERIFY(controller->switchSpace(away));
    controller->setNowForTests(start + 2 * hour);
    QSignalSpy released(controller.get(), &BrowserController::tabPagesReleased);
    controller->putAwayUnusedTabs();
    QCOMPARE(released.size(), 1);
    QCOMPARE(released.constFirst().constFirst().toStringList(), QStringList {kept});
}

void FoldersTest::theTabsPutAwaySparesAreSparedRelease()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto windowed = open(*controller, "windowed", false);
    const auto left = open(*controller, "left");
    const auto right = open(*controller, "right");
    const auto old = open(*controller, "old");
    open(*controller, "shown");
    controller->activateTab(left);
    QVERIFY(controller->addSplit(right));
    controller->createFolder(QStringLiteral("Reading"), {windowed, left, old});
    QVERIFY(controller->popOutTab(windowed));
    controller->activateTab(
        controller->tabs()->data(controller->tabs()->index(4, 0), TabListModel::IdRole).toString());
    QVERIFY(controller->setPutAwayAfterSeconds(3600));
    controller->setNowForTests(start + 2 * hour);
    QSignalSpy released(controller.get(), &BrowserController::tabPagesReleased);
    controller->putAwayUnusedTabs();
    QCOMPARE(released.size(), 1);
    QCOMPARE(released.constFirst().constFirst().toStringList(), QStringList {old});
}

void FoldersTest::withTheSettingOffNoPageIsReleased()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto kept = open(*controller, "kept", false);
    open(*controller, "shown");
    controller->createFolder(QStringLiteral("Reading"), {kept});
    QVERIFY(controller->setPutAwayAfterSeconds(0));
    controller->setNowForTests(start + 1000 * hour);
    QSignalSpy released(controller.get(), &BrowserController::tabPagesReleased);
    controller->putAwayUnusedTabs();
    QCOMPARE(released.size(), 0);
}

void FoldersTest::unloadReleasesEveryPageButTheOneOnShowAndTheSpared()
{
    QTemporaryDir root;
    auto controller = openWindow(root);
    const auto shown = open(*controller, "shown", false);
    const auto windowed = open(*controller, "windowed");
    const auto idle = open(*controller, "idle");
    open(*controller, "loose");
    const auto folder
        = controller->createFolder(QStringLiteral("Reading"), {shown, windowed, idle});
    QVERIFY(controller->popOutTab(windowed));
    controller->activateTab(shown);
    // Asked for by name, so the age and the setting have nothing to say.
    QVERIFY(controller->setPutAwayAfterSeconds(0));

    QSignalSpy released(controller.get(), &BrowserController::tabPagesReleased);
    controller->unloadFolder(folder);
    QCOMPARE(released.size(), 1);
    QCOMPARE(released.constFirst().constFirst().toStringList(), QStringList {idle});
}

QTEST_MAIN(FoldersTest)
#include "tst_folders.moc"
