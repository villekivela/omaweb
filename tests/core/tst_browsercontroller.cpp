#include "BrowserController.h"
#include "HistoryQuery.h"
#include "PrivateSessionFixture.h"
#include "SessionFixture.h"
#include "SpaceListModel.h"
#include "SpaceStorage.h"
#include "SqliteSessionStore.h"
#include "TabListModel.h"
#include "WindowManager.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QTest>
#include <QUrlQuery>

#include <memory>
#include <type_traits>

using omaweb::BrowserController;
using omaweb::SpaceListModel;
using omaweb::SpaceStorage;
using omaweb::TabListModel;
using omaweb::WindowManager;
using omaweb::test::PrivateSessionFixture;
using omaweb::test::SessionFixture;
using omaweb::test::SessionSpec;
using omaweb::test::SpaceSpec;
using omaweb::test::TabSpec;

namespace {

using SessionPermissionDecisions = QSharedPointer<QHash<QString, int>>;

static_assert(std::is_constructible_v<BrowserController, SpaceStorage>);
static_assert(std::is_constructible_v<BrowserController, SpaceStorage, QString>);
static_assert(!std::is_constructible_v<BrowserController, SpaceStorage, QObject *>);
static_assert(!std::is_constructible_v<BrowserController, SpaceStorage, bool>);
static_assert(
    !std::is_constructible_v<BrowserController, SpaceStorage, bool, SessionPermissionDecisions>);
static_assert(!std::is_constructible_v<BrowserController, SpaceStorage, bool,
    SessionPermissionDecisions, QString>);
static_assert(!std::is_constructible_v<BrowserController, SpaceStorage, bool,
    SessionPermissionDecisions, QSharedPointer<omaweb::SessionSiteState>, QString>);
static_assert(std::is_constructible_v<BrowserController, std::shared_ptr<omaweb::SessionStore>,
    bool, SessionPermissionDecisions, QSharedPointer<omaweb::SessionSiteState>, QString>);

// Everything under a directory, so a read can be shown to have left the data
// root exactly as it found it.
QStringList entriesUnder(const QString &path)
{
    QStringList entries;
    QDirIterator walk(
        path, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (walk.hasNext()) {
        entries.append(walk.next());
    }
    entries.sort();
    return entries;
}

} // namespace

class BrowserControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void createsPersonalSpaceAndBlankTab();
    void createsAndSwitchesSpaces();
    void switchesSpacesWithoutReportingAStructuralChange();
    void renamesSpacePersistently();
    void reordersSpacesPersistently();
    void requiresNameToDeletePopulatedSpace();
    void treatsEngineStateAsPopulatedSpaceData();
    void suspendsInactiveSpaceAndRestoresItsTabs();
    void warnsBeforeMovingEditedTabBetweenSpaces();
    void restoresEverySpaceAfterRestart();
    void migratesLegacyGlobalTabsWithoutLockingSchema();
    void separatesEngineStorageBySpaceAndEngine();
    void saysWhereAnEngineProfileBelongsWithoutMakingOne();
    void makesAnEngineProfileOnlyWhenAskedToPrepareOne();
    void createsTabOnlyAfterCommittedInput();
    void opensKeyboardHintTargetsInBackground();
    void closesRequestedBackgroundTab();
    void persistsTabsAndPins();
    void landsThePendingTabWriteAtQuit();
    void pinningMovesTabIntoPinnedBlock();
    void keepsFinalTabAsBlankTab();
    void restsUntilSomethingIsOpenedInTheSpace();
    void appliesAddresslessPageReportsWithoutBlankingTheTab();
    void recordsVisitsWhenReportedLoadsFinish();
    void keepsReportedIconAndAudibilityAcrossSpaceSwitches();
    void dropsReportedPageStateWhenMovingATabBetweenSpaces();
    void exposesOnlyCombinedPageStateReportsToQml();
    void dropsThePreviousHostsIconFromPageReports();
    void keepsRendererFailureOnAffectedTab();
    void keepsMutingDecisionWhileSoundComesAndGoes();
    void stepsZoomAlongOneLadderPerTab();
    void restoresEveryTabsZoomAfterRestart();
    void sharesPrivateIdentityUntilLastWindowCloses();
    void keepsHistorySuggestionsInsideActiveSpace();
    void keepsHistoryInsideItsBoundWhileASpaceStaysOpen();
    void answersOnlyTheLatestOmnibarSearch();
    void abandonsASearchTheReaderHasMovedOnFrom();
    void filtersAndDeletesHistoryAtRequestedBoundaries();
    void resolvesAddressesBeforeSearches();
    void migratesAndUsesSearchEngineConfiguration();
    void addsPredefinedSearchEngineProviders();
    void namesTheEngineATypedKeywordSelects();
    void offersTheKeywordsATypedPrefixCouldBecome();
    void allowsEveryWindowCapabilityInAMainWindow();
    void refusesEveryWindowCapabilityInAPrivateWindow();
    void clearsSelectedBrowsingDataWithinConfirmedScope();
    void scopesPermissionDecisionsToOriginSpaceAndLifetime();
    void remembersOnlyThePermissionsThatMayBeRemembered();
    void listsAndResetsOneSitesPermissionsWithinItsSpace();
    void refusesEveryCertificateExceptionButAnOverridableLocalMainFrame();
    void keepsCertificateExceptionsOutOfEveryStoreAndSession();
    void keepsAGrantedCertificateExceptionVisibleForItsSession();
    void blocksThirdPartyCookiesUntilAFlowIsGivenAVisibleAllowance();
    void endsThirdPartyCookieAllowancesWithTheirSpaceAndPrivateSession();
    void measuresTheSiteDataHeldForOneSpace();
    void scopesExternalProtocolDecisionsToOriginSchemeSpaceAndPrivateSession();
    void configuresOneDownloadDirectoryForEveryWindow();
    void persistsInterfacePreferencesOutsidePrivateBrowsing();
    void attachesOneInspectorToOneTab();
    void keepsTheInspectorThroughASpaceSwitch();
    void detachesTheInspectorWithTheTabItInspects();
    void reordersTabsWithinTheirSection();
    void duplicatesOnlyTheAddress();
    void sweepingClosesSpareEveryPinnedTab();
    void keepsRecentClosesPerSpaceAcrossRestart();
    void reopensClosedTabsNewestFirstAndBoundedAtTwentyFive();
    void keepsPrivateClosesInMemoryOnly();
    void remembersTheSameSpacesLastPageClosedTwice();
    void restoresMutingWithTheTabAndNeverByOrigin();
    void allowsKeepActiveOnlyOnPinnedTabs();
    void namesEverySuspensionExceptionAndNothingElse();
    void restoresRetainedTabsOfUnvisitedSpacesAfterRestart();
    void stopsRetainingTheSpaceThatReplacesADeletedOne();
    void restoresTheSessionAfterAnUncleanExit();
    void routesNotificationsToTheOriginatingTab();
    void remembersOriginInteractionWithinOneSpaceAndSession();
    void holdsBackSoundUntilTheOriginIsDealtWith();
    void neverRestoresTheInspectorAfterRestart();
    void pairsTwoOrdinaryTabsOfTheSpaceAsOneSplit();
    void pairsABlankTabBesideTheActiveOneWhenNoneIsNamed();
    void refusesASplitOfPinnedPairedOrUnknownTabs();
    void movesFocusBetweenTheHalvesOfASplit();
    void keepsTheSplitRowWhileAnotherTabIsOnShowAndStepsOverItAsOneStop();
    void separatesASplitIntoTwoAdjacentOrdinaryRows();
    void endsASplitWhenEitherTabClosesOrLeavesTheSpace();
    void keepsASplitTabFromBeingPinnedOrMoved();
    void keepsASplitAcrossARestartAndASpaceSwitch();
    void repairsAPairingTheStoreHandsBackBroken();
    void splitsInAPrivateWindowAndWritesNothing();
};

void BrowserControllerTest::createsPersonalSpaceAndBlankTab()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));

    QVERIFY(controller.ready());
    QCOMPARE(controller.activeSpaceName(), QStringLiteral("Personal"));
    QCOMPARE(controller.spaces()->rowCount(), 1);
    QCOMPARE(controller.tabs()->rowCount(), 1);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("about:blank")));
}

void BrowserControllerTest::createsAndSwitchesSpaces()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://personal.example"), false);
    const auto personalSpaceId = controller.activeSpaceId();

    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(!workSpaceId.isEmpty());
    QCOMPARE(controller.spaces()->rowCount(), 2);
    QCOMPARE(controller.activeSpaceId(), personalSpaceId);

    QVERIFY(controller.switchSpace(workSpaceId));
    QCOMPARE(controller.activeSpaceName(), QStringLiteral("Work"));
    QCOMPARE(controller.tabs()->rowCount(), 1);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("about:blank")));

    QVERIFY(controller.switchSpace(personalSpaceId));
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://personal.example")));
}

void BrowserControllerTest::switchesSpacesWithoutReportingAStructuralChange()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QSignalSpy resetSpy(controller.spaces(), &QAbstractItemModel::modelReset);
    QSignalSpy changedSpy(controller.spaces(), &QAbstractItemModel::dataChanged);

    QVERIFY(controller.switchSpace(workSpaceId));

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 2);
    for (const auto &change : changedSpy) {
        QCOMPARE(change.at(2).value<QList<int>>(), {SpaceListModel::ActiveRole});
    }
}

void BrowserControllerTest::opensKeyboardHintTargetsInBackground()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto activeTabId = controller.activeTabId();

    controller.openInputInBackground(QUrl(QStringLiteral("https://example.com/hint")));

    QCOMPARE(controller.tabs()->rowCount(), 2);
    QCOMPARE(controller.activeTabId(), activeTabId);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("about:blank")));
}

void BrowserControllerTest::closesRequestedBackgroundTab()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto activeTabId = controller.activeTabId();
    controller.openInputInBackground(QUrl(QStringLiteral("https://example.com/background")));
    const auto backgroundIndex = controller.tabs()->index(1, 0);
    const auto backgroundTabId
        = controller.tabs()->data(backgroundIndex, TabListModel::IdRole).toString();

    controller.closeTab(backgroundTabId);

    QCOMPARE(controller.tabs()->rowCount(), 1);
    QCOMPARE(controller.activeTabId(), activeTabId);
    controller.reopenClosedTab();
    QCOMPARE(controller.tabs()->rowCount(), 2);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://example.com/background")));
}

void BrowserControllerTest::renamesSpacePersistently()
{
    QTemporaryDir root;
    QString workSpaceId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.renameSpace(workSpaceId, QStringLiteral("Research")));
        QVERIFY(controller.switchSpace(workSpaceId));
        QCOMPARE(controller.activeSpaceName(), QStringLiteral("Research"));
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(restored.activeSpaceId(), workSpaceId);
    QCOMPARE(restored.activeSpaceName(), QStringLiteral("Research"));
}

void BrowserControllerTest::reordersSpacesPersistently()
{
    const auto spaceOrder = [](BrowserController &controller) {
        auto *model = controller.spaces();
        QStringList names;
        for (int row = 0; row < model->rowCount(); ++row) {
            names.append(model->data(model->index(row, 0), SpaceListModel::NameRole).toString());
        }
        return names;
    };
    const auto tabsOf = [](BrowserController &controller, const QString &spaceId) {
        QStringList ids;
        for (const auto &tab : controller.sessionStore()->loadTabs(spaceId)) {
            ids.append(tab.id);
        }
        return ids;
    };

    SessionFixture fixture(SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = QStringLiteral("personal"),
                .name = QStringLiteral("Personal"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("personal-tab"),
                    .url = QUrl(QStringLiteral("https://personal.example/session")),
                }},
            },
            SpaceSpec {
                .id = QStringLiteral("work"),
                .name = QStringLiteral("Work"),
                .tabs = {TabSpec {
                             .id = QStringLiteral("work-pin"),
                             .url = QUrl(QStringLiteral("https://work.example/pinned")),
                             .pinned = true,
                         },
                    TabSpec {
                        .id = QStringLiteral("work-tab"),
                        .url = QUrl(QStringLiteral("https://work.example/session")),
                    }},
                .activeTabId = QStringLiteral("work-tab"),
            },
            SpaceSpec {
                .id = QStringLiteral("reading"),
                .name = QStringLiteral("Reading"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("reading-tab"),
                    .url = QUrl(QStringLiteral("https://reading.example/session")),
                }},
            },
        },
        .activeSpaceId = QStringLiteral("personal"),
    });
    QVERIFY_SESSION_READY(fixture);

    const QStringList createdOrder {
        QStringLiteral("Personal"), QStringLiteral("Work"), QStringLiteral("Reading")};
    const QStringList movedOrder {
        QStringLiteral("Personal"), QStringLiteral("Reading"), QStringLiteral("Work")};

    {
        const auto controller = fixture.createController();
        QCOMPARE(spaceOrder(*controller), createdOrder);
        const auto activeTabId = controller->activeTabId();

        // A move reports itself as a move, so a list watching the model keeps
        // the rows it already has.
        QSignalSpy resetSpy(controller->spaces(), &QAbstractItemModel::modelReset);
        QSignalSpy movedSpy(controller->spaces(), &QAbstractItemModel::rowsMoved);

        QVERIFY(controller->moveSpaceBy(QStringLiteral("reading"), -1));
        QCOMPARE(spaceOrder(*controller), movedOrder);
        QCOMPARE(movedSpy.count(), 1);
        QCOMPARE(resetSpy.count(), 0);

        // Either end has nowhere to go that way, a Space the list does not
        // hold has no place in it at all, and none of these changes anything.
        QVERIFY(!controller->moveSpaceBy(QStringLiteral("personal"), -1));
        QVERIFY(!controller->moveSpaceBy(QStringLiteral("work"), 1));
        QVERIFY(!controller->moveSpaceBy(QStringLiteral("no-such-space"), -1));
        QVERIFY(!controller->moveSpaceBy(QStringLiteral("reading"), 0));
        QCOMPARE(spaceOrder(*controller), movedOrder);
        QCOMPARE(movedSpy.count(), 1);

        // Only positions change. The Space on show, the tab on show in it, and
        // what every Space holds are where the reader left them, the Space
        // that moved and the one it moved past included.
        QCOMPARE(controller->activeSpaceId(), QStringLiteral("personal"));
        QCOMPARE(controller->activeTabId(), activeTabId);
        QCOMPARE(tabsOf(*controller, QStringLiteral("personal")),
            QStringList {QStringLiteral("personal-tab")});
        QCOMPARE(tabsOf(*controller, QStringLiteral("work")),
            QStringList({QStringLiteral("work-pin"), QStringLiteral("work-tab")}));
        QCOMPARE(tabsOf(*controller, QStringLiteral("reading")),
            QStringList {QStringLiteral("reading-tab")});
    }

    const auto restored = fixture.createController();
    QCOMPARE(spaceOrder(*restored), movedOrder);
    QCOMPARE(restored->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(tabsOf(*restored, QStringLiteral("work")),
        QStringList({QStringLiteral("work-pin"), QStringLiteral("work-tab")}));
}

void BrowserControllerTest::requiresNameToDeletePopulatedSpace()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalSpaceId = controller.activeSpaceId();
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    controller.openInput(QStringLiteral("https://work.example"), false);
    const auto workProfilePath = controller.prepareProfileForSpace(workSpaceId);
    QCOMPARE(workProfilePath, controller.activeProfilePath());
    QVERIFY(QFileInfo::exists(workProfilePath));
    QVERIFY(controller.switchSpace(personalSpaceId));

    QVERIFY(!controller.deleteSpace(workSpaceId, QString {}));
    QVERIFY(!controller.deleteSpace(workSpaceId, QStringLiteral("work")));
    QVERIFY(controller.deleteSpace(workSpaceId, QStringLiteral("Work")));
    QCOMPARE(controller.spaces()->rowCount(), 1);
    QVERIFY(!QFileInfo::exists(workProfilePath));

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(restored.spaces()->rowCount(), 1);
    QCOMPARE(restored.activeSpaceId(), personalSpaceId);
}

void BrowserControllerTest::treatsEngineStateAsPopulatedSpaceData()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("qt")));
    const auto personalSpaceId = controller.activeSpaceId();
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    QFile cookieState(controller.prepareProfileForSpace(workSpaceId) + QStringLiteral("/Cookies"));
    QVERIFY(cookieState.open(QIODevice::WriteOnly));
    QVERIFY(cookieState.write("engine-state") > 0);
    cookieState.close();
    QVERIFY(controller.switchSpace(personalSpaceId));

    QVERIFY(!controller.deleteSpace(workSpaceId, QString {}));
    QVERIFY(controller.deleteSpace(workSpaceId, QStringLiteral("Work")));
}

void BrowserControllerTest::suspendsInactiveSpaceAndRestoresItsTabs()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://one.example"), false);
    controller.openInput(QStringLiteral("https://two.example"), true);
    const auto personalSpaceId = controller.activeSpaceId();
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QSignalSpy suspendedSpy(&controller, &BrowserController::spaceSuspended);
    QSignalSpy restoredSpy(&controller, &BrowserController::spaceRestored);

    QVERIFY(controller.switchSpace(workSpaceId));
    QCOMPARE(suspendedSpy.takeFirst().at(0).toString(), personalSpaceId);
    QCOMPARE(restoredSpy.takeFirst().at(0).toString(), workSpaceId);
    QCOMPARE(controller.tabs()->rowCount(), 1);

    QVERIFY(controller.switchSpace(personalSpaceId));
    QCOMPARE(suspendedSpy.takeFirst().at(0).toString(), workSpaceId);
    QCOMPARE(restoredSpy.takeFirst().at(0).toString(), personalSpaceId);
    QCOMPARE(controller.tabs()->rowCount(), 2);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://two.example")));
}

void BrowserControllerTest::warnsBeforeMovingEditedTabBetweenSpaces()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://draft.example/form"), false);
    controller.toggleActivePinned();
    const auto movedTabId = controller.activeTabId();
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QSignalSpy confirmationSpy(&controller, &BrowserController::tabMoveConfirmationRequested);
    QSignalSpy removedSpy(controller.tabs(), &QAbstractItemModel::rowsRemoved);

    QVERIFY(controller.requestTabMoveToSpace(movedTabId, workSpaceId, true));
    QCOMPARE(confirmationSpy.count(), 1);
    QCOMPARE(controller.activeTabId(), movedTabId);

    QVERIFY(controller.confirmTabMoveToSpace(movedTabId, workSpaceId));
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(controller.tabs()->rowCount(), 1);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("about:blank")));

    QVERIFY(controller.switchSpace(workSpaceId));
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://draft.example/form")));
    QVERIFY(controller.activeTabPinned());
}

void BrowserControllerTest::restoresEverySpaceAfterRestart()
{
    SessionFixture fixture(SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = QStringLiteral("personal"),
                .name = QStringLiteral("Personal"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("personal-tab"),
                    .url = QUrl(QStringLiteral("https://personal.example/session")),
                }},
            },
            SpaceSpec {
                .id = QStringLiteral("work"),
                .name = QStringLiteral("Work"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("work-tab"),
                    .url = QUrl(QStringLiteral("https://work.example/session")),
                }},
            },
        },
        .activeSpaceId = QStringLiteral("work"),
    });
    QVERIFY_SESSION_READY(fixture);

    const auto restored = fixture.createController();
    QCOMPARE(restored->activeSpaceId(), QStringLiteral("work"));
    QCOMPARE(restored->activeUrl(), QUrl(QStringLiteral("https://work.example/session")));
    QVERIFY(restored->switchSpace(QStringLiteral("personal")));
    QCOMPARE(restored->activeUrl(), QUrl(QStringLiteral("https://personal.example/session")));
}

void BrowserControllerTest::separatesEngineStorageBySpaceAndEngine()
{
    QTemporaryDir root;
    BrowserController qtController(SpaceStorage(root.path(), QStringLiteral("qt")));
    const auto personalQtPath = qtController.activeProfilePath();
    const auto workSpaceId = qtController.createSpace(QStringLiteral("Work"));
    QVERIFY(qtController.switchSpace(workSpaceId));
    const auto workQtPath = qtController.activeProfilePath();

    QVERIFY(personalQtPath != workQtPath);
    QVERIFY(personalQtPath.endsWith(QStringLiteral("/engines/qt")));
    QVERIFY(workQtPath.endsWith(QStringLiteral("/engines/qt")));

    BrowserController ladybirdController(SpaceStorage(root.path(), QStringLiteral("ladybird")));
    QCOMPARE(ladybirdController.activeSpaceId(), workSpaceId);
    QVERIFY(ladybirdController.activeProfilePath() != workQtPath);
    QVERIFY(ladybirdController.activeProfilePath().endsWith(QStringLiteral("/engines/ladybird")));
}

// Asking where an Engine profile belongs is not asking for it to exist. A
// reader that created directories left every "is it there" test unable to
// fail, including the one Site information measures site data with.
void BrowserControllerTest::saysWhereAnEngineProfileBelongsWithoutMakingOne()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("qt")));
    const auto spaceId = controller.activeSpaceId();
    const auto before = entriesUnder(root.path());

    const auto activePath = controller.activeProfilePath();
    QVERIFY(!activePath.isEmpty());
    QCOMPARE(controller.profilePathForSpace(spaceId), activePath);
    QCOMPARE(controller.siteDataBytes(spaceId, {QStringLiteral("Cookies")}), 0);

    QVERIFY(!QFileInfo::exists(activePath));
    QCOMPARE(entriesUnder(root.path()), before);

    // A Space this window does not have is not a Space to answer for.
    QVERIFY(controller.profilePathForSpace(QStringLiteral("absent")).isEmpty());
}

// The one call that does create it, made where an engine host is about to be
// pointed at the directory.
void BrowserControllerTest::makesAnEngineProfileOnlyWhenAskedToPrepareOne()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("qt")));
    const auto spaceId = controller.activeSpaceId();

    const auto prepared = controller.prepareProfileForSpace(spaceId);
    QCOMPARE(prepared, controller.profilePathForSpace(spaceId));
    QVERIFY(QFileInfo(prepared).isDir());
    // Preparing one that is already there is the same answer, not a failure.
    QCOMPARE(controller.prepareProfileForSpace(spaceId), prepared);
    QVERIFY(controller.prepareProfileForSpace(QStringLiteral("absent")).isEmpty());

    // A Space that cannot be given a directory is told so, rather than handed
    // a path an engine would fail over later.
    const auto blockedSpaceId = controller.createSpace(QStringLiteral("Blocked"));
    const auto blockedPath = controller.profilePathForSpace(blockedSpaceId);
    QVERIFY(QDir().mkpath(QFileInfo(QFileInfo(blockedPath).path()).path()));
    QFile blocking(QFileInfo(blockedPath).path());
    QVERIFY(blocking.open(QIODevice::WriteOnly));
    blocking.close();
    QVERIFY(controller.prepareProfileForSpace(blockedSpaceId).isEmpty());
}

void BrowserControllerTest::migratesLegacyGlobalTabsWithoutLockingSchema()
{
    QTemporaryDir root;
    const auto connectionName = QStringLiteral("legacy-session-fixture");
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(root.filePath(QStringLiteral("state.sqlite")));
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(
            QStringLiteral("CREATE TABLE spaces (id TEXT PRIMARY KEY, name TEXT NOT NULL, "
                           "color TEXT NOT NULL, active INTEGER NOT NULL DEFAULT 0, "
                           "position INTEGER NOT NULL DEFAULT 0)")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE tabs (id TEXT PRIMARY KEY, space_id TEXT NOT NULL, "
            "url TEXT NOT NULL, title TEXT NOT NULL, pinned INTEGER NOT NULL DEFAULT 0, "
            "active INTEGER NOT NULL DEFAULT 0, position INTEGER NOT NULL DEFAULT 0)")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO spaces(id, name, color, active, position) "
                                          "VALUES('personal', 'Personal', '#7c6cff', 1, 0)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO tabs(id, space_id, url, title, pinned, active, position) "
            "VALUES('legacy-tab', 'personal', 'https://example.com', 'Example', 0, 1, 0)")));
        query.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));

    QVERIFY2(controller.ready(), qPrintable(controller.errorMessage()));
    QCOMPARE(controller.tabs()->rowCount(), 1);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://example.com")));
}

void BrowserControllerTest::createsTabOnlyAfterCommittedInput()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));

    QCOMPARE(controller.tabs()->rowCount(), 1);
    controller.openInput(QStringLiteral("omaweb browser"), true);
    QCOMPARE(controller.tabs()->rowCount(), 2);
    QCOMPARE(controller.activeUrl().host(), QStringLiteral("duckduckgo.com"));
}

void BrowserControllerTest::persistsTabsAndPins()
{
    QTemporaryDir root;
    QString activeId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        controller.openInput(QStringLiteral("https://example.com"), false);
        controller.toggleActivePinned();
        activeId = controller.activeTabId();
        QVERIFY(controller.activeTabPinned());
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(restored.activeTabId(), activeId);
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://example.com")));
    QVERIFY(restored.activeTabPinned());
}

// The address a page reported is written on a delay, so a quit inside that
// delay has a write still pending: it is queued to the store as the window
// goes and lands before the store closes.
void BrowserControllerTest::landsThePendingTabWriteAtQuit()
{
    QTemporaryDir root;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        controller.openInput(QStringLiteral("https://example.com/pending"), false);
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://example.com/pending")));
}

void BrowserControllerTest::pinningMovesTabIntoPinnedBlock()
{
    QTemporaryDir root;
    QString newlyPinnedId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        controller.openInput(QStringLiteral("https://first.example"), false);
        controller.toggleActivePinned();
        for (int index = 2; index <= 5; ++index) {
            controller.openInput(QStringLiteral("https://tab-%1.example").arg(index), true);
        }
        newlyPinnedId = controller.activeTabId();

        QCOMPARE(controller.tabs()
                     ->data(controller.tabs()->index(4, 0), TabListModel::IdRole)
                     .toString(),
            newlyPinnedId);
        controller.toggleActivePinned();

        QCOMPARE(controller.tabs()
                     ->data(controller.tabs()->index(1, 0), TabListModel::IdRole)
                     .toString(),
            newlyPinnedId);
        QCOMPARE(controller.pinnedTabs()->rowCount(), 2);
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(restored.tabs()->data(restored.tabs()->index(1, 0), TabListModel::IdRole).toString(),
        newlyPinnedId);
}

void BrowserControllerTest::keepsFinalTabAsBlankTab()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://example.com"), false);
    controller.closeActiveTab();
    QCOMPARE(controller.tabs()->rowCount(), 1);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("about:blank")));
}

// A Space at rest is the state the interface has no page to show for and no
// ordinary tab to list: nothing has been opened in it, or the last page in it
// has been closed.
void BrowserControllerTest::restsUntilSomethingIsOpenedInTheSpace()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    QSignalSpy restChanged(&controller, &BrowserController::atRestChanged);

    QVERIFY(controller.atRest());

    controller.openInput(QStringLiteral("https://example.com"), false);
    QVERIFY(!controller.atRest());
    QCOMPARE(restChanged.count(), 1);

    // Closing the last page empties the Space rather than the window.
    controller.closeActiveTab();
    QCOMPARE(controller.tabs()->rowCount(), 1);
    QVERIFY(controller.atRest());
    QCOMPARE(restChanged.count(), 2);

    // A pinned tab is the Space's own furniture. It is there whether anything
    // has been opened or not, so it does not decide whether the Space rests.
    controller.openInput(QStringLiteral("https://pinned.example"), false);
    controller.toggleActivePinned();
    QCOMPARE(controller.pinnedTabs()->rowCount(), 1);
    QVERIFY(!controller.atRest());
    controller.openInput(QStringLiteral("about:blank"), true);
    QCOMPARE(controller.unpinnedTabs()->rowCount(), 1);
    QVERIFY(controller.atRest());

    // A blank tab beside an open page is a tab in its own right — a window a
    // page asked for, say — and has to stay listed and closable.
    controller.openInput(QStringLiteral("https://second.example"), true);
    QCOMPARE(controller.unpinnedTabs()->rowCount(), 2);
    QVERIFY(!controller.atRest());
}

void BrowserControllerTest::keepsRendererFailureOnAffectedTab()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto failedTabId = controller.activeTabId();

    controller.reportTabRendererFailure(
        failedTabId, QStringLiteral("Renderer exited unexpectedly"));
    QVERIFY(controller.activeRendererFailed());
    QCOMPARE(
        controller.activeRendererFailureReason(), QStringLiteral("Renderer exited unexpectedly"));

    controller.openInput(QStringLiteral("https://example.com"), true);
    QVERIFY(!controller.activeRendererFailed());

    controller.activateTab(failedTabId);
    QVERIFY(controller.activeRendererFailed());

    QSignalSpy reloadSpy(&controller, &BrowserController::rendererRecoveryReloadRequested);
    controller.recoverActiveTab();
    QVERIFY(!controller.activeRendererFailed());
    QCOMPARE(reloadSpy.count(), 1);
}

void BrowserControllerTest::appliesAddresslessPageReportsWithoutBlankingTheTab()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://example.com/first"), false);
    const auto tabId = controller.activeTabId();

    controller.reportTabPageState(tabId, {}, QStringLiteral("Missing"), {}, true, false);

    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://example.com/first")));
    QCOMPARE(controller.activeTitle(), QStringLiteral("example.com"));
    QVERIFY(controller.tabs()
            ->data(controller.tabs()->index(0, 0), TabListModel::LoadingRole)
            .toBool());
}

void BrowserControllerTest::recordsVisitsWhenReportedLoadsFinish()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto tabId = controller.activeTabId();
    const QUrl address(QStringLiteral("https://example.com/visited"));

    controller.reportTabPageState(tabId, address, QStringLiteral("Visited"), {}, true, false);
    QCOMPARE(controller.history({}).size(), 0);

    controller.reportTabPageState(tabId, address, QStringLiteral("Visited"), {}, false, false);
    const auto visits = controller.history({});
    QCOMPARE(visits.size(), 1);
    QCOMPARE(visits.first().toMap().value(QStringLiteral("url")).toUrl(), address);
    QCOMPARE(visits.first().toMap().value(QStringLiteral("title")).toString(),
        QStringLiteral("Visited"));
    controller.reportTabPageState(tabId, address, QStringLiteral("Visited"), {}, false, false);
    QCOMPARE(controller.history({}).size(), 1);
}

void BrowserControllerTest::keepsReportedIconAndAudibilityAcrossSpaceSwitches()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalSpaceId = controller.activeSpaceId();
    controller.openInput(QStringLiteral("https://example.com"), false);
    const auto tabId = controller.activeTabId();
    const QUrl icon(QStringLiteral("https://example.com/favicon.png"));
    controller.reportTabPageState(
        tabId, controller.activeUrl(), QStringLiteral("Example"), icon, false, true);
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));

    QVERIFY(controller.switchSpace(workSpaceId));
    QVERIFY(controller.switchSpace(personalSpaceId));

    const auto index = controller.tabs()->index(0, 0);
    QCOMPARE(controller.tabs()->data(index, TabListModel::IconUrlRole).toUrl(), icon);
    QVERIFY(controller.tabs()->data(index, TabListModel::AudibleRole).toBool());
}

void BrowserControllerTest::dropsReportedPageStateWhenMovingATabBetweenSpaces()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalSpaceId = controller.activeSpaceId();
    controller.openInput(QStringLiteral("https://example.com"), false);
    const auto tabId = controller.activeTabId();
    const QUrl icon(QStringLiteral("https://example.com/favicon.png"));
    controller.reportTabPageState(
        tabId, controller.activeUrl(), QStringLiteral("Example"), icon, false, true);
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    QVERIFY(controller.switchSpace(personalSpaceId));

    QVERIFY(controller.confirmTabMoveToSpace(tabId, workSpaceId));
    QVERIFY(controller.switchSpace(workSpaceId));

    auto *tabs = controller.tabs();
    for (int row = 0; row < tabs->rowCount(); ++row) {
        const auto index = tabs->index(row, 0);
        if (tabs->data(index, TabListModel::IdRole).toString() != tabId) {
            continue;
        }
        QVERIFY(tabs->data(index, TabListModel::IconUrlRole).toUrl().isEmpty());
        QVERIFY(!tabs->data(index, TabListModel::AudibleRole).toBool());
        return;
    }
    QFAIL("moved tab is missing from its destination Space");
}

void BrowserControllerTest::exposesOnlyCombinedPageStateReportsToQml()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto *metaObject = controller.metaObject();

    QVERIFY(
        metaObject->indexOfMethod("reportTabPageState(QString,QUrl,QString,QUrl,bool,bool)") >= 0);
    QCOMPARE(metaObject->indexOfMethod("setTabIcon(QString,QUrl)"), -1);
    QCOMPARE(metaObject->indexOfMethod("setTabLoading(QString,bool)"), -1);
    QCOMPARE(metaObject->indexOfMethod("setTabAudible(QString,bool)"), -1);
}

void BrowserControllerTest::dropsThePreviousHostsIconFromPageReports()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto tabId = controller.activeTabId();
    const QUrl firstAddress(QStringLiteral("https://first.example/page"));
    const QUrl firstIcon(QStringLiteral("https://first.example/favicon.png"));
    controller.reportTabPageState(
        tabId, firstAddress, QStringLiteral("First"), firstIcon, false, false);
    controller.reportTabPageState(
        tabId, firstAddress, QStringLiteral("First"), firstIcon, false, false);
    const auto index = controller.tabs()->index(0, 0);
    QCOMPARE(controller.tabs()->data(index, TabListModel::IconUrlRole).toUrl(), firstIcon);

    controller.reportTabPageState(tabId, QUrl(QStringLiteral("https://second.example/page")),
        QStringLiteral("Second"), firstIcon, false, false);

    QVERIFY(controller.tabs()->data(index, TabListModel::IconUrlRole).toUrl().isEmpty());
}

// Sound is the page's to report and muting is the reader's to decide, so the
// two are held apart: a page that falls silent leaves the tab muted, because
// the reader silenced the tab and not one clip in it.
void BrowserControllerTest::keepsMutingDecisionWhileSoundComesAndGoes()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto tabId = controller.activeTabId();
    auto *tabs = controller.tabs();
    const auto tabIndex = tabs->index(0, 0);

    QVERIFY(!tabs->data(tabIndex, TabListModel::AudibleRole).toBool());
    QVERIFY(!tabs->data(tabIndex, TabListModel::MutedRole).toBool());

    QSignalSpy changes(tabs, &QAbstractItemModel::dataChanged);
    controller.setTabAudible(tabId, true);
    QVERIFY(tabs->data(tabIndex, TabListModel::AudibleRole).toBool());
    QCOMPARE(changes.count(), 1);

    // Saying again what the row already says would repaint every listening
    // tab on every report the engine makes.
    controller.setTabAudible(tabId, true);
    QCOMPARE(changes.count(), 1);

    controller.toggleTabMuted(tabId);
    QVERIFY(tabs->data(tabIndex, TabListModel::MutedRole).toBool());

    controller.setTabAudible(tabId, false);
    QVERIFY(!tabs->data(tabIndex, TabListModel::AudibleRole).toBool());
    QVERIFY(tabs->data(tabIndex, TabListModel::MutedRole).toBool());

    controller.toggleTabMuted(tabId);
    QVERIFY(!tabs->data(tabIndex, TabListModel::MutedRole).toBool());
}

// Zoom belongs to one tab: stepping it moves that tab and no other, and the
// ladder is the same going up and coming back down.
void BrowserControllerTest::stepsZoomAlongOneLadderPerTab()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://first.example"), false);
    const auto firstTabId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://second.example"), true);
    const auto secondTabId = controller.activeTabId();

    QCOMPARE(controller.activeTabZoom(), 1.0);
    controller.stepActiveZoom(1);
    QCOMPARE(controller.activeTabZoom(), 1.1);
    controller.stepActiveZoom(1);
    QCOMPARE(controller.activeTabZoom(), 1.25);
    controller.stepActiveZoom(-1);
    QCOMPARE(controller.activeTabZoom(), 1.1);

    // The other tab never moved, and the model says so for both.
    auto *tabs = controller.tabs();
    const auto zoomOf = [tabs](const QString &tabId) {
        for (int row = 0; row < tabs->rowCount(); ++row) {
            const auto index = tabs->index(row, 0);
            if (tabs->data(index, TabListModel::IdRole).toString() == tabId) {
                return tabs->data(index, TabListModel::ZoomRole).toDouble();
            }
        }
        return 0.0;
    };
    QCOMPARE(zoomOf(firstTabId), 1.0);
    QCOMPARE(zoomOf(secondTabId), 1.1);

    // The ends of the ladder hold: asking for more than it has changes nothing.
    for (int step = 0; step < 20; ++step) {
        controller.stepActiveZoom(1);
    }
    QCOMPARE(controller.activeTabZoom(), 3.0);
    for (int step = 0; step < 20; ++step) {
        controller.stepActiveZoom(-1);
    }
    QCOMPARE(controller.activeTabZoom(), 0.25);

    controller.resetActiveZoom();
    QCOMPARE(controller.activeTabZoom(), 1.0);

    // A new tab starts at 100 percent whatever the tab beside it is drawn at.
    controller.stepActiveZoom(1);
    controller.openInput(QStringLiteral("https://third.example"), true);
    QCOMPARE(controller.activeTabZoom(), 1.0);
}

void BrowserControllerTest::restoresEveryTabsZoomAfterRestart()
{
    SessionFixture fixture(SessionSpec {
        .spaces = {SpaceSpec {
            .id = QStringLiteral("personal"),
            .name = QStringLiteral("Personal"),
            .tabs = {
                TabSpec {
                    .id = QStringLiteral("plain"),
                    .url = QUrl(QStringLiteral("https://plain.example")),
                },
                TabSpec {
                    .id = QStringLiteral("zoomed"),
                    .url = QUrl(QStringLiteral("https://zoomed.example")),
                    .zoom = 1.25,
                },
            },
            .activeTabId = QStringLiteral("zoomed"),
        }},
    });
    QVERIFY_SESSION_READY(fixture);

    const auto restored = fixture.createController();
    QCOMPARE(restored->activeTabId(), QStringLiteral("zoomed"));
    QCOMPARE(restored->activeTabZoom(), 1.25);
    restored->activateTab(QStringLiteral("plain"));
    QCOMPARE(restored->activeTabZoom(), 1.0);
}

void BrowserControllerTest::sharesPrivateIdentityUntilLastWindowCloses()
{
    WindowManager manager;
    auto *first = manager.createPrivateWindow();
    auto *second = manager.createPrivateWindow();

    QVERIFY(first);
    QVERIFY(second);
    QCOMPARE(manager.privateWindowCount(), 2);
    const auto sharedProfilePath = manager.privateProfilePath();
    QVERIFY(!sharedProfilePath.isEmpty());
    QVERIFY(QFileInfo::exists(sharedProfilePath));
    QCOMPARE(first->spaces()->rowCount(), 0);
    QCOMPARE(second->spaces()->rowCount(), 0);
    QVERIFY(first->activeSpaceId().isEmpty());
    // A Private window holds no Space storage, so it says where nothing
    // belongs without being asked whether it is private.
    QVERIFY(first->activeProfilePath().isEmpty());
    QVERIFY(first->profilePathForSpace(QStringLiteral("personal")).isEmpty());
    QVERIFY(first->prepareProfileForSpace(QStringLiteral("personal")).isEmpty());
    QVERIFY(first->acceptDownloads());
    QCOMPARE(first->downloadDirectory(),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    QTemporaryDir elsewhere;
    QVERIFY(!first->setDownloadDirectory(elsewhere.path()));
    QCOMPARE(first->downloads()->rowCount(), 0);
    QVERIFY(first->setPermissionDecision(QUrl(QStringLiteral("https://camera.example")),
        QStringLiteral("camera"), BrowserController::AllowOnce));
    QCOMPARE(second->permissionDecision(
                 QUrl(QStringLiteral("https://camera.example/path")), QStringLiteral("camera")),
        BrowserController::AllowOnce);
    QCOMPARE(second->permissionDecision(
                 QUrl(QStringLiteral("https://camera.example/path")), QStringLiteral("camera")),
        BrowserController::Ask);

    QSignalSpy closeSpy(first, &BrowserController::closeWindowRequested);
    first->closeActiveTab();
    QCOMPARE(closeSpy.count(), 1);

    manager.releasePrivateWindow(first);
    QCOMPARE(manager.privateWindowCount(), 1);
    QVERIFY(QFileInfo::exists(sharedProfilePath));

    manager.releasePrivateWindow(second);
    QCOMPARE(manager.privateWindowCount(), 0);
    QTRY_VERIFY(!QFileInfo::exists(sharedProfilePath));

    auto *fresh = manager.createPrivateWindow();
    QVERIFY(fresh);
    QVERIFY(manager.privateProfilePath() != sharedProfilePath);
    QCOMPARE(fresh->activeUrl(), QUrl(QStringLiteral("about:blank")));
    QCOMPARE(fresh->permissionDecision(
                 QUrl(QStringLiteral("https://camera.example")), QStringLiteral("camera")),
        BrowserController::Ask);
    manager.releasePrivateWindow(fresh);
}

void BrowserControllerTest::keepsHistorySuggestionsInsideActiveSpace()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalSpaceId = controller.activeSpaceId();
    controller.recordVisit(QUrl(QStringLiteral("https://docs.example/personal")),
        QStringLiteral("Personal documentation"));
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    controller.recordVisit(
        QUrl(QStringLiteral("https://docs.example/work")), QStringLiteral("Work documentation"));
    QSignalSpy readySpy(&controller, &BrowserController::historySuggestionsReady);

    controller.requestHistorySuggestions(QStringLiteral("docs"));
    QTRY_COMPARE(readySpy.count(), 1);
    const auto workSuggestions = readySpy.takeFirst().first().toList();
    QCOMPARE(workSuggestions.size(), 1);
    QCOMPARE(workSuggestions.first().toMap().value(QStringLiteral("url")).toUrl(),
        QUrl(QStringLiteral("https://docs.example/work")));

    QVERIFY(controller.switchSpace(personalSpaceId));
    controller.requestHistorySuggestions(QStringLiteral("documentation"));
    QTRY_COMPARE(readySpy.count(), 1);
    const auto personalSuggestions = readySpy.takeFirst().first().toList();
    QCOMPARE(personalSuggestions.size(), 1);
    QCOMPARE(personalSuggestions.first().toMap().value(QStringLiteral("url")).toUrl(),
        QUrl(QStringLiteral("https://docs.example/personal")));
}

// Retention is a bound on the Space, not a step taken when its database is
// opened. A session that never restarts still trims, within one batch of the
// bound, and keeps the most recent visits. The rows are read back through the
// store, which answers once every visit given before the question has landed.
void BrowserControllerTest::keepsHistoryInsideItsBoundWhileASpaceStaysOpen()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto visits = omaweb::history::retainedRows + omaweb::history::cleanupBatch + 20;
    for (int visit = 0; visit < visits; ++visit) {
        controller.recordVisit(QUrl(QStringLiteral("https://example.test/page/%1").arg(visit)),
            QStringLiteral("Example page %1").arg(visit));
    }

    const auto history = controller.history({}, visits + 1);
    const auto rows = history.size();
    QVERIFY2(rows <= omaweb::history::retainedRows + omaweb::history::cleanupBatch,
        qPrintable(QStringLiteral("history holds %1 rows").arg(rows)));
    // Trimming from the wrong end would leave a bound and no History.
    QVERIFY(rows >= omaweb::history::retainedRows);
    QCOMPARE(history.first().toMap().value(QStringLiteral("url")).toUrl(),
        QUrl(QStringLiteral("https://example.test/page/%1").arg(visits - 1)));
}

// Typing is faster than the store answers, so requests coalesce: one search
// runs, one waits, and the reader is shown the answer to what they last typed
// rather than to what they typed first.
void BrowserControllerTest::answersOnlyTheLatestOmnibarSearch()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.recordVisit(
        QUrl(QStringLiteral("https://alpha.example/one")), QStringLiteral("Alpha one"));
    controller.recordVisit(
        QUrl(QStringLiteral("https://omega.example/two")), QStringLiteral("Omega two"));
    controller.setHistorySearchDelayForTests(60);

    QSignalSpy readySpy(&controller, &BrowserController::historySuggestionsReady);
    controller.requestHistorySuggestions(QStringLiteral("alpha"));
    controller.requestHistorySuggestions(QStringLiteral("nothing-matches-this"));
    controller.requestHistorySuggestions(QStringLiteral("omega"));
    // The GUI thread is free while the search runs: this is the only reason
    // the third request can be made before the first has answered.
    QCOMPARE(readySpy.count(), 0);

    QTRY_COMPARE(readySpy.count(), 1);
    const auto suggestions = readySpy.takeFirst().first().toList();
    QCOMPARE(suggestions.size(), 1);
    QCOMPARE(suggestions.first().toMap().value(QStringLiteral("url")).toUrl(),
        QUrl(QStringLiteral("https://omega.example/two")));
    // Nothing arrives for the two requests that were replaced.
    QTest::qWait(120);
    QCOMPARE(readySpy.count(), 0);
}

// A search in flight outlives the state it was asked about. Every route out of
// that state discards its answer rather than drawing it over what replaced it.
void BrowserControllerTest::abandonsASearchTheReaderHasMovedOnFrom()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalSpaceId = controller.activeSpaceId();
    controller.recordVisit(
        QUrl(QStringLiteral("https://alpha.example/one")), QStringLiteral("Alpha one"));
    controller.setHistorySearchDelayForTests(60);
    QSignalSpy readySpy(&controller, &BrowserController::historySuggestionsReady);

    // The Omnibar closes while a search runs.
    controller.requestHistorySuggestions(QStringLiteral("alpha"));
    controller.cancelHistorySuggestions();
    QTest::qWait(160);
    QCOMPARE(readySpy.count(), 0);

    // The reader switches Space, whose History is not the one being searched.
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    controller.requestHistorySuggestions(QStringLiteral("alpha"));
    QVERIFY(controller.switchSpace(workSpaceId));
    QTest::qWait(160);
    QCOMPARE(readySpy.count(), 0);

    // The History being searched is deleted.
    QVERIFY(controller.switchSpace(personalSpaceId));
    controller.requestHistorySuggestions(QStringLiteral("alpha"));
    QVERIFY(controller.deleteHistorySince(0));
    QTest::qWait(160);
    QCOMPARE(readySpy.count(), 0);

    // With nothing in the way, the same request answers.
    controller.recordVisit(
        QUrl(QStringLiteral("https://alpha.example/again")), QStringLiteral("Alpha again"));
    controller.requestHistorySuggestions(QStringLiteral("alpha"));
    QTRY_COMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.takeFirst().first().toList().size(), 1);
}

void BrowserControllerTest::filtersAndDeletesHistoryAtRequestedBoundaries()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.recordVisit(
        QUrl(QStringLiteral("https://one.example/first")), QStringLiteral("First visit"));
    QTest::qWait(2);
    const auto boundary = QDateTime::currentMSecsSinceEpoch();
    QTest::qWait(2);
    controller.recordVisit(
        QUrl(QStringLiteral("https://one.example/second")), QStringLiteral("Second visit"));
    controller.recordVisit(
        QUrl(QStringLiteral("https://two.example/keep")), QStringLiteral("Keep this"));

    auto rows = controller.history(QStringLiteral("one.example"));
    QCOMPARE(rows.size(), 2);
    QVERIFY(controller.deleteHistoryVisit(
        rows.first().toMap().value(QStringLiteral("id")).toLongLong()));
    QCOMPARE(controller.history(QStringLiteral("one.example")).size(), 1);

    QVERIFY(controller.deleteHistorySince(boundary));
    QCOMPARE(controller.history({}).size(), 1);
    QCOMPARE(controller.history({}).first().toMap().value(QStringLiteral("title")).toString(),
        QStringLiteral("First visit"));

    controller.recordVisit(QUrl(QStringLiteral("https://one.example/a")), QStringLiteral("A"));
    controller.recordVisit(QUrl(QStringLiteral("https://one.example:444/b")), QStringLiteral("B"));
    controller.recordVisit(QUrl(QStringLiteral("https://other.example/c")), QStringLiteral("C"));
    QVERIFY(controller.deleteHistoryOrigin(QUrl(QStringLiteral("https://one.example/path"))));
    QCOMPARE(controller.history(QStringLiteral("one.example")).size(), 1);
    QCOMPARE(controller.history(QStringLiteral("one.example"))
                 .first()
                 .toMap()
                 .value(QStringLiteral("url"))
                 .toUrl()
                 .port(),
        444);
}

void BrowserControllerTest::resolvesAddressesBeforeSearches()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));

    controller.openInput(QStringLiteral("example.com/path"), false);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://example.com/path")));
    controller.openInput(QStringLiteral("localhost:3000/app"), false);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("http://localhost:3000/app")));
    controller.openInput(QStringLiteral("localhost?mode=test"), false);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("http://localhost?mode=test")));
    controller.openInput(QStringLiteral("127.0.0.1:8080"), false);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("http://127.0.0.1:8080")));
    controller.openInput(QStringLiteral("::1"), false);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("http://[::1]")));
    controller.openInput(QStringLiteral("project.test"), false);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("http://project.test")));
    controller.openInput(QStringLiteral("example.com:444/path"), false);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://example.com:444/path")));
    QVERIFY(controller.retryActiveUrlInsecurely());
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("http://example.com:444/path")));
    controller.openInput(QStringLiteral("http://example.com/plain"), false);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("http://example.com/plain")));
    controller.openInput(QStringLiteral("words to search"), false);
    QCOMPARE(controller.activeUrl().host(), QStringLiteral("duckduckgo.com"));
}

void BrowserControllerTest::migratesAndUsesSearchEngineConfiguration()
{
    QTemporaryDir root;
    const auto configRoot = root.filePath(QStringLiteral("config"));
    QVERIFY(QDir().mkpath(configRoot));
    QFile legacy(QDir(configRoot).filePath(QStringLiteral("search-engines.json")));
    QVERIFY(legacy.open(QIODevice::WriteOnly));
    const auto written = legacy.write(R"JSON({
        "default": "docs",
        "engines": [{
            "id": "docs", "name": "Docs", "queryUrl": "https://docs.example/?q={query}",
            "keyword": "d"
        }]
    })JSON");
    QVERIFY(written > 0);
    legacy.close();

    BrowserController controller(
        SpaceStorage(root.filePath(QStringLiteral("data")), QStringLiteral("test")), configRoot);
    QVERIFY(controller.ready());
    QCOMPARE(controller.searchEngines().size(), 1);
    controller.openInput(QStringLiteral("migration guide"), false);
    QCOMPARE(controller.activeUrl().host(), QStringLiteral("docs.example"));
    controller.openInput(QStringLiteral("d shortcuts"), false);
    QCOMPARE(QUrlQuery(controller.activeUrl()).queryItemValue(QStringLiteral("q")),
        QStringLiteral("shortcuts"));

    QFile migrated(legacy.fileName());
    QVERIFY(migrated.open(QIODevice::ReadOnly));
    const auto document = QJsonDocument::fromJson(migrated.readAll());
    QCOMPARE(document.object().value(QStringLiteral("version")).toInt(), 1);

    PrivateSessionFixture privateSession(configRoot);
    auto privateController = privateSession.createController();
    privateController->openInput(QStringLiteral("private search"), false);
    QCOMPARE(privateController->activeUrl().host(), QStringLiteral("docs.example"));
}

void BrowserControllerTest::addsPredefinedSearchEngineProviders()
{
    QTemporaryDir root;
    BrowserController controller(
        SpaceStorage(root.filePath(QStringLiteral("data")), QStringLiteral("test")),
        root.filePath(QStringLiteral("config")));

    const auto presets = controller.searchEnginePresets();
    QVERIFY(presets.size() >= 5);
    QVERIFY(controller.addSearchEnginePreset(QStringLiteral("brave")));
    QVERIFY(!controller.addSearchEnginePreset(QStringLiteral("brave")));
    QCOMPARE(controller.searchEngines().size(), 2);

    controller.openInput(QStringLiteral("br private search"), false);
    QCOMPARE(controller.activeUrl().host(), QStringLiteral("search.brave.com"));
}

// The Omnibar asks this on every keystroke and the commit reads the same
// answer, so what the panel names is where Return goes.
void BrowserControllerTest::namesTheEngineATypedKeywordSelects()
{
    QTemporaryDir root;
    BrowserController controller(
        SpaceStorage(root.filePath(QStringLiteral("data")), QStringLiteral("test")),
        root.filePath(QStringLiteral("config")));
    QVERIFY(controller.addSearchEnginePreset(QStringLiteral("google")));

    auto intent = controller.searchIntent(QStringLiteral("g rust lifetimes"));
    QCOMPARE(intent.value(QStringLiteral("engineId")).toString(), QStringLiteral("google"));
    QCOMPARE(intent.value(QStringLiteral("engineName")).toString(), QStringLiteral("Google"));
    QCOMPARE(intent.value(QStringLiteral("terms")).toString(), QStringLiteral("rust lifetimes"));
    QCOMPARE(intent.value(QStringLiteral("keyword")).toString(), QStringLiteral("g"));
    controller.openInput(QStringLiteral("g rust lifetimes"), false);
    QCOMPARE(controller.activeUrl(),
        QUrl(QStringLiteral("https://www.google.com/search?q=rust%20lifetimes")));

    // The keyword alone is the engine's front page, not a search for the letter.
    intent = controller.searchIntent(QStringLiteral("g"));
    QCOMPARE(intent.value(QStringLiteral("engineId")).toString(), QStringLiteral("google"));
    QCOMPARE(intent.value(QStringLiteral("terms")).toString(), QString {});
    controller.openInput(QStringLiteral("g"), false);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://www.google.com/")));

    // A mistyped keyword is a default-engine search for the whole text, and
    // the intent says so rather than saying nothing.
    intent = controller.searchIntent(QStringLiteral("gg rust"));
    QCOMPARE(intent.value(QStringLiteral("engineId")).toString(), QStringLiteral("duckduckgo"));
    QCOMPARE(intent.value(QStringLiteral("engineName")).toString(), QStringLiteral("DuckDuckGo"));
    QCOMPARE(intent.value(QStringLiteral("terms")).toString(), QStringLiteral("gg rust"));
    QCOMPARE(intent.value(QStringLiteral("keyword")).toString(), QString {});

    QVERIFY(controller.searchIntent(QStringLiteral("example.com")).isEmpty());
    QVERIFY(controller.searchIntent(QStringLiteral("localhost:3000 g")).isEmpty());
    QVERIFY(controller.searchIntent(QStringLiteral("   ")).isEmpty());

    // A held shift key does not change the engine.
    intent = controller.searchIntent(QStringLiteral("G rust"));
    QCOMPARE(intent.value(QStringLiteral("engineId")).toString(), QStringLiteral("google"));
    QCOMPARE(intent.value(QStringLiteral("keyword")).toString(), QStringLiteral("g"));
    controller.openInput(QStringLiteral("G rust"), false);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://www.google.com/search?q=rust")));

    // Keywords are one namespace whatever their case, and are kept lowercased.
    QVERIFY(!controller.addSearchEngine(QStringLiteral("Shouty"),
        QStringLiteral("https://shouty.example/?q={query}"), QStringLiteral("G")));
    QVERIFY(controller.addSearchEngine(QStringLiteral("Docs"),
        QStringLiteral("https://docs.example/?q={query}"), QStringLiteral("DOC")));
    QCOMPARE(controller.searchEngines().last().toMap().value(QStringLiteral("keyword")).toString(),
        QStringLiteral("doc"));
    QCOMPARE(controller.searchIntent(QStringLiteral("Doc api"))
                 .value(QStringLiteral("engineId"))
                 .toString(),
        QStringLiteral("docs"));

    // A file from before keywords shared one case keeps every engine; the
    // second `g` loses its keyword rather than the reader losing the list.
    QFile file(QDir(root.filePath(QStringLiteral("config")))
            .filePath(QStringLiteral("search-engines.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    const auto written = file.write(R"JSON({
        "version": 1, "default": "duckduckgo",
        "engines": [
            {"id": "duckduckgo", "name": "DuckDuckGo",
             "queryUrl": "https://duckduckgo.com/?q={query}", "keyword": ""},
            {"id": "google", "name": "Google",
             "queryUrl": "https://www.google.com/search?q={query}", "keyword": "g"},
            {"id": "shouty", "name": "Shouty",
             "queryUrl": "https://shouty.example/?q={query}", "keyword": "G"}
        ]
    })JSON");
    QVERIFY(written > 0);
    file.close();
    BrowserController reloaded(
        SpaceStorage(root.filePath(QStringLiteral("reloaded")), QStringLiteral("test")),
        root.filePath(QStringLiteral("config")));
    QCOMPARE(reloaded.searchEngines().size(), 3);
    QCOMPARE(reloaded.searchEngines().last().toMap().value(QStringLiteral("keyword")).toString(),
        QString {});
    QCOMPARE(reloaded.searchIntent(QStringLiteral("G rust"))
                 .value(QStringLiteral("engineId"))
                 .toString(),
        QStringLiteral("google"));
}

void BrowserControllerTest::offersTheKeywordsATypedPrefixCouldBecome()
{
    QTemporaryDir root;
    BrowserController controller(
        SpaceStorage(root.filePath(QStringLiteral("data")), QStringLiteral("test")),
        root.filePath(QStringLiteral("config")));
    QVERIFY(controller.addSearchEnginePreset(QStringLiteral("bing")));
    QVERIFY(controller.addSearchEnginePreset(QStringLiteral("brave")));
    QVERIFY(controller.addSearchEnginePreset(QStringLiteral("google")));
    QVERIFY(controller.setDefaultSearchEngine(QStringLiteral("bing")));

    // The default engine is what plain text already searches, so it is not
    // offered; Brave Search is.
    auto offers = controller.searchKeywordOffers(QStringLiteral("b"));
    QCOMPARE(offers.size(), 1);
    QCOMPARE(offers.first().toMap().value(QStringLiteral("engineId")).toString(),
        QStringLiteral("brave"));
    QCOMPARE(offers.first().toMap().value(QStringLiteral("engineName")).toString(),
        QStringLiteral("Brave Search"));
    QCOMPARE(
        offers.first().toMap().value(QStringLiteral("keyword")).toString(), QStringLiteral("br"));

    QCOMPARE(controller.searchKeywordOffers(QStringLiteral("B")).size(), 1);
    QCOMPARE(controller.searchKeywordOffers(QStringLiteral("br")).size(), 1);
    QCOMPARE(controller.searchKeywordOffers(QStringLiteral("g")).size(), 1);
    QCOMPARE(controller.searchKeywordOffers(QStringLiteral("x")).size(), 0);
    // The empty Omnibar stays what it is, and a space has already chosen.
    QCOMPARE(controller.searchKeywordOffers(QString {}).size(), 0);
    QCOMPARE(controller.searchKeywordOffers(QStringLiteral("b ")).size(), 0);
}

namespace {

// The four capabilities a window either has or has not, asked of a window that
// has them and of one that has not. Every place a Private window refuses is
// reached from here, so a capability that stops being asked for shows up as a
// main window refusing or a Private window agreeing.

enum class Window { Main, Private };

std::unique_ptr<BrowserController> makeControllerFor(
    Window window, const QString &dataRoot, const PrivateSessionFixture &privateSession)
{
    if (window == Window::Private) {
        return privateSession.createController();
    }
    return std::make_unique<BrowserController>(SpaceStorage(dataRoot, QStringLiteral("test")));
}

void exerciseSpaces(Window window)
{
    const bool allowed = window == Window::Main;
    QTemporaryDir root;
    PrivateSessionFixture privateSession;
    auto controller = makeControllerFor(window, root.path(), privateSession);
    const auto personalSpaceId = controller->activeSpaceId();

    const auto workSpaceId = controller->createSpace(QStringLiteral("Work"));
    QCOMPARE(!workSpaceId.isEmpty(), allowed);
    QCOMPARE(controller->switchSpace(workSpaceId), allowed);
    QCOMPARE(controller->renameSpace(workSpaceId, QStringLiteral("Deep work")), allowed);
    QCOMPARE(controller->switchSpace(personalSpaceId), allowed);

    const auto tabId = controller->activeTabId();
    QSignalSpy confirmationSpy(controller.get(), &BrowserController::tabMoveConfirmationRequested);
    QCOMPARE(controller->requestTabMoveToSpace(tabId, workSpaceId, true), allowed);
    QCOMPARE(confirmationSpy.count(), allowed ? 1 : 0);
    QCOMPARE(controller->confirmTabMoveToSpace(tabId, workSpaceId), allowed);
    QCOMPARE(controller->moveSpaceBy(workSpaceId, -1), allowed);
    QCOMPARE(controller->deleteSpace(workSpaceId, QStringLiteral("Deep work")), allowed);
}

void exercisePinnedTabs(Window window)
{
    const bool allowed = window == Window::Main;
    QTemporaryDir root;
    PrivateSessionFixture privateSession;
    auto controller = makeControllerFor(window, root.path(), privateSession);
    controller->openInput(QStringLiteral("https://pinned.example"), false);
    const auto tabId = controller->activeTabId();

    controller->toggleActivePinned();
    QCOMPARE(controller->activeTabPinned(), allowed);
    QCOMPARE(controller->setTabKeepActive(tabId, true), allowed);
    QCOMPARE(controller->activeTabKeepActive(), allowed);
    QCOMPARE(controller->releaseRetainedTab(tabId), allowed);
    QVERIFY(!controller->activeTabKeepActive());

    // Reopening is the one place that answers by clearing rather than by
    // refusing, so both windows reopen. What a window without Pinned tabs
    // clears cannot be produced here: closing spares a Pinned tab, so no close
    // a window remembers was ever pinned.
    controller->openInput(QStringLiteral("https://second.example"), true);
    const auto secondTabId = controller->activeTabId();
    controller->closeTab(secondTabId);
    QCOMPARE(controller->closedTabCount(), 1);
    controller->reopenClosedTab();
    QCOMPARE(controller->closedTabCount(), 0);
    QCOMPARE(controller->activeUrl(), QUrl(QStringLiteral("https://second.example")));
    QVERIFY(!controller->activeTabPinned());
    QVERIFY(!controller->activeTabKeepActive());
}

void exerciseHistorySearch(Window window)
{
    const bool allowed = window == Window::Main;
    QTemporaryDir root;
    PrivateSessionFixture privateSession;
    auto controller = makeControllerFor(window, root.path(), privateSession);
    controller->recordVisit(
        QUrl(QStringLiteral("https://searched.example")), QStringLiteral("Searched"));
    QSignalSpy readySpy(controller.get(), &BrowserController::historySuggestionsReady);

    controller->requestHistorySuggestions(QStringLiteral("searched"));
    // A window with no search answers on the spot, having no thread to ask.
    QCOMPARE(readySpy.count() == 1, !allowed);
    QTRY_COMPARE(readySpy.count(), 1);
    QCOMPARE(!readySpy.takeFirst().first().toList().isEmpty(), allowed);
    // One answer and no more: a window with no search answered at once and off
    // no thread, and a window with one has nothing further to say.
    QVERIFY(!readySpy.wait(100));
}

void exerciseClearBrowsingData(Window window)
{
    const bool allowed = window == Window::Main;
    QTemporaryDir root;
    PrivateSessionFixture privateSession;
    auto controller = makeControllerFor(window, root.path(), privateSession);
    controller->recordVisit(
        QUrl(QStringLiteral("https://cleared.example")), QStringLiteral("Cleared"));
    QSignalSpy clearSpy(controller.get(), &BrowserController::engineDataClearRequested);

    QCOMPARE(controller->clearBrowsingData({QStringLiteral("history")}, 0, false, {}), allowed);
    QCOMPARE(clearSpy.count(), allowed ? 1 : 0);
    QCOMPARE(controller->history({}).size(), 0);
}

} // namespace

void BrowserControllerTest::allowsEveryWindowCapabilityInAMainWindow()
{
    exerciseSpaces(Window::Main);
    exercisePinnedTabs(Window::Main);
    exerciseHistorySearch(Window::Main);
    exerciseClearBrowsingData(Window::Main);
}

void BrowserControllerTest::refusesEveryWindowCapabilityInAPrivateWindow()
{
    exerciseSpaces(Window::Private);
    exercisePinnedTabs(Window::Private);
    exerciseHistorySearch(Window::Private);
    exerciseClearBrowsingData(Window::Private);
}

void BrowserControllerTest::clearsSelectedBrowsingDataWithinConfirmedScope()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalSpaceId = controller.activeSpaceId();
    controller.recordVisit(
        QUrl(QStringLiteral("https://personal-clear.example")), QStringLiteral("Personal"));
    QVERIFY(controller.setPermissionDecision(QUrl(QStringLiteral("https://personal-clear.example")),
        QStringLiteral("camera"), BrowserController::Block));
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    controller.recordVisit(
        QUrl(QStringLiteral("https://work-clear.example")), QStringLiteral("Work"));
    QSignalSpy clearSpy(&controller, &BrowserController::engineDataClearRequested);

    QVERIFY(controller.clearBrowsingData({QStringLiteral("history")}, 0, false, {}));
    QCOMPARE(controller.history({}).size(), 0);
    QCOMPARE(clearSpy.count(), 1);
    QVERIFY(controller.switchSpace(personalSpaceId));
    QCOMPARE(controller.history({}).size(), 1);
    QCOMPARE(controller.permissionDecision(
                 QUrl(QStringLiteral("https://personal-clear.example")), QStringLiteral("camera")),
        BrowserController::Block);

    QVERIFY(
        !controller.clearBrowsingData({QStringLiteral("history"), QStringLiteral("permissions")}, 0,
            true, QStringLiteral("clear all")));
    QVERIFY(controller.clearBrowsingData({QStringLiteral("history"), QStringLiteral("permissions")},
        0, true, QStringLiteral("CLEAR ALL")));
    QCOMPARE(controller.history({}).size(), 0);
    QCOMPARE(controller.permissionDecision(
                 QUrl(QStringLiteral("https://personal-clear.example")), QStringLiteral("camera")),
        BrowserController::Ask);
}

void BrowserControllerTest::scopesPermissionDecisionsToOriginSpaceAndLifetime()
{
    QTemporaryDir root;
    QString personalSpaceId;
    QString workSpaceId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        personalSpaceId = controller.activeSpaceId();
        QVERIFY(controller.setPermissionDecision(
            QUrl(QStringLiteral("https://EXAMPLE.com/path?ignored=1")),
            QStringLiteral("geolocation"), BrowserController::AllowPersistently));
        QCOMPARE(controller.permissionDecision(QUrl(QStringLiteral("https://example.com/another")),
                     QStringLiteral("geolocation")),
            BrowserController::AllowPersistently);
        QCOMPARE(controller.permissionDecision(
                     QUrl(QStringLiteral("https://example.com:443/default-port")),
                     QStringLiteral("geolocation")),
            BrowserController::AllowPersistently);
        QCOMPARE(controller.permissionDecision(QUrl(QStringLiteral("https://sub.example.com")),
                     QStringLiteral("geolocation")),
            BrowserController::Ask);
        QCOMPARE(controller.permissionDecision(QUrl(QStringLiteral("https://example.com:444")),
                     QStringLiteral("geolocation")),
            BrowserController::Ask);

        QVERIFY(controller.setPermissionDecision(QUrl(QStringLiteral("https://once.example")),
            QStringLiteral("notifications"), BrowserController::AllowOnce));
        workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workSpaceId));
        QCOMPARE(controller.permissionDecision(
                     QUrl(QStringLiteral("https://example.com")), QStringLiteral("geolocation")),
            BrowserController::Ask);
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY(restored.switchSpace(personalSpaceId));
    QCOMPARE(restored.permissionDecision(
                 QUrl(QStringLiteral("https://example.com")), QStringLiteral("geolocation")),
        BrowserController::AllowPersistently);
    QCOMPARE(restored.permissionDecision(
                 QUrl(QStringLiteral("https://once.example")), QStringLiteral("notifications")),
        BrowserController::Ask);
}

void BrowserControllerTest::scopesExternalProtocolDecisionsToOriginSchemeSpaceAndPrivateSession()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalSpaceId = controller.activeSpaceId();

    QVERIFY(controller.rememberExternalProtocolDecision(
        QUrl(QStringLiteral("https://EXAMPLE.com/page")), QStringLiteral("MailTo")));
    QVERIFY(controller.externalProtocolAllowed(
        QUrl(QStringLiteral("https://example.com/another")), QStringLiteral("mailto")));
    QVERIFY(!controller.externalProtocolAllowed(
        QUrl(QStringLiteral("https://elsewhere.example")), QStringLiteral("mailto")));
    QVERIFY(!controller.externalProtocolAllowed(
        QUrl(QStringLiteral("https://example.com")), QStringLiteral("webcal")));

    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    QVERIFY(!controller.externalProtocolAllowed(
        QUrl(QStringLiteral("https://example.com")), QStringLiteral("mailto")));
    QVERIFY(controller.switchSpace(personalSpaceId));
    QVERIFY(controller.externalProtocolAllowed(
        QUrl(QStringLiteral("https://example.com")), QStringLiteral("mailto")));

    PrivateSessionFixture sharedPrivateSession;
    auto firstPrivate = sharedPrivateSession.createController();
    auto secondPrivate = sharedPrivateSession.createController();
    QVERIFY(firstPrivate->rememberExternalProtocolDecision(
        QUrl(QStringLiteral("https://private.example")), QStringLiteral("mailto")));
    QVERIFY(secondPrivate->externalProtocolAllowed(
        QUrl(QStringLiteral("https://private.example/path")), QStringLiteral("mailto")));
    QVERIFY(secondPrivate->externalProtocolAllowed(
        QUrl(QStringLiteral("https://private.example/path")), QStringLiteral("mailto")));

    PrivateSessionFixture freshPrivateSession;
    auto freshPrivate = freshPrivateSession.createController();
    QVERIFY(!freshPrivate->externalProtocolAllowed(
        QUrl(QStringLiteral("https://private.example")), QStringLiteral("mailto")));
}

void BrowserControllerTest::configuresOneDownloadDirectoryForEveryWindow()
{
    QTemporaryDir root;
    QTemporaryDir config;
    QTemporaryDir chosen;
    {
        BrowserController controller(
            SpaceStorage(root.path(), QStringLiteral("test")), config.path());
        QCOMPARE(controller.downloadDirectory(),
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        QSignalSpy spy(&controller, &BrowserController::downloadDirectoryChanged);
        QVERIFY(controller.setDownloadDirectory(chosen.path()));
        QCOMPARE(controller.downloadDirectory(), chosen.path());
        QCOMPARE(spy.count(), 1);
        QVERIFY(!controller.setDownloadDirectory(
            QDir(root.path()).filePath(QStringLiteral("nowhere"))));
        QVERIFY(!controller.setDownloadDirectory(QString()));
        QCOMPARE(controller.downloadDirectory(), chosen.path());
        QCOMPARE(spy.count(), 1);
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")), config.path());
    QCOMPARE(restored.downloadDirectory(), chosen.path());

    PrivateSessionFixture privateSession(config.path());
    auto privateWindow = privateSession.createController();
    QCOMPARE(privateWindow->downloadDirectory(), chosen.path());
    QTemporaryDir elsewhere;
    QVERIFY(!privateWindow->setDownloadDirectory(elsewhere.path()));
    QCOMPARE(privateWindow->downloadDirectory(), chosen.path());
}

void BrowserControllerTest::persistsInterfacePreferencesOutsidePrivateBrowsing()
{
    QTemporaryDir root;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        QCOMPARE(controller.preference(QStringLiteral("sidebar-width"), QStringLiteral("292")),
            QStringLiteral("292"));
        QVERIFY(controller.setPreference(QStringLiteral("sidebar-width"), QStringLiteral("360")));
        QVERIFY(controller.setPreference(QStringLiteral("sidebar-width"), QStringLiteral("412")));
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(restored.preference(QStringLiteral("sidebar-width"), QStringLiteral("292")),
        QStringLiteral("412"));

    // A Private window browses on the defaults and writes nothing back.
    PrivateSessionFixture privateSession;
    auto privateController = privateSession.createController();
    QCOMPARE(privateController->preference(QStringLiteral("sidebar-width"), QStringLiteral("292")),
        QStringLiteral("292"));
    QVERIFY(
        !privateController->setPreference(QStringLiteral("sidebar-width"), QStringLiteral("500")));
    QCOMPARE(restored.preference(QStringLiteral("sidebar-width"), QStringLiteral("292")),
        QStringLiteral("412"));
}

// One inspector inspects one tab. Asking for it on a second tab moves it rather
// than opening another, and a blank tab has no page for it to attach to.
void BrowserControllerTest::attachesOneInspectorToOneTab()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));

    controller.openDeveloperTools();
    QVERIFY2(controller.developerToolsTabId().isEmpty(), "a blank tab has no page to inspect");

    controller.openInput(QStringLiteral("https://first.example"), false);
    const auto firstTabId = controller.activeTabId();
    QSignalSpy attachmentSpy(&controller, &BrowserController::developerToolsChanged);
    controller.openDeveloperTools();
    QCOMPARE(controller.developerToolsTabId(), firstTabId);
    QVERIFY(controller.activeTabInspected());
    QCOMPARE(attachmentSpy.count(), 1);

    controller.openInput(QStringLiteral("https://second.example"), true);
    const auto secondTabId = controller.activeTabId();
    QVERIFY(secondTabId != firstTabId);
    // The inspector stays where it was until it is asked for here.
    QCOMPARE(controller.developerToolsTabId(), firstTabId);
    QVERIFY(!controller.activeTabInspected());

    controller.openDeveloperTools();
    QCOMPARE(controller.developerToolsTabId(), secondTabId);

    controller.toggleDeveloperTools();
    QVERIFY(controller.developerToolsTabId().isEmpty());
    QVERIFY(!controller.activeTabInspected());
}

// Developer tools follow the tab, not the window: selecting another Space hides
// them, and coming back to the inspected tab brings them with it.
void BrowserControllerTest::keepsTheInspectorThroughASpaceSwitch()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://inspected.example"), false);
    const auto inspectedTabId = controller.activeTabId();
    controller.openDeveloperTools();

    const auto otherSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(otherSpaceId));
    QCOMPARE(controller.developerToolsTabId(), inspectedTabId);
    QVERIFY(!controller.activeTabInspected());

    QVERIFY(controller.switchSpace(controller.spaces()
            ->data(controller.spaces()->index(0, 0), SpaceListModel::IdRole)
            .toString()));
    QCOMPARE(controller.activeTabId(), inspectedTabId);
    QVERIFY(controller.activeTabInspected());
}

// Closing the tab, emptying it, moving it to another Space, or deleting the
// Space it lives in all take the page the inspector was attached to away.
void BrowserControllerTest::detachesTheInspectorWithTheTabItInspects()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));

    controller.openInput(QStringLiteral("https://closed.example"), false);
    controller.openInput(QStringLiteral("https://kept.example"), true);
    controller.openDeveloperTools();
    controller.closeActiveTab();
    QVERIFY(controller.developerToolsTabId().isEmpty());

    // The last tab in a Space is emptied rather than removed, and an empty tab
    // has no page either.
    controller.openDeveloperTools();
    QVERIFY(!controller.developerToolsTabId().isEmpty());
    controller.closeActiveTab();
    QVERIFY(controller.developerToolsTabId().isEmpty());

    controller.openInput(QStringLiteral("https://navigated.example"), false);
    controller.openDeveloperTools();
    controller.reportTabPageState(controller.activeTabId(), QUrl(QStringLiteral("about:blank")),
        QStringLiteral("New tab"), {}, false, false);
    QVERIFY(controller.developerToolsTabId().isEmpty());

    controller.openInput(QStringLiteral("https://moved.example"), false);
    const auto movedTabId = controller.activeTabId();
    const auto destinationSpaceId = controller.createSpace(QStringLiteral("Work"));
    controller.openDeveloperTools();
    QVERIFY(controller.confirmTabMoveToSpace(movedTabId, destinationSpaceId));
    QVERIFY(controller.developerToolsTabId().isEmpty());

    // Deleting the Space an inspected tab lives in, while another Space is
    // active and that tab is not in the model to be noticed missing.
    QVERIFY(controller.switchSpace(destinationSpaceId));
    QCOMPARE(controller.activeTabId(), movedTabId);
    controller.openDeveloperTools();
    QCOMPARE(controller.developerToolsTabId(), movedTabId);
    const auto personalSpaceId
        = controller.spaces()
              ->data(controller.spaces()->index(0, 0), SpaceListModel::IdRole)
              .toString();
    QVERIFY(controller.switchSpace(personalSpaceId));
    QVERIFY(controller.deleteSpace(destinationSpaceId, QStringLiteral("Work")));
    QVERIFY(controller.developerToolsTabId().isEmpty());
}

void BrowserControllerTest::neverRestoresTheInspectorAfterRestart()
{
    SessionFixture fixture(SessionSpec {
        .spaces = {SpaceSpec {
            .id = QStringLiteral("personal"),
            .name = QStringLiteral("Personal"),
            .tabs = {TabSpec {
                .id = QStringLiteral("inspected"),
                .url = QUrl(QStringLiteral("https://inspected.example")),
            }},
        }},
    });
    QVERIFY_SESSION_READY(fixture);
    {
        const auto controller = fixture.createController();
        controller->openDeveloperTools();
        QCOMPARE(controller->developerToolsTabId(), QStringLiteral("inspected"));
    }

    const auto restarted = fixture.createController();
    QCOMPARE(restarted->activeTabId(), QStringLiteral("inspected"));
    QVERIFY(restarted->developerToolsTabId().isEmpty());
    QVERIFY(!restarted->activeTabInspected());
}

// Order inside a section is the reader's. A drag names a destination and a
// keypress names a step, and neither can carry a tab out of its own section.
void BrowserControllerTest::reordersTabsWithinTheirSection()
{
    QTemporaryDir root;
    QString firstPinId;
    QString secondPinId;
    QString lastTabId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        controller.openInput(QStringLiteral("https://pin-one.example"), false);
        controller.toggleActivePinned();
        firstPinId = controller.activeTabId();
        controller.openInput(QStringLiteral("https://pin-two.example"), true);
        controller.toggleActivePinned();
        secondPinId = controller.activeTabId();
        controller.openInput(QStringLiteral("https://one.example"), true);
        const auto firstTabId = controller.activeTabId();
        controller.openInput(QStringLiteral("https://two.example"), true);
        lastTabId = controller.activeTabId();

        auto *tabs = controller.tabs();
        const auto idAt = [tabs](int row) {
            return tabs->data(tabs->index(row, 0), TabListModel::IdRole).toString();
        };
        // Pins lead the model, so the sections are the first two rows and the
        // two ordinary rows after them.
        QCOMPARE(tabs->rowCount(), 4);
        QCOMPARE(idAt(0), firstPinId);
        QCOMPARE(idAt(1), secondPinId);

        // Section-relative: the second pin asked for the first place lands in
        // row 0, not somewhere among the ordinary tabs.
        QVERIFY(controller.moveTab(secondPinId, 0));
        QCOMPARE(idAt(0), secondPinId);
        QCOMPARE(idAt(1), firstPinId);

        // A pin cannot be asked for a place the Pinned section does not have.
        QVERIFY(!controller.moveTab(secondPinId, 2));
        QCOMPARE(idAt(0), secondPinId);

        // The keyboard steps, and stops at the section edge rather than
        // spilling into the pins above.
        QCOMPARE(controller.tabSectionIndex(lastTabId), 1);
        QVERIFY(controller.moveTabBy(lastTabId, -1));
        QCOMPARE(controller.tabSectionIndex(lastTabId), 0);
        QCOMPARE(idAt(2), lastTabId);
        QVERIFY(!controller.moveTabBy(lastTabId, -1));
        QCOMPARE(idAt(2), lastTabId);
        QCOMPARE(controller.tabSectionIndex(firstTabId), 1);
    }

    // Arrangement is written through as it is made, not at the next quit.
    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    auto *tabs = restored.tabs();
    QCOMPARE(tabs->data(tabs->index(0, 0), TabListModel::IdRole).toString(), secondPinId);
    QCOMPARE(tabs->data(tabs->index(1, 0), TabListModel::IdRole).toString(), firstPinId);
    QCOMPARE(tabs->data(tabs->index(2, 0), TabListModel::IdRole).toString(), lastTabId);
}

// Duplicate opens the address again and copies nothing else about the tab: not
// the pin, not the zoom, not the muting, and — because it is a new tab with a
// new engine — neither history nor form state nor the running page.
void BrowserControllerTest::duplicatesOnlyTheAddress()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://source.example/page"), false);
    const auto sourceId = controller.activeTabId();
    controller.setTabZoom(sourceId, 1.5);
    controller.setTabMuted(sourceId, true);
    controller.toggleActivePinned();

    const auto duplicateId = controller.duplicateTab(sourceId);
    QVERIFY(!duplicateId.isEmpty());
    QVERIFY(duplicateId != sourceId);
    QCOMPARE(controller.activeTabId(), duplicateId);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://source.example/page")));
    QVERIFY(!controller.activeTabPinned());
    QCOMPARE(controller.activeTabZoom(), 1.0);

    auto *tabs = controller.tabs();
    const auto duplicateRow = tabs->index(tabs->rowCount() - 1, 0);
    QCOMPARE(tabs->data(duplicateRow, TabListModel::IdRole).toString(), duplicateId);
    QVERIFY(!tabs->data(duplicateRow, TabListModel::MutedRole).toBool());

    // A blank tab has no address to open again.
    controller.openInput(QStringLiteral("about:blank"), true);
    QVERIFY(controller.duplicateTab(controller.activeTabId()).isEmpty());
}

// Both sweeping closes mean the ordinary list. A pin is the Space's furniture
// and is never taken by a command aimed at the rows around it.
void BrowserControllerTest::sweepingClosesSpareEveryPinnedTab()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://pinned.example"), false);
    controller.toggleActivePinned();
    const auto pinnedId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://above.example"), true);
    controller.openInput(QStringLiteral("https://kept.example"), true);
    const auto keptId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://below-one.example"), true);
    controller.openInput(QStringLiteral("https://below-two.example"), true);

    // Below means below in the ordinary list; the row above it stays.
    controller.closeTabsBelow(keptId);
    auto *tabs = controller.tabs();
    QCOMPARE(tabs->rowCount(), 3);
    QCOMPARE(tabs->data(tabs->index(0, 0), TabListModel::IdRole).toString(), pinnedId);

    controller.closeOtherTabs(keptId);
    QCOMPARE(tabs->rowCount(), 2);
    QCOMPARE(tabs->data(tabs->index(0, 0), TabListModel::IdRole).toString(), pinnedId);
    QCOMPARE(tabs->data(tabs->index(1, 0), TabListModel::IdRole).toString(), keptId);

    // Asked about a pin, the command aimed at the ordinary rows below it does
    // nothing at all.
    controller.closeTabsBelow(pinnedId);
    QCOMPARE(tabs->rowCount(), 2);
}

// Each Space keeps its own recent closes across a restart, newest first, and
// gives back everything the session held about a tab.
void BrowserControllerTest::keepsRecentClosesPerSpaceAcrossRestart()
{
    SessionFixture fixture(SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = QStringLiteral("personal"),
                .name = QStringLiteral("Personal"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("personal-tab"),
                    .url = QUrl(QStringLiteral("https://one.example")),
                }},
                .recentCloses = {TabSpec {
                    .url = QUrl(QStringLiteral("https://two.example")),
                    .zoom = 1.25,
                    .muted = true,
                }},
            },
            SpaceSpec {
                .id = QStringLiteral("work"),
                .name = QStringLiteral("Work"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("work-resting"),
                }},
                .recentCloses = {TabSpec {
                    .url = QUrl(QStringLiteral("https://work.example")),
                }},
            },
        },
        .activeSpaceId = QStringLiteral("personal"),
    });
    QVERIFY_SESSION_READY(fixture);

    const auto restored = fixture.createController();
    QCOMPARE(restored->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(restored->closedTabCount(), 1);
    restored->reopenClosedTab();
    QCOMPARE(restored->activeUrl(), QUrl(QStringLiteral("https://two.example")));
    QCOMPARE(restored->activeTabZoom(), 1.25);
    auto *tabs = restored->tabs();
    const auto reopened = tabs->index(tabs->rowCount() - 1, 0);
    QVERIFY(tabs->data(reopened, TabListModel::MutedRole).toBool());
    QCOMPARE(restored->closedTabCount(), 0);

    QVERIFY(restored->switchSpace(QStringLiteral("work")));
    QCOMPARE(restored->closedTabCount(), 1);
    restored->reopenClosedTab();
    QCOMPARE(restored->activeUrl(), QUrl(QStringLiteral("https://work.example")));
}

// Twenty-five deep, in reverse closing order, and a pin comes back pinned.
void BrowserControllerTest::reopensClosedTabsNewestFirstAndBoundedAtTwentyFive()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://anchor.example"), false);

    for (int index = 0; index < 30; ++index) {
        controller.openInput(QStringLiteral("https://closed-%1.example").arg(index), true);
        controller.closeActiveTab();
    }
    QCOMPARE(controller.closedTabCount(), 25);

    // The oldest five fell off the far end; the newest is the first back.
    controller.reopenClosedTab();
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://closed-29.example")));
    controller.reopenClosedTab();
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://closed-28.example")));
    QCOMPARE(controller.closedTabCount(), 23);

    controller.openInput(QStringLiteral("https://pinned.example"), true);
    controller.toggleActivePinned();
    const auto pinnedId = controller.activeTabId();
    // A pin is closed by unpinning it first, the way the interface does; the
    // close itself refuses a pinned tab.
    controller.toggleActivePinned();
    controller.closeTab(pinnedId);
    controller.reopenClosedTab();
    QVERIFY(!controller.activeTabPinned());
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://pinned.example")));
}

// The last tab in a Space is emptied rather than removed, and keeps its id. So
// the same tab can be the one that was closed twice, and both closes have to be
// there to take back — a stack that refused the second would stop recording
// anything at all.
void BrowserControllerTest::remembersTheSameSpacesLastPageClosedTwice()
{
    QTemporaryDir root;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        controller.openInput(QStringLiteral("https://first.example"), false);
        controller.closeActiveTab();
        QCOMPARE(controller.tabs()->rowCount(), 1);
        controller.openInput(QStringLiteral("https://second.example"), false);
        controller.closeActiveTab();
        QCOMPARE(controller.closedTabCount(), 2);
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(restored.closedTabCount(), 2);
    restored.reopenClosedTab();
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://second.example")));
    restored.reopenClosedTab();
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://first.example")));
}

// A Private session remembers its closes for as long as it lasts and writes
// none of them down.
void BrowserControllerTest::keepsPrivateClosesInMemoryOnly()
{
    PrivateSessionFixture privateSession;
    auto controller = privateSession.createController();
    controller->openInput(QStringLiteral("https://private-one.example"), false);
    controller->openInput(QStringLiteral("https://private-two.example"), true);
    const auto secondId = controller->activeTabId();

    controller->closeTab(secondId);
    QCOMPARE(controller->closedTabCount(), 1);
    controller->reopenClosedTab();
    QCOMPARE(controller->activeUrl(), QUrl(QStringLiteral("https://private-two.example")));
    QCOMPARE(controller->closedTabCount(), 0);
}

// Muting is the reader's standing decision about a tab and comes back with it,
// while what a page is playing does not: a restored tab is silent until its
// page says otherwise. Neither is ever keyed by origin.
void BrowserControllerTest::restoresMutingWithTheTabAndNeverByOrigin()
{
    QTemporaryDir root;
    QString mutedId;
    QString sameOriginId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        controller.openInput(QStringLiteral("https://loud.example/one"), false);
        mutedId = controller.activeTabId();
        controller.setTabMuted(mutedId, true);
        controller.setTabAudible(mutedId, true);
        controller.openInput(QStringLiteral("https://loud.example/two"), true);
        sameOriginId = controller.activeTabId();
        auto *tabs = controller.tabs();
        // The same site in another tab is not muted by the decision made here.
        QVERIFY(!tabs->data(tabs->index(1, 0), TabListModel::MutedRole).toBool());

        // Muting survives navigation within the tab.
        controller.reportTabPageState(mutedId, QUrl(QStringLiteral("https://elsewhere.example")),
            QStringLiteral("Elsewhere"), {}, false, true);
        QVERIFY(tabs->data(tabs->index(0, 0), TabListModel::MutedRole).toBool());
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    auto *tabs = restored.tabs();
    QVERIFY(tabs->data(tabs->index(0, 0), TabListModel::MutedRole).toBool());
    QVERIFY(!tabs->data(tabs->index(0, 0), TabListModel::AudibleRole).toBool());
    QVERIFY(!tabs->data(tabs->index(1, 0), TabListModel::MutedRole).toBool());
}

// Keep active belongs to a Pinned tab, never to an ordinary one, and a pin
// never implies it. Unpinning gives it up.
void BrowserControllerTest::allowsKeepActiveOnlyOnPinnedTabs()
{
    QTemporaryDir root;
    QString pinnedId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        controller.openInput(QStringLiteral("https://kept.example"), false);
        pinnedId = controller.activeTabId();

        QVERIFY(!controller.setTabKeepActive(pinnedId, true));
        controller.toggleActivePinned();
        QVERIFY(!controller.activeTabKeepActive());
        QVERIFY(controller.setTabKeepActive(pinnedId, true));
        QVERIFY(controller.activeTabKeepActive());

        controller.toggleActivePinned();
        QVERIFY(!controller.activeTabKeepActive());
        controller.toggleActivePinned();
        QVERIFY(!controller.activeTabKeepActive());
        QVERIFY(controller.toggleActiveKeepActive());
        QVERIFY(controller.activeTabKeepActive());
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(restored.activeTabId(), pinnedId);
    QVERIFY(restored.activeTabKeepActive());

    // A Private window has no Pinned section, so it has nothing to keep active.
    PrivateSessionFixture privateSession;
    auto privateController = privateSession.createController();
    privateController->openInput(QStringLiteral("https://kept.example"), false);
    QVERIFY(!privateController->setTabKeepActive(privateController->activeTabId(), true));
}

// Suspension has exactly two exceptions: a Pinned tab marked Keep active, and
// the tab an inspector is attached to. Both are named when their Space is put
// away and both are identified while it is gone; nothing else survives.
void BrowserControllerTest::namesEverySuspensionExceptionAndNothingElse()
{
    const auto personalSpaceId = QStringLiteral("personal");
    const auto workSpaceId = QStringLiteral("work");
    const auto keptId = QStringLiteral("kept");
    const auto suspendedPinId = QStringLiteral("suspended-pin");
    const auto inspectedId = QStringLiteral("inspected");
    SessionFixture fixture(SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = personalSpaceId,
                .name = QStringLiteral("Personal"),
                .tabs = {
                    TabSpec {
                        .id = keptId,
                        .url = QUrl(QStringLiteral("https://kept.example")),
                        .pinned = true,
                        .keepActive = true,
                    },
                    TabSpec {
                        .id = suspendedPinId,
                        .url = QUrl(QStringLiteral("https://suspended.example")),
                        .pinned = true,
                    },
                    TabSpec {
                        .id = inspectedId,
                        .url = QUrl(QStringLiteral("https://ordinary.example")),
                    },
                },
                .activeTabId = inspectedId,
            },
            SpaceSpec {
                .id = workSpaceId,
                .name = QStringLiteral("Work"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("work-resting"),
                }},
            },
        },
        .activeSpaceId = personalSpaceId,
    });
    QVERIFY_SESSION_READY(fixture);
    const auto ownedController = fixture.createController();
    auto &controller = *ownedController;
    controller.openDeveloperTools();

    QCOMPARE(controller.retainedTabIds(), QStringList({keptId, inspectedId}));
    // Nothing is being retained while the Space holding these tabs is the one
    // on show: its pages are live because the reader is looking at them.
    QVERIFY(controller.retainedTabs().isEmpty());

    QSignalSpy suspendedSpy(&controller, &BrowserController::spaceSuspended);
    QVERIFY(controller.switchSpace(workSpaceId));
    QCOMPARE(suspendedSpy.count(), 1);
    QCOMPARE(suspendedSpy.first().at(0).toString(), personalSpaceId);
    QCOMPARE(suspendedSpy.first().at(1).toStringList(), QStringList({keptId, inspectedId}));

    QCOMPARE(controller.retainedTabs().size(), 2);
    QSet<QString> retainedIds;
    for (const auto &entry : controller.retainedTabs()) {
        const auto retained = entry.toMap();
        retainedIds.insert(retained.value(QStringLiteral("tabId")).toString());
        QCOMPARE(retained.value(QStringLiteral("spaceId")).toString(), personalSpaceId);
        QCOMPARE(
            retained.value(QStringLiteral("spaceName")).toString(), QStringLiteral("Personal"));
    }
    QVERIFY(retainedIds.contains(keptId));
    QVERIFY(retainedIds.contains(inspectedId));
    QVERIFY(!retainedIds.contains(suspendedPinId));

    // Detaching the inspector puts its tab back under ordinary suspension.
    controller.closeDeveloperTools();
    QCOMPARE(controller.retainedTabs().size(), 1);
    QCOMPARE(controller.retainedTabs().first().toMap().value(QStringLiteral("tabId")).toString(),
        keptId);
}

// A Pinned tab marked Keep active is running before its Space has ever been
// selected, which is what a restart — or a crash the session outlived — has to
// bring back.
void BrowserControllerTest::restoresRetainedTabsOfUnvisitedSpacesAfterRestart()
{
    SessionFixture fixture(SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = QStringLiteral("personal"),
                .name = QStringLiteral("Personal"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("personal-resting"),
                }},
            },
            SpaceSpec {
                .id = QStringLiteral("work"),
                .name = QStringLiteral("Work"),
                .tabs = {
                    TabSpec {
                        .id = QStringLiteral("kept"),
                        .url = QUrl(QStringLiteral("https://kept.example")),
                        .pinned = true,
                        .keepActive = true,
                    },
                    TabSpec {
                        .id = QStringLiteral("work-resting"),
                    },
                },
                .activeTabId = QStringLiteral("kept"),
            },
        },
        .activeSpaceId = QStringLiteral("personal"),
    });
    QVERIFY_SESSION_READY(fixture);

    // Nothing here has selected the Work Space, and the session still knows
    // one of its tabs is meant to be running.
    const auto restored = fixture.createController();
    QCOMPARE(restored->retainedTabs().size(), 1);
    const auto retained = restored->retainedTabs().first().toMap();
    QCOMPARE(retained.value(QStringLiteral("tabId")).toString(), QStringLiteral("kept"));
    QCOMPARE(retained.value(QStringLiteral("spaceId")).toString(), QStringLiteral("work"));
    QCOMPARE(retained.value(QStringLiteral("url")).toUrl(),
        QUrl(QStringLiteral("https://kept.example")));

    // Selecting that Space stops retaining it: the reader is looking at it.
    QVERIFY(restored->switchSpace(QStringLiteral("work")));
    QVERIFY(restored->retainedTabs().isEmpty());
}

// A crash is an exit with no chance to write anything down. Everything the
// reader arranges about a tab — its place, its pin, Keep active, its zoom, its
// muting, and the closes it can still take back — is written as they do it, so
// a session opened over a store whose last writer never closed it finds all of
// it. The first session is deliberately still open here: nothing it did may
// depend on its destructor.
void BrowserControllerTest::restoresTheSessionAfterAnUncleanExit()
{
    QTemporaryDir root;
    BrowserController crashed(SpaceStorage(root.path(), QStringLiteral("test")));
    crashed.openInput(QStringLiteral("https://kept.example"), false);
    const auto keptId = crashed.activeTabId();
    crashed.toggleActivePinned();
    QVERIFY(crashed.setTabKeepActive(keptId, true));
    crashed.openInput(QStringLiteral("https://first.example"), true);
    const auto firstId = crashed.activeTabId();
    crashed.setTabZoom(firstId, 1.25);
    crashed.setTabMuted(firstId, true);
    crashed.openInput(QStringLiteral("https://second.example"), true);
    const auto secondId = crashed.activeTabId();
    QVERIFY(crashed.moveTab(secondId, 0));
    crashed.openInput(QStringLiteral("https://closed.example"), true);
    crashed.closeActiveTab();

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    auto *tabs = restored.tabs();
    QCOMPARE(tabs->rowCount(), 3);
    const auto idAt = [tabs](int row) {
        return tabs->data(tabs->index(row, 0), TabListModel::IdRole).toString();
    };
    QCOMPARE(idAt(0), keptId);
    QVERIFY(tabs->data(tabs->index(0, 0), TabListModel::KeepActiveRole).toBool());
    // The arrangement the reader made, not the order the tabs were opened in.
    QCOMPARE(idAt(1), secondId);
    QCOMPARE(idAt(2), firstId);
    QCOMPARE(tabs->data(tabs->index(2, 0), TabListModel::ZoomRole).toDouble(), 1.25);
    QVERIFY(tabs->data(tabs->index(2, 0), TabListModel::MutedRole).toBool());

    QCOMPARE(restored.closedTabCount(), 1);
    restored.reopenClosedTab();
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://closed.example")));
}

// Deleting the Space on show hands the window to another one, and that Space's
// own Keep active tabs stop being something held for it: the reader is looking
// at them.
void BrowserControllerTest::stopsRetainingTheSpaceThatReplacesADeletedOne()
{
    const auto personalSpaceId = QStringLiteral("personal");
    const auto workSpaceId = QStringLiteral("work");
    SessionFixture fixture(SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = personalSpaceId,
                .name = QStringLiteral("Personal"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("kept"),
                    .url = QUrl(QStringLiteral("https://kept.example")),
                    .pinned = true,
                    .keepActive = true,
                }},
            },
            SpaceSpec {
                .id = workSpaceId,
                .name = QStringLiteral("Work"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("work-resting"),
                }},
            },
        },
        .activeSpaceId = workSpaceId,
    });
    QVERIFY_SESSION_READY(fixture);
    const auto ownedController = fixture.createController();
    auto &controller = *ownedController;
    QCOMPARE(controller.retainedTabs().size(), 1);

    QVERIFY(controller.deleteSpace(workSpaceId, QStringLiteral("Work")));
    QCOMPARE(controller.activeSpaceId(), personalSpaceId);
    QVERIFY(controller.retainedTabs().isEmpty());
}

// A notification names the origin and the Space, and activating it goes to the
// tab that sent it — changing Space on the way when it has to.
void BrowserControllerTest::routesNotificationsToTheOriginatingTab()
{
    const auto personalSpaceId = QStringLiteral("personal");
    const auto workSpaceId = QStringLiteral("work");
    const auto chatId = QStringLiteral("chat");
    SessionFixture fixture(SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = personalSpaceId,
                .name = QStringLiteral("Personal"),
                .tabs = {
                    TabSpec {
                        .id = chatId,
                        .url = QUrl(QStringLiteral("https://chat.example/room")),
                        .pinned = true,
                        .keepActive = true,
                    },
                    TabSpec {
                        .id = QStringLiteral("quiet"),
                        .url = QUrl(QStringLiteral("https://quiet.example")),
                    },
                },
                .activeTabId = QStringLiteral("quiet"),
            },
            SpaceSpec {
                .id = workSpaceId,
                .name = QStringLiteral("Work"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("work-resting"),
                }},
            },
        },
        .activeSpaceId = personalSpaceId,
    });
    QVERIFY_SESSION_READY(fixture);
    const auto ownedController = fixture.createController();
    auto &controller = *ownedController;

    const auto target = controller.notificationTarget(
        personalSpaceId, QUrl(QStringLiteral("https://chat.example")));
    QCOMPARE(target.value(QStringLiteral("tabId")).toString(), chatId);
    QCOMPARE(target.value(QStringLiteral("spaceName")).toString(), QStringLiteral("Personal"));
    QCOMPARE(
        target.value(QStringLiteral("origin")).toString(), QStringLiteral("https://chat.example"));

    // A Space with no page at that origin has no tab to speak for it.
    QVERIFY(controller
            .notificationTarget(personalSpaceId, QUrl(QStringLiteral("https://elsewhere.example")))
            .isEmpty());

    QVERIFY(controller.switchSpace(workSpaceId));

    // The retained tab may still say something while its Space is away; the
    // suspended one beside it may not.
    QCOMPARE(
        controller.notificationTarget(personalSpaceId, QUrl(QStringLiteral("https://chat.example")))
            .value(QStringLiteral("tabId"))
            .toString(),
        chatId);
    QVERIFY(controller
            .notificationTarget(personalSpaceId, QUrl(QStringLiteral("https://quiet.example")))
            .isEmpty());
    QVERIFY(controller.activateNotificationTarget(personalSpaceId, chatId));
    QCOMPARE(controller.activeSpaceId(), personalSpaceId);
    QCOMPARE(controller.activeTabId(), chatId);
}

// Audible autoplay waits for the reader to have dealt with the origin. The
// memory is one Space's and one session's.
void BrowserControllerTest::remembersOriginInteractionWithinOneSpaceAndSession()
{
    QTemporaryDir root;
    const QUrl page(QStringLiteral("https://video.example/watch"));
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        controller.openInput(page.toString(), false);
        QVERIFY(!controller.originInteracted(page));

        controller.recordOriginInteraction(page);
        QVERIFY(controller.originInteracted(page));
        // The origin, not the page: another path on the same site is the same
        // origin, and another site is not.
        QVERIFY(controller.originInteracted(QUrl(QStringLiteral("https://video.example/other"))));
        QVERIFY(!controller.originInteracted(QUrl(QStringLiteral("https://other.example/"))));

        // Another Space is another browsing identity.
        const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workSpaceId));
        QVERIFY(!controller.originInteracted(page));
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY(!restored.originInteracted(page));
}

// A page may start playing on its own; the sound is what waits. Every tab on
// an origin the reader has not dealt with is held silent, and the whole origin
// is heard the moment they deal with it — from the page or from the row. It is
// never the reader's own muting, and never written down.
void BrowserControllerTest::holdsBackSoundUntilTheOriginIsDealtWith()
{
    QTemporaryDir root;
    QString firstId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        auto *tabs = controller.tabs();
        // A blank tab has no page to play anything.
        QVERIFY(!controller.tabSoundSuppressed(controller.activeTabId()));

        controller.openInput(QStringLiteral("https://loud.example/one"), false);
        firstId = controller.activeTabId();
        controller.openInput(QStringLiteral("https://loud.example/two"), true);
        const auto secondId = controller.activeTabId();
        controller.openInput(QStringLiteral("https://quiet.example/"), true);
        const auto otherOriginId = controller.activeTabId();

        QVERIFY(controller.tabSoundSuppressed(firstId));
        QVERIFY(controller.tabSoundSuppressed(secondId));
        QVERIFY(tabs->data(tabs->index(0, 0), TabListModel::SoundSuppressedRole).toBool());
        // Held back is not muted: the reader asked for neither.
        QVERIFY(!tabs->data(tabs->index(0, 0), TabListModel::MutedRole).toBool());

        // The row asking for one tab's sound is the reader dealing with the
        // origin, so the other tab on that site is heard as well.
        controller.grantTabSound(firstId);
        QVERIFY(!controller.tabSoundSuppressed(firstId));
        QVERIFY(!controller.tabSoundSuppressed(secondId));
        QVERIFY(controller.tabSoundSuppressed(otherOriginId));

        // Leaving for a site the reader has not dealt with holds the sound
        // again, in the same tab.
        controller.reportTabPageState(firstId, QUrl(QStringLiteral("https://elsewhere.example/")),
            QStringLiteral("Elsewhere"), {}, false, false);
        QVERIFY(controller.tabSoundSuppressed(firstId));
    }

    // Nothing about it is the session's to keep: a restored tab is held silent
    // until the reader deals with its origin again.
    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY(restored.tabSoundSuppressed(restored.activeTabId()));
    auto *tabs = restored.tabs();
    QVERIFY(!tabs->data(tabs->index(0, 0), TabListModel::MutedRole).toBool());
}

// Camera, microphone, geolocation and notifications take the three decisions.
// Clipboard read and screen sharing take approval each time: an allowance the
// reader cannot see being spent is not one they gave. Everything outside the
// contract is refused without asking, because a question whose only honest
// answer is no is not a question.
void BrowserControllerTest::remembersOnlyThePermissionsThatMayBeRemembered()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));

    for (const auto *rememberable :
        {"camera", "microphone", "camera-and-microphone", "geolocation", "notifications"}) {
        const auto permission = QString::fromLatin1(rememberable);
        const QUrl origin(QStringLiteral("https://%1.example").arg(permission));
        QCOMPARE(controller.permissionPolicy(permission), int(BrowserController::Rememberable));
        QVERIFY2(controller.setPermissionDecision(
                     origin, permission, BrowserController::AllowPersistently),
            rememberable);
        QCOMPARE(controller.permissionDecision(origin, permission),
            BrowserController::AllowPersistently);
        QVERIFY(controller.setPermissionDecision(origin, permission, BrowserController::Block));
        QCOMPARE(controller.permissionDecision(origin, permission), BrowserController::Block);
    }

    for (const auto *eachTime : {"clipboard-read", "screen-sharing"}) {
        const auto permission = QString::fromLatin1(eachTime);
        const QUrl origin(QStringLiteral("https://%1.example").arg(permission));
        QCOMPARE(controller.permissionPolicy(permission), int(BrowserController::AskedEachTime));
        // Answering once is answering this request, and the next one asks
        // again — whichever way the reader answered.
        QVERIFY2(controller.setPermissionDecision(origin, permission, BrowserController::AllowOnce),
            eachTime);
        QCOMPARE(controller.permissionDecision(origin, permission), BrowserController::Ask);
        QVERIFY(!controller.setPermissionDecision(
            origin, permission, BrowserController::AllowPersistently));
        QCOMPARE(controller.permissionDecision(origin, permission), BrowserController::Ask);
        QVERIFY(controller.setPermissionDecision(origin, permission, BrowserController::Block));
        QCOMPARE(controller.permissionDecision(origin, permission), BrowserController::Ask);
    }

    for (const auto *outside : {"usb", "bluetooth", "serial", "midi", "unsupported", ""}) {
        const auto permission = QString::fromLatin1(outside);
        const QUrl origin(QStringLiteral("https://outside.example"));
        QCOMPARE(controller.permissionPolicy(permission), int(BrowserController::Refused));
        QVERIFY2(!controller.setPermissionDecision(
                     origin, permission, BrowserController::AllowPersistently),
            outside);
        QCOMPARE(controller.permissionDecision(origin, permission), BrowserController::Block);
    }
}

// What Site information reads and what its reset action does. Both belong to
// one origin inside one Space, and neither reaches the Space beside it.
void BrowserControllerTest::listsAndResetsOneSitesPermissionsWithinItsSpace()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalSpaceId = controller.activeSpaceId();
    const QUrl site(QStringLiteral("https://site.example/page"));
    QVERIFY(controller.setPermissionDecision(
        site, QStringLiteral("camera"), BrowserController::AllowPersistently));
    QVERIFY(controller.setPermissionDecision(
        site, QStringLiteral("geolocation"), BrowserController::Block));
    QVERIFY(controller.setPermissionDecision(QUrl(QStringLiteral("https://other.example")),
        QStringLiteral("camera"), BrowserController::AllowPersistently));

    auto listed = controller.sitePermissions(site);
    QCOMPARE(listed.size(), 2);
    QVariantMap byPermission;
    for (const auto &entry : listed) {
        const auto row = entry.toMap();
        byPermission.insert(row.value(QStringLiteral("permission")).toString(),
            row.value(QStringLiteral("decision")));
    }
    QCOMPARE(byPermission.value(QStringLiteral("camera")).toInt(),
        int(BrowserController::AllowPersistently));
    QCOMPARE(
        byPermission.value(QStringLiteral("geolocation")).toInt(), int(BrowserController::Block));

    // A Space of its own knows nothing about the decisions made in the other.
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    QVERIFY(controller.sitePermissions(site).isEmpty());
    QVERIFY(controller.switchSpace(personalSpaceId));

    // An answer the session is holding is listed too — it is a decision the
    // site holds, and a Private window has no other kind.
    QVERIFY(controller.setPermissionDecision(
        site, QStringLiteral("notifications"), BrowserController::AllowOnce));
    QCOMPARE(controller.sitePermissions(site).size(), 3);
    // Listing it does not spend it: the site has not asked to use it.
    QCOMPARE(controller.permissionDecision(site, QStringLiteral("notifications")),
        BrowserController::AllowOnce);

    QVERIFY(controller.resetSitePermissions(site));
    QVERIFY(controller.sitePermissions(site).isEmpty());
    QCOMPARE(controller.permissionDecision(site, QStringLiteral("camera")), BrowserController::Ask);
    // The reset was this site's. The site beside it keeps what it was given.
    QCOMPARE(controller.permissionDecision(
                 QUrl(QStringLiteral("https://other.example")), QStringLiteral("camera")),
        BrowserController::AllowPersistently);
}

// A certificate failure blocks. The one exception Omaweb will even offer is a
// Local-development site's own main frame, and only where the engine says the
// failure can be overridden at all.
void BrowserControllerTest::refusesEveryCertificateExceptionButAnOverridableLocalMainFrame()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));

    for (const auto *local :
        {"https://localhost:8443/app", "https://app.localhost/", "https://api.test/v1",
            "https://127.0.0.1:3000/", "https://[::1]:8080/", "https://192.168.1.20:8443/"}) {
        const QUrl url(QString::fromLatin1(local));
        QVERIFY2(controller.localDevelopmentSite(url), local);
        QVERIFY2(controller.mayOfferCertificateException(url, true, true, false), local);
    }

    // A public site is refused however the failure is described.
    const QUrl publicSite(QStringLiteral("https://bank.example/login"));
    QVERIFY(!controller.localDevelopmentSite(publicSite));
    QVERIFY(!controller.mayOfferCertificateException(publicSite, true, true, false));

    const QUrl local(QStringLiteral("https://localhost:8443/app"));
    // A subresource: the reader is looking at a page, not at the request that
    // failed, so there is nothing they could be shown to decide about.
    QVERIFY(!controller.mayOfferCertificateException(local, true, false, false));
    // Not overridable, and fatal, are the engine's own two refusals.
    QVERIFY(!controller.mayOfferCertificateException(local, false, true, false));
    QVERIFY(!controller.mayOfferCertificateException(local, true, true, true));
    // Nothing to decide about an address that names no site.
    QVERIFY(!controller.mayOfferCertificateException(QUrl(), true, true, false));
    QVERIFY(!controller.mayOfferCertificateException(
        QUrl(QStringLiteral("http://localhost:8443/")), true, true, false));
}

// One exception, for the load in front of the reader. Nothing writes it down,
// so the next failure asks again — in this session and in the next.
void BrowserControllerTest::keepsCertificateExceptionsOutOfEveryStoreAndSession()
{
    QTemporaryDir root;
    const QUrl local(QStringLiteral("https://localhost:8443/app"));
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        QVERIFY(controller.mayOfferCertificateException(local, true, true, false));
        // Answering it is answering one request. There is no decision to store
        // and no permission the answer becomes.
        QVERIFY(!controller.setPermissionDecision(
            local, QStringLiteral("certificate-exception"), BrowserController::AllowPersistently));
        QCOMPARE(controller.permissionPolicy(QStringLiteral("certificate-exception")),
            int(BrowserController::Refused));
        QVERIFY(controller.sitePermissions(local).isEmpty());
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY(restored.sitePermissions(local).isEmpty());
    QCOMPARE(restored.permissionDecision(local, QStringLiteral("certificate-exception")),
        BrowserController::Block);
    // Offering it again is the point: an exception the browser wrote down would
    // be a certificate check the reader stopped being asked about.
    QVERIFY(restored.mayOfferCertificateException(local, true, true, false));
    QVERIFY(restored.certificateExceptionOrigins().isEmpty());
    QVERIFY(!restored.certificateExceptionInEffect(local));
}

// Engines keep an accepted certificate for as long as their profile lives and
// offer no way to take it back, so the grant is recorded here to keep the
// address trigger saying the check was waived. The record is one Space's, is
// never written down, and goes with the session.
void BrowserControllerTest::keepsAGrantedCertificateExceptionVisibleForItsSession()
{
    QTemporaryDir root;
    const QUrl local(QStringLiteral("https://localhost:8443/app"));
    QString personalSpaceId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        personalSpaceId = controller.activeSpaceId();
        QSignalSpy changed(&controller, &BrowserController::certificateExceptionsChanged);
        QVERIFY(controller.recordCertificateException(local));
        QCOMPARE(changed.count(), 1);
        QVERIFY(controller.certificateExceptionInEffect(local));
        // The origin's, so every tab of the Space showing that origin says so.
        QVERIFY(controller.certificateExceptionInEffect(
            QUrl(QStringLiteral("https://localhost:8443/other"))));
        QVERIFY(!controller.certificateExceptionInEffect(
            QUrl(QStringLiteral("https://localhost:9443/app"))));
        QCOMPARE(controller.certificateExceptionOrigins(),
            QStringList {QStringLiteral("https://localhost:8443")});

        // The Space beside it shares no engine profile, so it shares no waived
        // check either.
        const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workSpaceId));
        QVERIFY(!controller.certificateExceptionInEffect(local));
        QVERIFY(controller.certificateExceptionOrigins().isEmpty());

        // Nothing about an address that names no site is recorded.
        QVERIFY(!controller.recordCertificateException(QUrl()));
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY(restored.switchSpace(personalSpaceId));
    QVERIFY(!restored.certificateExceptionInEffect(local));
    QVERIFY(restored.certificateExceptionOrigins().isEmpty());

    // Private windows share one session, and its waived checks go with it.
    WindowManager manager;
    auto *first = manager.createPrivateWindow();
    auto *second = manager.createPrivateWindow();
    QVERIFY(first && second);
    QVERIFY(first->recordCertificateException(local));
    QVERIFY(second->certificateExceptionInEffect(local));
    manager.releasePrivateWindow(first);
    manager.releasePrivateWindow(second);
    QTRY_VERIFY(manager.privateProfilePath().isEmpty());
    auto *reopened = manager.createPrivateWindow();
    QVERIFY(reopened);
    QVERIFY(!reopened->certificateExceptionInEffect(local));
}

// Third-party cookies are blocked. A sign-in or a payment can be given one
// origin's worth of allowance: temporary, listed where the reader can see it,
// and taken back on demand.
void BrowserControllerTest::blocksThirdPartyCookiesUntilAFlowIsGivenAVisibleAllowance()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto spaceId = controller.activeSpaceId();
    const QUrl identity(QStringLiteral("https://login.example/callback"));

    QVERIFY(!controller.thirdPartyCookiesAllowed(spaceId, identity));
    QVERIFY(controller.thirdPartyCookieAllowances().isEmpty());

    // Only a flow that has a name Omaweb can show the reader.
    QVERIFY(!controller.allowThirdPartyCookies(identity, QStringLiteral("advertising")));
    QVERIFY(!controller.thirdPartyCookiesAllowed(spaceId, identity));

    QSignalSpy changed(&controller, &BrowserController::thirdPartyCookieAllowancesChanged);
    QVERIFY(controller.allowThirdPartyCookies(identity, QStringLiteral("authentication")));
    QCOMPARE(changed.count(), 1);
    QVERIFY(controller.thirdPartyCookiesAllowed(spaceId, identity));
    // The allowance is the origin's, not the address's.
    QVERIFY(controller.thirdPartyCookiesAllowed(
        spaceId, QUrl(QStringLiteral("https://login.example/token"))));
    QVERIFY(!controller.thirdPartyCookiesAllowed(
        spaceId, QUrl(QStringLiteral("https://tracker.example/"))));

    const auto listed = controller.thirdPartyCookieAllowances();
    QCOMPARE(listed.size(), 1);
    QCOMPARE(listed.first().toMap().value(QStringLiteral("origin")).toString(),
        QStringLiteral("https://login.example"));
    QCOMPARE(listed.first().toMap().value(QStringLiteral("purpose")).toString(),
        QStringLiteral("authentication"));

    QVERIFY(controller.revokeThirdPartyCookieAllowance(identity));
    QCOMPARE(changed.count(), 2);
    QVERIFY(!controller.thirdPartyCookiesAllowed(spaceId, identity));
    QVERIFY(controller.thirdPartyCookieAllowances().isEmpty());
    QVERIFY(!controller.revokeThirdPartyCookieAllowance(identity));
}

void BrowserControllerTest::endsThirdPartyCookieAllowancesWithTheirSpaceAndPrivateSession()
{
    QTemporaryDir root;
    const QUrl identity(QStringLiteral("https://pay.example/"));
    QString personalSpaceId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        personalSpaceId = controller.activeSpaceId();
        QVERIFY(controller.allowThirdPartyCookies(identity, QStringLiteral("payment")));

        const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workSpaceId));
        QVERIFY(controller.thirdPartyCookieAllowances().isEmpty());
        QVERIFY(!controller.thirdPartyCookiesAllowed(workSpaceId, identity));
        // The Space it was granted in still has it, and is the only one that
        // does.
        QVERIFY(controller.thirdPartyCookiesAllowed(personalSpaceId, identity));
    }

    // Nothing wrote it down, so the next session starts with none.
    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY(restored.switchSpace(personalSpaceId));
    QVERIFY(restored.thirdPartyCookieAllowances().isEmpty());
    QVERIFY(!restored.thirdPartyCookiesAllowed(personalSpaceId, identity));

    // Private windows share one temporary session, and it goes when the last
    // of them does.
    WindowManager manager;
    auto *first = manager.createPrivateWindow();
    auto *second = manager.createPrivateWindow();
    QVERIFY(first && second);
    QVERIFY(first->allowThirdPartyCookies(identity, QStringLiteral("authentication")));
    QCOMPARE(second->thirdPartyCookieAllowances().size(), 1);
    QVERIFY(second->thirdPartyCookiesAllowed(second->activeSpaceId(), identity));
    manager.releasePrivateWindow(first);
    manager.releasePrivateWindow(second);
    // The session's temporary state is dropped once the close has settled, so
    // the next private window starts from nothing.
    QTRY_VERIFY(manager.privateProfilePath().isEmpty());
    auto *reopened = manager.createPrivateWindow();
    QVERIFY(reopened);
    QVERIFY(reopened->thirdPartyCookieAllowances().isEmpty());
}

// What Site information shows for stored data. It is the Space's own, it is
// measured where the engine says it keeps it, and it counts only what the
// clearing action can actually take — a number the action cannot move would be
// worse than none.
void BrowserControllerTest::measuresTheSiteDataHeldForOneSpace()
{
    const QStringList named {
        QStringLiteral("Cookies"), QStringLiteral("Local Storage"), QStringLiteral("cache")};
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto spaceId = controller.activeSpaceId();
    const auto profilePath = controller.prepareProfileForSpace(spaceId);
    QVERIFY(!profilePath.isEmpty());

    // A Space that has kept none of it measures nothing, which is a
    // measurement rather than an unknown.
    QCOMPARE(controller.siteDataBytes(spaceId, named), 0);

    const QDir profile(profilePath);
    QVERIFY(QDir().mkpath(profile.filePath(QStringLiteral("Local Storage/leveldb"))));
    QFile stored(profile.filePath(QStringLiteral("Local Storage/leveldb/000003.log")));
    QVERIFY(stored.open(QIODevice::WriteOnly));
    QCOMPARE(stored.write(QByteArray(3000, 'x')), 3000);
    stored.close();
    // A named file counts as much as a named directory: Chromium keeps cookies
    // in one file and storage in a tree.
    QFile cookies(profile.filePath(QStringLiteral("Cookies")));
    QVERIFY(cookies.open(QIODevice::WriteOnly));
    QCOMPARE(cookies.write(QByteArray(1096, 'c')), 1096);
    cookies.close();
    QCOMPARE(controller.siteDataBytes(spaceId, named), 4096);

    // Everything else in the profile is not site data the action can clear, so
    // it is not counted. Reporting it would be a number nothing could move.
    QFile elsewhere(profile.filePath(QStringLiteral("Local State")));
    QVERIFY(elsewhere.open(QIODevice::WriteOnly));
    QCOMPARE(elsewhere.write(QByteArray(9999, 'z')), 9999);
    elsewhere.close();
    QCOMPARE(controller.siteDataBytes(spaceId, named), 4096);

    // Another Space's data is not this one's, however much of it there is.
    // This one's engine has never run, so it holds none, which is a
    // measurement and not an unknown — and taking it creates nothing.
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QCOMPARE(controller.siteDataBytes(workSpaceId, named), 0);
    QVERIFY(!QFileInfo::exists(controller.profilePathForSpace(workSpaceId)));
    // An engine that names nothing is an engine with nothing to measure, and
    // so is a Space with nowhere on disk to look.
    QCOMPARE(controller.siteDataBytes(spaceId, {}), -1);
    QCOMPARE(controller.siteDataBytes(QString(), named), -1);
}

namespace {

QString tabIdAt(BrowserController &controller, int row)
{
    return controller.tabs()
        ->data(controller.tabs()->index(row, 0), TabListModel::IdRole)
        .toString();
}

bool tabBesideAt(BrowserController &controller, int row)
{
    return controller.tabs()
        ->data(controller.tabs()->index(row, 0), TabListModel::TabBesideRole)
        .toBool();
}

QString splitPartnerAt(BrowserController &controller, int row)
{
    return controller.tabs()
        ->data(controller.tabs()->index(row, 0), TabListModel::SplitPartnerIdRole)
        .toString();
}

} // namespace

// A split pairs the named tab with the active one. The active tab stays
// active and keeps its place; the partner leaves its own row and stands to
// the right of it, and the model says which of the two is the tab beside.
void BrowserControllerTest::pairsTwoOrdinaryTabsOfTheSpaceAsOneSplit()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://reference.example"), false);
    const auto referenceId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://work.example"), true);
    const auto workId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://other.example"), true);
    const auto otherId = controller.activeTabId();
    controller.activateTab(workId);
    QSignalSpy splitChanged(&controller, &BrowserController::splitChanged);

    QVERIFY(!controller.splitOnShow());
    QVERIFY(controller.addSplit(referenceId));
    QCOMPARE(splitChanged.count(), 1);
    QVERIFY(controller.splitOnShow());
    QCOMPARE(controller.activeTabId(), workId);
    QCOMPARE(controller.splitLeftTabId(), workId);
    QCOMPARE(controller.splitRightTabId(), referenceId);
    QCOMPARE(controller.tabBesideId(), referenceId);
    QVERIFY(controller.tabInSplit(workId));
    QVERIFY(controller.tabInSplit(referenceId));
    QVERIFY(!controller.tabInSplit(otherId));

    QCOMPARE(tabIdAt(controller, 0), workId);
    QCOMPARE(tabIdAt(controller, 1), referenceId);
    QCOMPARE(tabIdAt(controller, 2), otherId);
    QCOMPARE(splitPartnerAt(controller, 0), referenceId);
    QCOMPARE(splitPartnerAt(controller, 1), workId);
    QVERIFY(splitPartnerAt(controller, 2).isEmpty());
    QVERIFY(!tabBesideAt(controller, 0));
    QVERIFY(tabBesideAt(controller, 1));
    QVERIFY(!tabBesideAt(controller, 2));

    // The chooser lists what is left to pair: neither half, and never the
    // active tab.
    QCOMPARE(controller.splittableTabIds(), QStringList {otherId});

    // The tab beside is the page it shows and nothing else: the chrome's
    // answers stay the active tab's.
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://work.example")));
}

void BrowserControllerTest::pairsABlankTabBesideTheActiveOneWhenNoneIsNamed()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://work.example"), false);
    const auto workId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://later.example"), true);
    const auto laterId = controller.activeTabId();
    controller.activateTab(workId);

    QVERIFY(controller.addSplit());
    QCOMPARE(controller.tabs()->rowCount(), 3);
    const auto blankId = controller.activeTabId();
    QVERIFY(blankId != workId);
    QVERIFY(controller.activeTabBlank());
    QCOMPARE(controller.splitLeftTabId(), workId);
    QCOMPARE(controller.splitRightTabId(), blankId);
    QCOMPARE(controller.tabBesideId(), workId);
    QCOMPARE(tabIdAt(controller, 1), blankId);
    QCOMPARE(tabIdAt(controller, 2), laterId);
    // A Space with a split is not at rest, even with a blank half.
    QVERIFY(!controller.atRest());

    // The next address opened lands in the focused pane, which is the blank
    // one.
    controller.openInput(QStringLiteral("https://opened.example"), false);
    QCOMPARE(controller.activeTabId(), blankId);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://opened.example")));
    QCOMPARE(controller.tabBesideId(), workId);

    // Naming the active tab itself asks for the same thing.
    controller.activateTab(laterId);
    QVERIFY(controller.addSplit(laterId));
    QCOMPARE(controller.tabs()->rowCount(), 4);
    QCOMPARE(controller.tabBesideId(), laterId);
    QVERIFY(controller.activeTabBlank());
}

void BrowserControllerTest::refusesASplitOfPinnedPairedOrUnknownTabs()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://pinned.example"), false);
    const auto pinnedId = controller.activeTabId();
    controller.toggleActivePinned();
    controller.openInput(QStringLiteral("https://one.example"), true);
    const auto oneId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://two.example"), true);
    const auto twoId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://three.example"), true);
    const auto threeId = controller.activeTabId();

    // A Pinned tab is never paired, on either side.
    QVERIFY(!controller.addSplit(pinnedId));
    controller.activateTab(pinnedId);
    QVERIFY(!controller.addSplit(oneId));
    QVERIFY(!controller.addSplit());
    QVERIFY(!controller.splitOnShow());

    // Nor a tab that is not there.
    controller.activateTab(twoId);
    QVERIFY(!controller.addSplit(QStringLiteral("nowhere")));

    // Each tab is in at most one split: a paired tab cannot be paired again,
    // and neither can a tab be paired with the active one while that is
    // already paired.
    QVERIFY(controller.addSplit(oneId));
    QVERIFY(!controller.addSplit(threeId));
    controller.activateTab(threeId);
    QVERIFY(!controller.addSplit(oneId));
    QVERIFY(!controller.addSplit(twoId));
    QCOMPARE(controller.splittableTabIds(), QStringList {});
    QVERIFY(controller.tabInSplit(oneId));
    QVERIFY(controller.tabInSplit(twoId));
    QVERIFY(!controller.tabInSplit(threeId));
}

void BrowserControllerTest::movesFocusBetweenTheHalvesOfASplit()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://left.example"), false);
    const auto leftId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://right.example"), true);
    const auto rightId = controller.activeTabId();
    controller.activateTab(leftId);
    QVERIFY(controller.addSplit(rightId));
    QSignalSpy splitChanged(&controller, &BrowserController::splitChanged);
    QSignalSpy activeTabChanged(&controller, &BrowserController::activeTabChanged);

    // Focus moves to the tab beside and everything the active tab answers for
    // moves with it; the pair and its order stay.
    QVERIFY(controller.focusSplitPartner());
    QCOMPARE(controller.activeTabId(), rightId);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://right.example")));
    QCOMPARE(controller.tabBesideId(), leftId);
    QCOMPARE(controller.splitLeftTabId(), leftId);
    QCOMPARE(controller.splitRightTabId(), rightId);
    QCOMPARE(splitChanged.count(), 1);
    QCOMPARE(activeTabChanged.count(), 1);
    QVERIFY(tabBesideAt(controller, 0));
    QVERIFY(!tabBesideAt(controller, 1));

    // Activating a half is the same move.
    controller.activateTab(leftId);
    QCOMPARE(controller.activeTabId(), leftId);
    QCOMPARE(controller.tabBesideId(), rightId);
    QVERIFY(controller.focusSplitPartner());
    QCOMPARE(controller.activeTabId(), rightId);

    // With no split on show there is no partner to focus.
    controller.openInput(QStringLiteral("https://alone.example"), true);
    QVERIFY(!controller.focusSplitPartner());
}

void BrowserControllerTest::keepsTheSplitRowWhileAnotherTabIsOnShowAndStepsOverItAsOneStop()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://left.example"), false);
    const auto leftId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://right.example"), true);
    const auto rightId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://third.example"), true);
    const auto thirdId = controller.activeTabId();
    controller.activateTab(leftId);
    QVERIFY(controller.addSplit(rightId));
    QVERIFY(controller.focusSplitPartner());
    QCOMPARE(controller.activeTabId(), rightId);

    // Selecting a third tab shows it alone. The row stays: both tabs still
    // name each other, and neither is the tab beside.
    controller.activateTab(thirdId);
    QVERIFY(!controller.splitOnShow());
    QVERIFY(controller.splitLeftTabId().isEmpty());
    QVERIFY(controller.tabInSplit(leftId));
    QVERIFY(controller.tabInSplit(rightId));
    QVERIFY(!tabBesideAt(controller, 0));
    QVERIFY(!tabBesideAt(controller, 1));

    // Cycling treats the row as one stop and enters on the half the reader
    // was last in.
    controller.stepTab(1);
    QCOMPARE(controller.activeTabId(), rightId);
    QVERIFY(controller.splitOnShow());
    controller.stepTab(1);
    QCOMPARE(controller.activeTabId(), thirdId);
    controller.stepTab(-1);
    QCOMPARE(controller.activeTabId(), rightId);
    controller.stepTab(-1);
    QCOMPARE(controller.activeTabId(), thirdId);

    // Any way to another tab shows it alone: one opened, one reopened, one
    // duplicated.
    controller.activateTab(leftId);
    QSignalSpy splitChanged(&controller, &BrowserController::splitChanged);
    controller.openInput(QStringLiteral("https://opened.example"), true);
    QVERIFY(!controller.splitOnShow());
    QVERIFY(controller.splitLeftTabId().isEmpty());
    // Announced, since the interface caches the answer until it is.
    QCOMPARE(splitChanged.count(), 1);
    controller.activateTab(leftId);
    controller.duplicateTab(thirdId);
    QVERIFY(!controller.splitOnShow());
    controller.closeActiveTab();
    controller.activateTab(leftId);
    controller.reopenClosedTab();
    QVERIFY(!controller.splitOnShow());
    controller.closeActiveTab();
    controller.closeTab(controller.activeTabId());
    controller.activateTab(thirdId);

    // Activating either half brings the split back with that half active.
    controller.activateTab(leftId);
    QVERIFY(controller.splitOnShow());
    QCOMPARE(controller.tabBesideId(), rightId);
    controller.activateTab(thirdId);
    controller.stepTab(-1);
    QCOMPARE(controller.activeTabId(), leftId);
}

void BrowserControllerTest::separatesASplitIntoTwoAdjacentOrdinaryRows()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://first.example"), false);
    const auto firstId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://second.example"), true);
    const auto secondId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://third.example"), true);
    const auto thirdId = controller.activeTabId();
    QVERIFY(controller.addSplit(firstId));
    QCOMPARE(tabIdAt(controller, 0), secondId);
    QCOMPARE(tabIdAt(controller, 1), thirdId);
    QCOMPARE(tabIdAt(controller, 2), firstId);

    // Nothing to separate on a tab in no split.
    QVERIFY(!controller.separateSplit(secondId));

    QSignalSpy splitChanged(&controller, &BrowserController::splitChanged);
    QVERIFY(controller.separateSplit());
    QCOMPARE(splitChanged.count(), 1);
    QVERIFY(!controller.splitOnShow());
    QVERIFY(!controller.tabInSplit(thirdId));
    QVERIFY(!controller.tabInSplit(firstId));
    QCOMPARE(controller.activeTabId(), thirdId);
    QCOMPARE(tabIdAt(controller, 1), thirdId);
    QCOMPARE(tabIdAt(controller, 2), firstId);
    QCOMPARE(controller.tabs()->rowCount(), 3);
    QVERIFY(!controller.separateSplit());

    // Separating from the other half, and from away, does the same.
    QVERIFY(controller.addSplit(firstId));
    controller.activateTab(secondId);
    QVERIFY(controller.separateSplit(firstId));
    QVERIFY(!controller.tabInSplit(thirdId));
    QCOMPARE(controller.activeTabId(), secondId);
}

void BrowserControllerTest::endsASplitWhenEitherTabClosesOrLeavesTheSpace()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    controller.openInput(QStringLiteral("https://first.example"), false);
    const auto firstId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://second.example"), true);
    const auto secondId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://third.example"), true);
    const auto thirdId = controller.activeTabId();

    // Closing the tab beside leaves the active tab as an ordinary row, still
    // on show.
    controller.activateTab(firstId);
    QVERIFY(controller.addSplit(secondId));
    controller.closeTab(secondId);
    QVERIFY(!controller.splitOnShow());
    QVERIFY(!controller.tabInSplit(firstId));
    QCOMPARE(controller.activeTabId(), firstId);

    // Closing the active half shows the tab that was beside it.
    QVERIFY(controller.addSplit(thirdId));
    controller.closeActiveTab();
    QCOMPARE(controller.activeTabId(), thirdId);
    QVERIFY(!controller.tabInSplit(thirdId));
    QVERIFY(!controller.splitOnShow());
    QCOMPARE(controller.tabs()->rowCount(), 1);

    // Moving either tab to another Space ends the split. The tab that stays
    // is an ordinary row, and the pairing is written to neither Space.
    controller.openInput(QStringLiteral("https://fourth.example"), true);
    const auto fourthId = controller.activeTabId();
    QVERIFY(controller.addSplit(thirdId));
    QVERIFY(controller.confirmTabMoveToSpace(fourthId, workSpaceId));
    QCOMPARE(controller.activeTabId(), thirdId);
    QVERIFY(!controller.tabInSplit(thirdId));
    QVERIFY(!controller.splitOnShow());
    for (const auto &tab : controller.sessionStore()->loadTabs(workSpaceId)) {
        QVERIFY(tab.splitPartnerId.isEmpty());
        QVERIFY(!tab.splitFocused);
    }
    for (const auto &tab : controller.sessionStore()->loadTabs(controller.activeSpaceId())) {
        QVERIFY(tab.splitPartnerId.isEmpty());
    }

    // The sweeping closes are about the other rows, and the split is one row:
    // the tab beside is spared with the tab the command was asked on.
    controller.openInput(QStringLiteral("https://fifth.example"), true);
    const auto fifthId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://sixth.example"), true);
    controller.activateTab(thirdId);
    QVERIFY(controller.addSplit(fifthId));
    controller.closeTabsBelow(thirdId);
    QCOMPARE(controller.tabs()->rowCount(), 2);
    QVERIFY(controller.splitOnShow());
    controller.openInput(QStringLiteral("https://seventh.example"), true);
    controller.activateTab(fifthId);
    controller.closeOtherTabs(fifthId);
    QCOMPARE(controller.tabs()->rowCount(), 2);
    QVERIFY(controller.splitOnShow());
    QCOMPARE(controller.tabBesideId(), thirdId);
}

void BrowserControllerTest::keepsASplitTabFromBeingPinnedOrMoved()
{
    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.openInput(QStringLiteral("https://first.example"), false);
    const auto firstId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://second.example"), true);
    const auto secondId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://third.example"), true);
    const auto thirdId = controller.activeTabId();
    controller.activateTab(firstId);
    QVERIFY(controller.addSplit(secondId));

    controller.toggleActivePinned();
    QVERIFY(!controller.activeTabPinned());
    QVERIFY(controller.splitOnShow());
    QVERIFY(!controller.moveTab(firstId, 2));
    QVERIFY(!controller.moveTabBy(secondId, 1));
    QCOMPARE(tabIdAt(controller, 0), firstId);
    QCOMPARE(tabIdAt(controller, 1), secondId);

    // A copy of the left half lands after the pair rather than between it.
    const auto copyId = controller.duplicateTab(firstId);
    QCOMPARE(tabIdAt(controller, 2), copyId);
    QCOMPARE(tabIdAt(controller, 3), thirdId);
    QVERIFY(!controller.tabInSplit(copyId));

    // Separated, the tab is an ordinary one again and can be pinned.
    controller.activateTab(firstId);
    QVERIFY(controller.separateSplit());
    controller.toggleActivePinned();
    QVERIFY(controller.activeTabPinned());
}

void BrowserControllerTest::keepsASplitAcrossARestartAndASpaceSwitch()
{
    QTemporaryDir root;
    QString leftId;
    QString rightId;
    QString thirdId;
    QString workSpaceId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        workSpaceId = controller.createSpace(QStringLiteral("Work"));
        controller.openInput(QStringLiteral("https://left.example"), false);
        leftId = controller.activeTabId();
        controller.openInput(QStringLiteral("https://right.example"), true);
        rightId = controller.activeTabId();
        controller.openInput(QStringLiteral("https://third.example"), true);
        thirdId = controller.activeTabId();
        controller.activateTab(leftId);
        QVERIFY(controller.addSplit(rightId));
        QVERIFY(controller.focusSplitPartner());

        // Away and back finds the split as it was left, focused half and all.
        QVERIFY(controller.switchSpace(workSpaceId));
        QVERIFY(!controller.splitOnShow());
        QVERIFY(controller.switchSpace(controller.spaces()
                ->data(controller.spaces()->index(0, 0), SpaceListModel::IdRole)
                .toString()));
        QCOMPARE(controller.activeTabId(), rightId);
        QCOMPARE(controller.tabBesideId(), leftId);
        QCOMPARE(controller.splitLeftTabId(), leftId);

        // Left for another tab, the row remembers which half was last focused
        // through a Space switch too.
        controller.activateTab(thirdId);
        QVERIFY(controller.switchSpace(workSpaceId));
        QVERIFY(controller.switchSpace(controller.spaces()
                ->data(controller.spaces()->index(0, 0), SpaceListModel::IdRole)
                .toString()));
        QCOMPARE(controller.activeTabId(), thirdId);
        controller.stepTab(-1);
        QCOMPARE(controller.activeTabId(), rightId);
    }

    // A restart brings the split back with the Space's active tab as the
    // focused half.
    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(restored.activeTabId(), rightId);
    QVERIFY(restored.splitOnShow());
    QCOMPARE(restored.splitLeftTabId(), leftId);
    QCOMPARE(restored.splitRightTabId(), rightId);
    QCOMPARE(restored.tabBesideId(), leftId);
    QCOMPARE(tabIdAt(restored, 0), leftId);
    QCOMPARE(tabIdAt(restored, 1), rightId);
    QCOMPARE(tabIdAt(restored, 2), thirdId);
}

// A store hands back what it was given, and what it was given may be a
// record another version wrote or a half of a pair that was lost on the way.
// The tab model only ever holds a whole split, so what does not add up to one
// is read as ordinary tabs.
void BrowserControllerTest::repairsAPairingTheStoreHandsBackBroken()
{
    QTemporaryDir root;
    QString spaceId;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        spaceId = controller.activeSpaceId();
    }
    const auto makeTab
        = [&spaceId](const QString &id, const QString &partner, bool focused, bool pinned = false) {
              omaweb::TabState tab;
              tab.id = id;
              tab.spaceId = spaceId;
              tab.url = QUrl(QStringLiteral("https://%1.example").arg(id));
              tab.title = id;
              tab.pinned = pinned;
              tab.splitPartnerId = partner;
              tab.splitFocused = focused;
              return tab;
          };
    {
        omaweb::SqliteSessionStore store(root.path());
        QVERIFY(store.open());
        // "one" names a partner that names it back but is not beside it;
        // "lost" names a tab that is gone; "pin" is pinned and paired with
        // "five", which names it; "six" and "seven" both claim focus.
        QVERIFY(store.saveTabs(spaceId,
            {
                makeTab(QStringLiteral("one"), QStringLiteral("two"), false),
                makeTab(QStringLiteral("lost"), QStringLiteral("gone"), true),
                makeTab(QStringLiteral("two"), QStringLiteral("one"), false),
                makeTab(QStringLiteral("pin"), QStringLiteral("five"), true, true),
                makeTab(QStringLiteral("five"), QStringLiteral("pin"), false),
                makeTab(QStringLiteral("six"), QStringLiteral("seven"), true),
                makeTab(QStringLiteral("seven"), QStringLiteral("six"), true),
            },
            QStringLiteral("lost")));
    }

    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(controller.activeTabId(), QStringLiteral("lost"));
    QVERIFY(!controller.tabInSplit(QStringLiteral("lost")));
    QVERIFY(!controller.tabInSplit(QStringLiteral("pin")));
    QVERIFY(!controller.tabInSplit(QStringLiteral("five")));
    QVERIFY(controller.tabInSplit(QStringLiteral("one")));
    QVERIFY(controller.tabInSplit(QStringLiteral("two")));
    // The pins are listed first, then the pair stands together.
    QCOMPARE(tabIdAt(controller, 0), QStringLiteral("pin"));
    QCOMPARE(tabIdAt(controller, 1), QStringLiteral("one"));
    QCOMPARE(tabIdAt(controller, 2), QStringLiteral("two"));
    QCOMPARE(tabIdAt(controller, 3), QStringLiteral("lost"));
    controller.stepTab(-1);
    QCOMPARE(controller.activeTabId(), QStringLiteral("one"));
    // Both halves claiming focus reads as the left one.
    controller.activateTab(QStringLiteral("lost"));
    controller.stepTab(1);
    QCOMPARE(controller.activeTabId(), QStringLiteral("five"));
    controller.stepTab(1);
    QCOMPARE(controller.activeTabId(), QStringLiteral("six"));
    QCOMPARE(controller.tabBesideId(), QStringLiteral("seven"));
}

void BrowserControllerTest::splitsInAPrivateWindowAndWritesNothing()
{
    QTemporaryDir configRoot;
    PrivateSessionFixture fixture(configRoot.path());
    const auto controller = fixture.createController();
    controller->openInput(QStringLiteral("https://left.example"), false);
    const auto leftId = controller->activeTabId();
    controller->openInput(QStringLiteral("https://right.example"), true);
    const auto rightId = controller->activeTabId();
    controller->activateTab(leftId);

    QVERIFY(controller->addSplit(rightId));
    QVERIFY(controller->splitOnShow());
    QCOMPARE(controller->tabBesideId(), rightId);
    QVERIFY(controller->focusSplitPartner());
    QCOMPARE(controller->activeTabId(), rightId);
    QVERIFY(controller->separateSplit());
    QVERIFY(!controller->tabInSplit(leftId));
    QVERIFY(controller->sessionStore()->loadTabs({}).isEmpty());
    QVERIFY(entriesUnder(configRoot.path()).isEmpty());
}

QTEST_GUILESS_MAIN(BrowserControllerTest)

#include "tst_browsercontroller.moc"
