#include "BrowserController.h"
#include "SpaceListModel.h"
#include "TabListModel.h"
#include "WindowManager.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QDir>
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

using omaweb::BrowserController;
using omaweb::SpaceListModel;
using omaweb::TabListModel;
using omaweb::WindowManager;

class BrowserControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void createsPersonalSpaceAndBlankTab();
    void createsAndSwitchesSpaces();
    void renamesSpacePersistently();
    void requiresNameToDeletePopulatedSpace();
    void treatsEngineStateAsPopulatedSpaceData();
    void suspendsInactiveSpaceAndRestoresItsTabs();
    void warnsBeforeMovingEditedTabBetweenSpaces();
    void restoresEverySpaceAfterRestart();
    void migratesLegacyGlobalTabsWithoutLockingSchema();
    void separatesEngineStorageBySpaceAndEngine();
    void createsTabOnlyAfterCommittedInput();
    void opensKeyboardHintTargetsInBackground();
    void closesRequestedBackgroundTab();
    void persistsTabsAndPins();
    void pinningMovesTabIntoPinnedBlock();
    void keepsFinalTabAsBlankTab();
    void restsUntilSomethingIsOpenedInTheSpace();
    void keepsRendererFailureOnAffectedTab();
    void keepsMutingDecisionWhileSoundComesAndGoes();
    void stepsZoomAlongOneLadderPerTab();
    void restoresEveryTabsZoomAfterRestart();
    void sharesPrivateIdentityUntilLastWindowCloses();
    void keepsHistorySuggestionsInsideActiveSpace();
    void filtersAndDeletesHistoryAtRequestedBoundaries();
    void resolvesAddressesBeforeSearches();
    void migratesAndUsesSearchEngineConfiguration();
    void addsPredefinedSearchEngineProviders();
    void refusesPersistentBrowsingDataActionsInPrivateWindows();
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
    void persistsOnlyNonPrivateDownloadHistory();
    void asksBeforeWritingDownAProgram();
    void takesAPermissionForAutomaticAndMultipleDownloads();
    void sendsAConflictingNameToTheSaveDialog();
    void configuresOneDownloadDirectoryForEveryWindow();
    void forgetsOneDownloadWithoutForgettingTheRest();
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
};

void BrowserControllerTest::createsPersonalSpaceAndBlankTab()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));

    QVERIFY(controller.ready());
    QCOMPARE(controller.activeSpaceName(), QStringLiteral("Personal"));
    QCOMPARE(controller.spaces()->rowCount(), 1);
    QCOMPARE(controller.tabs()->rowCount(), 1);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("about:blank")));
}

void BrowserControllerTest::createsAndSwitchesSpaces()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
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

void BrowserControllerTest::opensKeyboardHintTargetsInBackground()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto activeTabId = controller.activeTabId();

    controller.openInputInBackground(QUrl(QStringLiteral("https://example.com/hint")));

    QCOMPARE(controller.tabs()->rowCount(), 2);
    QCOMPARE(controller.activeTabId(), activeTabId);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("about:blank")));
}

void BrowserControllerTest::closesRequestedBackgroundTab()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
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
        BrowserController controller(root.path(), QStringLiteral("test"));
        workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.renameSpace(workSpaceId, QStringLiteral("Research")));
        QVERIFY(controller.switchSpace(workSpaceId));
        QCOMPARE(controller.activeSpaceName(), QStringLiteral("Research"));
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.activeSpaceId(), workSpaceId);
    QCOMPARE(restored.activeSpaceName(), QStringLiteral("Research"));
}

void BrowserControllerTest::requiresNameToDeletePopulatedSpace()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto personalSpaceId = controller.activeSpaceId();
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    controller.openInput(QStringLiteral("https://work.example"), false);
    const auto workProfilePath = controller.activeProfilePath();
    QVERIFY(QFileInfo::exists(workProfilePath));
    QVERIFY(controller.switchSpace(personalSpaceId));

    QVERIFY(!controller.deleteSpace(workSpaceId, QString {}));
    QVERIFY(!controller.deleteSpace(workSpaceId, QStringLiteral("work")));
    QVERIFY(controller.deleteSpace(workSpaceId, QStringLiteral("Work")));
    QCOMPARE(controller.spaces()->rowCount(), 1);
    QVERIFY(!QFileInfo::exists(workProfilePath));

    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.spaces()->rowCount(), 1);
    QCOMPARE(restored.activeSpaceId(), personalSpaceId);
}

void BrowserControllerTest::treatsEngineStateAsPopulatedSpaceData()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("qt"));
    const auto personalSpaceId = controller.activeSpaceId();
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    QFile cookieState(controller.activeProfilePath() + QStringLiteral("/Cookies"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));
    controller.openInput(QStringLiteral("https://draft.example/form"), false);
    controller.toggleActivePinned();
    const auto movedTabId = controller.activeTabId();
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QSignalSpy confirmationSpy(&controller, &BrowserController::tabMoveConfirmationRequested);

    QVERIFY(controller.requestTabMoveToSpace(movedTabId, workSpaceId, true));
    QCOMPARE(confirmationSpy.count(), 1);
    QCOMPARE(controller.activeTabId(), movedTabId);

    QVERIFY(controller.confirmTabMoveToSpace(movedTabId, workSpaceId));
    QCOMPARE(controller.tabs()->rowCount(), 1);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("about:blank")));

    QVERIFY(controller.switchSpace(workSpaceId));
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://draft.example/form")));
    QVERIFY(controller.activeTabPinned());
}

void BrowserControllerTest::restoresEverySpaceAfterRestart()
{
    QTemporaryDir root;
    QString personalSpaceId;
    QString workSpaceId;
    {
        BrowserController controller(root.path(), QStringLiteral("qt"));
        personalSpaceId = controller.activeSpaceId();
        controller.openInput(QStringLiteral("https://personal.example/session"), false);
        workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workSpaceId));
        controller.openInput(QStringLiteral("https://work.example/session"), false);
    }

    BrowserController restored(root.path(), QStringLiteral("qt"));
    QCOMPARE(restored.activeSpaceId(), workSpaceId);
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://work.example/session")));
    QVERIFY(restored.switchSpace(personalSpaceId));
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://personal.example/session")));
}

void BrowserControllerTest::separatesEngineStorageBySpaceAndEngine()
{
    QTemporaryDir root;
    BrowserController qtController(root.path(), QStringLiteral("qt"));
    const auto personalQtPath = qtController.activeProfilePath();
    const auto workSpaceId = qtController.createSpace(QStringLiteral("Work"));
    QVERIFY(qtController.switchSpace(workSpaceId));
    const auto workQtPath = qtController.activeProfilePath();

    QVERIFY(personalQtPath != workQtPath);
    QVERIFY(personalQtPath.endsWith(QStringLiteral("/engines/qt")));
    QVERIFY(workQtPath.endsWith(QStringLiteral("/engines/qt")));

    BrowserController ladybirdController(root.path(), QStringLiteral("ladybird"));
    QCOMPARE(ladybirdController.activeSpaceId(), workSpaceId);
    QVERIFY(ladybirdController.activeProfilePath() != workQtPath);
    QVERIFY(ladybirdController.activeProfilePath().endsWith(QStringLiteral("/engines/ladybird")));
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

    BrowserController controller(root.path(), QStringLiteral("test"));

    QVERIFY2(controller.ready(), qPrintable(controller.errorMessage()));
    QCOMPARE(controller.tabs()->rowCount(), 1);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://example.com")));
}

void BrowserControllerTest::createsTabOnlyAfterCommittedInput()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));

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
        BrowserController controller(root.path(), QStringLiteral("test"));
        controller.openInput(QStringLiteral("https://example.com"), false);
        controller.toggleActivePinned();
        activeId = controller.activeTabId();
        QVERIFY(controller.activeTabPinned());
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.activeTabId(), activeId);
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://example.com")));
    QVERIFY(restored.activeTabPinned());
}

void BrowserControllerTest::pinningMovesTabIntoPinnedBlock()
{
    QTemporaryDir root;
    QString newlyPinnedId;
    {
        BrowserController controller(root.path(), QStringLiteral("test"));
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

    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.tabs()->data(restored.tabs()->index(1, 0), TabListModel::IdRole).toString(),
        newlyPinnedId);
}

void BrowserControllerTest::keepsFinalTabAsBlankTab()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));
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

    QSignalSpy reloadSpy(&controller, &BrowserController::reloadRequested);
    controller.recoverActiveTab();
    QVERIFY(!controller.activeRendererFailed());
    QCOMPARE(reloadSpy.count(), 1);
}

// Sound is the page's to report and muting is the reader's to decide, so the
// two are held apart: a page that falls silent leaves the tab muted, because
// the reader silenced the tab and not one clip in it.
void BrowserControllerTest::keepsMutingDecisionWhileSoundComesAndGoes()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));
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
    QTemporaryDir root;
    QString zoomedTabId;
    QString plainTabId;
    {
        BrowserController controller(root.path(), QStringLiteral("test"));
        controller.openInput(QStringLiteral("https://plain.example"), false);
        plainTabId = controller.activeTabId();
        controller.openInput(QStringLiteral("https://zoomed.example"), true);
        zoomedTabId = controller.activeTabId();
        controller.stepActiveZoom(1);
        controller.stepActiveZoom(1);
        QCOMPARE(controller.activeTabZoom(), 1.25);
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.activeTabId(), zoomedTabId);
    QCOMPARE(restored.activeTabZoom(), 1.25);
    restored.activateTab(plainTabId);
    QCOMPARE(restored.activeTabZoom(), 1.0);
}

void BrowserControllerTest::sharesPrivateIdentityUntilLastWindowCloses()
{
    WindowManager manager(QStringLiteral("test"));
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
    QVERIFY(first->activeProfilePath().isEmpty());
    QVERIFY(first->acceptDownloads());
    QCOMPARE(first->downloadDirectory(),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    QTemporaryDir elsewhere;
    QVERIFY(!first->setDownloadDirectory(elsewhere.path()));
    QVERIFY(first->downloadHistory().isEmpty());
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
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto personalSpaceId = controller.activeSpaceId();
    controller.recordVisit(QUrl(QStringLiteral("https://docs.example/personal")),
        QStringLiteral("Personal documentation"));
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    controller.recordVisit(
        QUrl(QStringLiteral("https://docs.example/work")), QStringLiteral("Work documentation"));

    const auto workSuggestions = controller.historySuggestions(QStringLiteral("docs"));
    QCOMPARE(workSuggestions.size(), 1);
    QCOMPARE(workSuggestions.first().toMap().value(QStringLiteral("url")).toUrl(),
        QUrl(QStringLiteral("https://docs.example/work")));

    QVERIFY(controller.switchSpace(personalSpaceId));
    const auto personalSuggestions = controller.historySuggestions(QStringLiteral("documentation"));
    QCOMPARE(personalSuggestions.size(), 1);
    QCOMPARE(personalSuggestions.first().toMap().value(QStringLiteral("url")).toUrl(),
        QUrl(QStringLiteral("https://docs.example/personal")));
}

void BrowserControllerTest::filtersAndDeletesHistoryAtRequestedBoundaries()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));

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
        root.filePath(QStringLiteral("data")), QStringLiteral("test"), configRoot);
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

    BrowserController privateController({}, QStringLiteral("test"), true,
        QSharedPointer<QHash<QString, int>>::create(), configRoot);
    privateController.openInput(QStringLiteral("private search"), false);
    QCOMPARE(privateController.activeUrl().host(), QStringLiteral("docs.example"));
}

void BrowserControllerTest::addsPredefinedSearchEngineProviders()
{
    QTemporaryDir root;
    BrowserController controller(root.filePath(QStringLiteral("data")), QStringLiteral("test"),
        root.filePath(QStringLiteral("config")));

    const auto presets = controller.searchEnginePresets();
    QVERIFY(presets.size() >= 5);
    QVERIFY(controller.addSearchEnginePreset(QStringLiteral("brave")));
    QVERIFY(!controller.addSearchEnginePreset(QStringLiteral("brave")));
    QCOMPARE(controller.searchEngines().size(), 2);

    controller.openInput(QStringLiteral("br private search"), false);
    QCOMPARE(controller.activeUrl().host(), QStringLiteral("search.brave.com"));
}

void BrowserControllerTest::refusesPersistentBrowsingDataActionsInPrivateWindows()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"), true);
    QSignalSpy clearSpy(&controller, &BrowserController::engineDataClearRequested);

    QVERIFY(!controller.clearBrowsingData(
        { QStringLiteral("cookies"), QStringLiteral("history") }, 0, false, {}));
    QCOMPARE(clearSpy.count(), 0);
    QCOMPARE(controller.history({}).size(), 0);
}

void BrowserControllerTest::clearsSelectedBrowsingDataWithinConfirmedScope()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
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

    QVERIFY(controller.clearBrowsingData({ QStringLiteral("history") }, 0, false, {}));
    QCOMPARE(controller.history({}).size(), 0);
    QCOMPARE(clearSpy.count(), 1);
    QVERIFY(controller.switchSpace(personalSpaceId));
    QCOMPARE(controller.history({}).size(), 1);
    QCOMPARE(controller.permissionDecision(
                 QUrl(QStringLiteral("https://personal-clear.example")), QStringLiteral("camera")),
        BrowserController::Block);

    QVERIFY(
        !controller.clearBrowsingData({ QStringLiteral("history"), QStringLiteral("permissions") },
            0, true, QStringLiteral("clear all")));
    QVERIFY(
        controller.clearBrowsingData({ QStringLiteral("history"), QStringLiteral("permissions") },
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
        BrowserController controller(root.path(), QStringLiteral("test"));
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

    BrowserController restored(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));
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

    const auto privateDecisions = QSharedPointer<QHash<QString, int>>::create();
    BrowserController firstPrivate(root.path(), QStringLiteral("test"), true, privateDecisions);
    BrowserController secondPrivate(root.path(), QStringLiteral("test"), true, privateDecisions);
    QVERIFY(firstPrivate.rememberExternalProtocolDecision(
        QUrl(QStringLiteral("https://private.example")), QStringLiteral("mailto")));
    QVERIFY(secondPrivate.externalProtocolAllowed(
        QUrl(QStringLiteral("https://private.example/path")), QStringLiteral("mailto")));
    QVERIFY(secondPrivate.externalProtocolAllowed(
        QUrl(QStringLiteral("https://private.example/path")), QStringLiteral("mailto")));

    BrowserController freshPrivate(root.path(), QStringLiteral("test"), true);
    QVERIFY(!freshPrivate.externalProtocolAllowed(
        QUrl(QStringLiteral("https://private.example")), QStringLiteral("mailto")));
}

void BrowserControllerTest::persistsOnlyNonPrivateDownloadHistory()
{
    QTemporaryDir root;
    {
        BrowserController controller(root.path(), QStringLiteral("test"));
        const auto downloadId = controller.recordDownload(QStringLiteral("runtime-1"),
            QUrl(QStringLiteral("https://files.example/archive.zip")),
            QStringLiteral("/Downloads/archive.zip"), QStringLiteral("in-progress"), 12, 100);
        QVERIFY(!downloadId.isEmpty());
        controller.closeActiveTab();
        QVERIFY(controller.updateDownload(downloadId, QStringLiteral("completed"), 100, 100, {}));

        BrowserController privateController(root.path(), QStringLiteral("test"), true);
        QVERIFY(privateController
                .recordDownload(QStringLiteral("private-download"),
                    QUrl(QStringLiteral("https://files.example/private.zip")),
                    QStringLiteral("/Downloads/private.zip"), QStringLiteral("completed"), 5, 5)
                .isEmpty());
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    const auto downloads = restored.downloadHistory();
    QCOMPARE(downloads.size(), 1);
    const auto record = downloads.first().toMap();
    QVERIFY(!record.value(QStringLiteral("id")).toString().isEmpty());
    QCOMPARE(record.value(QStringLiteral("state")).toString(), QStringLiteral("completed"));
    QCOMPARE(record.value(QStringLiteral("receivedBytes")).toLongLong(), 100);
}

void BrowserControllerTest::asksBeforeWritingDownAProgram()
{
    QTemporaryDir root;
    QTemporaryDir downloads;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const QUrl origin(QStringLiteral("https://files.example/page"));
    controller.recordOriginInteraction(origin);

    const auto document = controller.downloadDisposition(
        origin, QStringLiteral("notes.pdf"), QStringLiteral("application/pdf"), downloads.path());
    QCOMPARE(document.value(QStringLiteral("disposition")).toString(), QStringLiteral("accept"));
    QVERIFY(document.value(QStringLiteral("risk")).toString().isEmpty());

    const auto script = controller.downloadDisposition(
        origin, QStringLiteral("install.sh"), QStringLiteral("text/plain"), downloads.path());
    QCOMPARE(script.value(QStringLiteral("disposition")).toString(), QStringLiteral("confirm"));
    QCOMPARE(BrowserController::dispositionName(BrowserController::ConfirmDownload),
        QStringLiteral("confirm"));
    QCOMPARE(script.value(QStringLiteral("risk")).toString(), QStringLiteral("script"));
    QCOMPARE(script.value(QStringLiteral("fileName")).toString(), QStringLiteral("install.sh"));
    QCOMPARE(
        script.value(QStringLiteral("origin")).toString(), QStringLiteral("https://files.example"));
    QVERIFY(!script.value(QStringLiteral("automatic")).toBool());
}

void BrowserControllerTest::takesAPermissionForAutomaticAndMultipleDownloads()
{
    QTemporaryDir root;
    QTemporaryDir downloads;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto personalSpaceId = controller.activeSpaceId();
    const QUrl origin(QStringLiteral("https://files.example/page"));
    const auto disposition = [&] {
        return controller.downloadDisposition(origin, QStringLiteral("notes.pdf"),
            QStringLiteral("application/pdf"), downloads.path());
    };

    QCOMPARE(disposition().value(QStringLiteral("disposition")).toString(),
        QStringLiteral("permission"));
    QVERIFY(disposition().value(QStringLiteral("automatic")).toBool());

    QCOMPARE(controller.permissionPolicy(QStringLiteral("automatic-downloads")),
        static_cast<int>(BrowserController::Rememberable));
    QVERIFY(controller.setPermissionDecision(
        origin, QStringLiteral("automatic-downloads"), BrowserController::Block));
    QCOMPARE(
        disposition().value(QStringLiteral("disposition")).toString(), QStringLiteral("refuse"));

    QVERIFY(controller.setPermissionDecision(
        origin, QStringLiteral("automatic-downloads"), BrowserController::AllowPersistently));
    QCOMPARE(
        disposition().value(QStringLiteral("disposition")).toString(), QStringLiteral("accept"));

    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    QCOMPARE(disposition().value(QStringLiteral("disposition")).toString(),
        QStringLiteral("permission"));
    QVERIFY(controller.switchSpace(personalSpaceId));

    const QUrl clicked(QStringLiteral("https://other.example/page"));
    controller.recordOriginInteraction(clicked);
    const auto clickedDisposition = [&] {
        return controller
            .downloadDisposition(clicked, QStringLiteral("notes.pdf"),
                QStringLiteral("application/pdf"), downloads.path())
            .value(QStringLiteral("disposition"))
            .toString();
    };
    QCOMPARE(clickedDisposition(), QStringLiteral("accept"));
    controller.noteDownloadStarted(clicked, QStringLiteral("runtime-1"));
    QCOMPARE(clickedDisposition(), QStringLiteral("permission"));
    QCOMPARE(
        controller
            .downloadDisposition(QUrl(QStringLiteral("https://third.example/page")),
                QStringLiteral("notes.pdf"), QStringLiteral("application/pdf"), downloads.path())
            .value(QStringLiteral("automatic"))
            .toBool(),
        true);
    controller.noteDownloadSettled(QStringLiteral("runtime-1"));
    QCOMPARE(clickedDisposition(), QStringLiteral("accept"));
    controller.noteDownloadSettled(QStringLiteral("runtime-1"));
    QCOMPARE(clickedDisposition(), QStringLiteral("accept"));
}

void BrowserControllerTest::sendsAConflictingNameToTheSaveDialog()
{
    QTemporaryDir root;
    QTemporaryDir downloads;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const QUrl origin(QStringLiteral("https://files.example/page"));
    controller.recordOriginInteraction(origin);

    QFile existing(QDir(downloads.path()).filePath(QStringLiteral("notes.pdf")));
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.close();

    QCOMPARE(controller
                 .downloadDisposition(origin, QStringLiteral("notes.pdf"),
                     QStringLiteral("application/pdf"), downloads.path())
                 .value(QStringLiteral("disposition"))
                 .toString(),
        QStringLiteral("save-as"));
    QCOMPARE(controller
                 .downloadDisposition(origin, QStringLiteral("other.pdf"),
                     QStringLiteral("application/pdf"), downloads.path())
                 .value(QStringLiteral("disposition"))
                 .toString(),
        QStringLiteral("accept"));

    QFile program(QDir(downloads.path()).filePath(QStringLiteral("install.sh")));
    QVERIFY(program.open(QIODevice::WriteOnly));
    program.close();
    QCOMPARE(controller
                 .downloadDisposition(origin, QStringLiteral("install.sh"),
                     QStringLiteral("text/plain"), downloads.path())
                 .value(QStringLiteral("disposition"))
                 .toString(),
        QStringLiteral("confirm"));
    QCOMPARE(controller
                 .downloadDisposition(origin, QStringLiteral("install.sh"),
                     QStringLiteral("text/plain"), downloads.path(), true)
                 .value(QStringLiteral("disposition"))
                 .toString(),
        QStringLiteral("save-as"));
    QCOMPARE(controller
                 .downloadDisposition(origin, QStringLiteral("free.sh"),
                     QStringLiteral("text/plain"), downloads.path(), true)
                 .value(QStringLiteral("disposition"))
                 .toString(),
        QStringLiteral("accept"));
    QCOMPARE(controller
                 .downloadDisposition(QUrl(QStringLiteral("https://untouched.example/x")),
                     QStringLiteral("free.pdf"), QStringLiteral("application/pdf"),
                     downloads.path(), true)
                 .value(QStringLiteral("disposition"))
                 .toString(),
        QStringLiteral("accept"));
}

void BrowserControllerTest::configuresOneDownloadDirectoryForEveryWindow()
{
    QTemporaryDir root;
    QTemporaryDir config;
    QTemporaryDir chosen;
    {
        BrowserController controller(root.path(), QStringLiteral("test"), config.path());
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

    BrowserController restored(root.path(), QStringLiteral("test"), config.path());
    QCOMPARE(restored.downloadDirectory(), chosen.path());

    const auto decisions = QSharedPointer<QHash<QString, int>>::create();
    BrowserController privateWindow(
        root.path(), QStringLiteral("test"), true, decisions, config.path());
    QCOMPARE(privateWindow.downloadDirectory(), chosen.path());
    QTemporaryDir elsewhere;
    QVERIFY(!privateWindow.setDownloadDirectory(elsewhere.path()));
    QCOMPARE(privateWindow.downloadDirectory(), chosen.path());
}

void BrowserControllerTest::forgetsOneDownloadWithoutForgettingTheRest()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto first = controller.recordDownload(QStringLiteral("runtime-1"),
        QUrl(QStringLiteral("https://files.example/first.zip")),
        QStringLiteral("/Downloads/first.zip"), QStringLiteral("completed"), 10, 10);
    const auto second = controller.recordDownload(QStringLiteral("runtime-2"),
        QUrl(QStringLiteral("https://files.example/second.zip")),
        QStringLiteral("/Downloads/second.zip"), QStringLiteral("completed"), 20, 20);
    QCOMPARE(controller.downloadHistory().size(), 2);

    QVERIFY(controller.forgetDownload(first));
    const auto remaining = controller.downloadHistory();
    QCOMPARE(remaining.size(), 1);
    QCOMPARE(remaining.first().toMap().value(QStringLiteral("id")).toString(), second);
    QVERIFY(!controller.forgetDownload(first));
    QVERIFY(!controller.forgetDownload(QString()));

    const auto privateDecisions = QSharedPointer<QHash<QString, int>>::create();
    BrowserController privateWindow(root.path(), QStringLiteral("test"), true, privateDecisions);
    QVERIFY(!privateWindow.forgetDownload(second));
}

void BrowserControllerTest::persistsInterfacePreferencesOutsidePrivateBrowsing()
{
    QTemporaryDir root;
    {
        BrowserController controller(root.path(), QStringLiteral("test"));
        QCOMPARE(controller.preference(QStringLiteral("sidebar-width"), QStringLiteral("292")),
            QStringLiteral("292"));
        QVERIFY(controller.setPreference(QStringLiteral("sidebar-width"), QStringLiteral("360")));
        QVERIFY(controller.setPreference(QStringLiteral("sidebar-width"), QStringLiteral("412")));
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.preference(QStringLiteral("sidebar-width"), QStringLiteral("292")),
        QStringLiteral("412"));

    // A Private window browses on the defaults and writes nothing back.
    BrowserController privateController(root.path(), QStringLiteral("test"), true);
    QCOMPARE(privateController.preference(QStringLiteral("sidebar-width"), QStringLiteral("292")),
        QStringLiteral("292"));
    QVERIFY(
        !privateController.setPreference(QStringLiteral("sidebar-width"), QStringLiteral("500")));
    QCOMPARE(restored.preference(QStringLiteral("sidebar-width"), QStringLiteral("292")),
        QStringLiteral("412"));
}

// One inspector inspects one tab. Asking for it on a second tab moves it rather
// than opening another, and a blank tab has no page for it to attach to.
void BrowserControllerTest::attachesOneInspectorToOneTab()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));

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
    BrowserController controller(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));

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
    controller.updateTab(
        controller.activeTabId(), QUrl(QStringLiteral("about:blank")), QStringLiteral("New tab"));
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
    QTemporaryDir root;
    QString inspectedTabId;
    {
        BrowserController controller(root.path(), QStringLiteral("test"));
        controller.openInput(QStringLiteral("https://inspected.example"), false);
        inspectedTabId = controller.activeTabId();
        controller.openDeveloperTools();
        QCOMPARE(controller.developerToolsTabId(), inspectedTabId);
    }

    BrowserController restarted(root.path(), QStringLiteral("test"));
    QCOMPARE(restarted.activeTabId(), inspectedTabId);
    QVERIFY(restarted.developerToolsTabId().isEmpty());
    QVERIFY(!restarted.activeTabInspected());
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
        BrowserController controller(root.path(), QStringLiteral("test"));
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
    BrowserController restored(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));
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
    QTemporaryDir root;
    QString personalSpaceId;
    QString workSpaceId;
    {
        BrowserController controller(root.path(), QStringLiteral("test"));
        personalSpaceId = controller.activeSpaceId();
        controller.openInput(QStringLiteral("https://one.example"), false);
        controller.openInput(QStringLiteral("https://two.example"), true);
        const auto secondId = controller.activeTabId();
        controller.setTabZoom(secondId, 1.25);
        controller.setTabMuted(secondId, true);
        controller.toggleActivePinned();
        controller.toggleActivePinned();
        controller.closeTab(secondId);
        QCOMPARE(controller.closedTabCount(), 1);

        workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workSpaceId));
        // Another Space's closes are not this Space's to take back.
        QCOMPARE(controller.closedTabCount(), 0);
        controller.openInput(QStringLiteral("https://work.example"), false);
        controller.closeActiveTab();
        QCOMPARE(controller.closedTabCount(), 1);
        QVERIFY(controller.switchSpace(personalSpaceId));
        QCOMPARE(controller.closedTabCount(), 1);
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.activeSpaceId(), personalSpaceId);
    QCOMPARE(restored.closedTabCount(), 1);
    restored.reopenClosedTab();
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://two.example")));
    QCOMPARE(restored.activeTabZoom(), 1.25);
    auto *tabs = restored.tabs();
    const auto reopened = tabs->index(tabs->rowCount() - 1, 0);
    QVERIFY(tabs->data(reopened, TabListModel::MutedRole).toBool());
    QCOMPARE(restored.closedTabCount(), 0);

    QVERIFY(restored.switchSpace(workSpaceId));
    QCOMPARE(restored.closedTabCount(), 1);
    restored.reopenClosedTab();
    QCOMPARE(restored.activeUrl(), QUrl(QStringLiteral("https://work.example")));
}

// Twenty-five deep, in reverse closing order, and a pin comes back pinned.
void BrowserControllerTest::reopensClosedTabsNewestFirstAndBoundedAtTwentyFive()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
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
        BrowserController controller(root.path(), QStringLiteral("test"));
        controller.openInput(QStringLiteral("https://first.example"), false);
        controller.closeActiveTab();
        QCOMPARE(controller.tabs()->rowCount(), 1);
        controller.openInput(QStringLiteral("https://second.example"), false);
        controller.closeActiveTab();
        QCOMPARE(controller.closedTabCount(), 2);
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
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
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"), true);
    controller.openInput(QStringLiteral("https://private-one.example"), false);
    controller.openInput(QStringLiteral("https://private-two.example"), true);
    const auto secondId = controller.activeTabId();

    controller.closeTab(secondId);
    QCOMPARE(controller.closedTabCount(), 1);
    controller.reopenClosedTab();
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("https://private-two.example")));
    QCOMPARE(controller.closedTabCount(), 0);

    QVERIFY(!QFileInfo::exists(QDir(root.path()).filePath(QStringLiteral("state.sqlite"))));
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
        BrowserController controller(root.path(), QStringLiteral("test"));
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
        controller.updateTab(mutedId, QUrl(QStringLiteral("https://elsewhere.example")),
            QStringLiteral("Elsewhere"));
        QVERIFY(tabs->data(tabs->index(0, 0), TabListModel::MutedRole).toBool());
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
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
        BrowserController controller(root.path(), QStringLiteral("test"));
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

    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.activeTabId(), pinnedId);
    QVERIFY(restored.activeTabKeepActive());

    // A Private window has no Pinned section, so it has nothing to keep active.
    BrowserController privateController(root.path(), QStringLiteral("test"), true);
    privateController.openInput(QStringLiteral("https://kept.example"), false);
    QVERIFY(!privateController.setTabKeepActive(privateController.activeTabId(), true));
}

// Suspension has exactly two exceptions: a Pinned tab marked Keep active, and
// the tab an inspector is attached to. Both are named when their Space is put
// away and both are identified while it is gone; nothing else survives.
void BrowserControllerTest::namesEverySuspensionExceptionAndNothingElse()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto personalSpaceId = controller.activeSpaceId();
    controller.openInput(QStringLiteral("https://kept.example"), false);
    controller.toggleActivePinned();
    const auto keptId = controller.activeTabId();
    QVERIFY(controller.setTabKeepActive(keptId, true));
    controller.openInput(QStringLiteral("https://suspended.example"), true);
    controller.toggleActivePinned();
    const auto suspendedPinId = controller.activeTabId();
    controller.openInput(QStringLiteral("https://ordinary.example"), true);
    const auto inspectedId = controller.activeTabId();
    controller.openDeveloperTools();

    QCOMPARE(controller.retainedTabIds(), QStringList({ keptId, inspectedId }));
    // Nothing is being retained while the Space holding these tabs is the one
    // on show: its pages are live because the reader is looking at them.
    QVERIFY(controller.retainedTabs().isEmpty());

    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QSignalSpy suspendedSpy(&controller, &BrowserController::spaceSuspended);
    QVERIFY(controller.switchSpace(workSpaceId));
    QCOMPARE(suspendedSpy.count(), 1);
    QCOMPARE(suspendedSpy.first().at(0).toString(), personalSpaceId);
    QCOMPARE(suspendedSpy.first().at(1).toStringList(), QStringList({ keptId, inspectedId }));

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
    QTemporaryDir root;
    QString workSpaceId;
    QString keptId;
    {
        BrowserController controller(root.path(), QStringLiteral("test"));
        const auto personalSpaceId = controller.activeSpaceId();
        workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workSpaceId));
        controller.openInput(QStringLiteral("https://kept.example"), false);
        controller.toggleActivePinned();
        keptId = controller.activeTabId();
        QVERIFY(controller.setTabKeepActive(keptId, true));
        QVERIFY(controller.switchSpace(personalSpaceId));
    }

    // Nothing here has selected the Work Space, and the session still knows
    // one of its tabs is meant to be running.
    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.retainedTabs().size(), 1);
    const auto retained = restored.retainedTabs().first().toMap();
    QCOMPARE(retained.value(QStringLiteral("tabId")).toString(), keptId);
    QCOMPARE(retained.value(QStringLiteral("spaceId")).toString(), workSpaceId);
    QCOMPARE(retained.value(QStringLiteral("url")).toUrl(),
        QUrl(QStringLiteral("https://kept.example")));

    // Selecting that Space stops retaining it: the reader is looking at it.
    QVERIFY(restored.switchSpace(workSpaceId));
    QVERIFY(restored.retainedTabs().isEmpty());
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
    BrowserController crashed(root.path(), QStringLiteral("test"));
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

    BrowserController restored(root.path(), QStringLiteral("test"));
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
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto personalSpaceId = controller.activeSpaceId();
    controller.openInput(QStringLiteral("https://kept.example"), false);
    controller.toggleActivePinned();
    QVERIFY(controller.setTabKeepActive(controller.activeTabId(), true));

    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    QCOMPARE(controller.retainedTabs().size(), 1);

    QVERIFY(controller.deleteSpace(workSpaceId, QStringLiteral("Work")));
    QCOMPARE(controller.activeSpaceId(), personalSpaceId);
    QVERIFY(controller.retainedTabs().isEmpty());
}

// A notification names the origin and the Space, and activating it goes to the
// tab that sent it — changing Space on the way when it has to.
void BrowserControllerTest::routesNotificationsToTheOriginatingTab()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto personalSpaceId = controller.activeSpaceId();
    controller.openInput(QStringLiteral("https://chat.example/room"), false);
    const auto chatId = controller.activeTabId();
    controller.toggleActivePinned();
    QVERIFY(controller.setTabKeepActive(chatId, true));
    controller.openInput(QStringLiteral("https://quiet.example"), true);
    const auto quietId = controller.activeTabId();

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

    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
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
    Q_UNUSED(quietId)

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
        BrowserController controller(root.path(), QStringLiteral("test"));
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

    BrowserController restored(root.path(), QStringLiteral("test"));
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
        BrowserController controller(root.path(), QStringLiteral("test"));
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
        controller.updateTab(firstId, QUrl(QStringLiteral("https://elsewhere.example/")),
            QStringLiteral("Elsewhere"));
        QVERIFY(controller.tabSoundSuppressed(firstId));
    }

    // Nothing about it is the session's to keep: a restored tab is held silent
    // until the reader deals with its origin again.
    BrowserController restored(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));

    for (const auto *rememberable :
        { "camera", "microphone", "camera-and-microphone", "geolocation", "notifications" }) {
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

    for (const auto *eachTime : { "clipboard-read", "screen-sharing" }) {
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

    for (const auto *outside : { "usb", "bluetooth", "serial", "midi", "unsupported", "" }) {
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
    BrowserController controller(root.path(), QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));

    for (const auto *local :
        { "https://localhost:8443/app", "https://app.localhost/", "https://api.test/v1",
            "https://127.0.0.1:3000/", "https://[::1]:8080/", "https://192.168.1.20:8443/" }) {
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
        BrowserController controller(root.path(), QStringLiteral("test"));
        QVERIFY(controller.mayOfferCertificateException(local, true, true, false));
        // Answering it is answering one request. There is no decision to store
        // and no permission the answer becomes.
        QVERIFY(!controller.setPermissionDecision(
            local, QStringLiteral("certificate-exception"), BrowserController::AllowPersistently));
        QCOMPARE(controller.permissionPolicy(QStringLiteral("certificate-exception")),
            int(BrowserController::Refused));
        QVERIFY(controller.sitePermissions(local).isEmpty());
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
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
        BrowserController controller(root.path(), QStringLiteral("test"));
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
            QStringList { QStringLiteral("https://localhost:8443") });

        // The Space beside it shares no engine profile, so it shares no waived
        // check either.
        const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workSpaceId));
        QVERIFY(!controller.certificateExceptionInEffect(local));
        QVERIFY(controller.certificateExceptionOrigins().isEmpty());

        // Nothing about an address that names no site is recorded.
        QVERIFY(!controller.recordCertificateException(QUrl()));
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    QVERIFY(restored.switchSpace(personalSpaceId));
    QVERIFY(!restored.certificateExceptionInEffect(local));
    QVERIFY(restored.certificateExceptionOrigins().isEmpty());

    // Private windows share one session, and its waived checks go with it.
    WindowManager manager(QStringLiteral("test"));
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
    BrowserController controller(root.path(), QStringLiteral("test"));
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
        BrowserController controller(root.path(), QStringLiteral("test"));
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
    BrowserController restored(root.path(), QStringLiteral("test"));
    QVERIFY(restored.switchSpace(personalSpaceId));
    QVERIFY(restored.thirdPartyCookieAllowances().isEmpty());
    QVERIFY(!restored.thirdPartyCookiesAllowed(personalSpaceId, identity));

    // Private windows share one temporary session, and it goes when the last
    // of them does.
    WindowManager manager(QStringLiteral("test"));
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
    const QStringList named { QStringLiteral("Cookies"), QStringLiteral("Local Storage"),
        QStringLiteral("cache") };
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto spaceId = controller.activeSpaceId();
    const auto profilePath = controller.profilePathForSpace(spaceId);
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
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QCOMPARE(controller.siteDataBytes(workSpaceId, named), 0);
    // An engine that names nothing is an engine with nothing to measure, and
    // so is a Space with nowhere on disk to look.
    QCOMPARE(controller.siteDataBytes(spaceId, {}), -1);
    QCOMPARE(controller.siteDataBytes(QString(), named), -1);
}

QTEST_GUILESS_MAIN(BrowserControllerTest)

#include "tst_browsercontroller.moc"
