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
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

using tanto::BrowserController;
using tanto::SpaceListModel;
using tanto::TabListModel;
using tanto::WindowManager;

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
    void scopesExternalProtocolDecisionsToOriginSchemeSpaceAndPrivateSession();
    void persistsOnlyNonPrivateDownloadHistory();
    void persistsInterfacePreferencesOutsidePrivateBrowsing();
    void attachesOneInspectorToOneTab();
    void keepsTheInspectorThroughASpaceSwitch();
    void detachesTheInspectorWithTheTabItInspects();
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
    const auto backgroundTabId = controller.tabs()->data(
        backgroundIndex, TabListModel::IdRole).toString();

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

    QVERIFY(!controller.deleteSpace(workSpaceId, QString{}));
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

    QVERIFY(!controller.deleteSpace(workSpaceId, QString{}));
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
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE spaces (id TEXT PRIMARY KEY, name TEXT NOT NULL, "
            "color TEXT NOT NULL, active INTEGER NOT NULL DEFAULT 0, "
            "position INTEGER NOT NULL DEFAULT 0)")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE tabs (id TEXT PRIMARY KEY, space_id TEXT NOT NULL, "
            "url TEXT NOT NULL, title TEXT NOT NULL, pinned INTEGER NOT NULL DEFAULT 0, "
            "active INTEGER NOT NULL DEFAULT 0, position INTEGER NOT NULL DEFAULT 0)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO spaces(id, name, color, active, position) "
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
    controller.openInput(QStringLiteral("tanto browser"), true);
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
            controller.openInput(
                QStringLiteral("https://tab-%1.example").arg(index), true);
        }
        newlyPinnedId = controller.activeTabId();

        QCOMPARE(controller.tabs()->data(
            controller.tabs()->index(4, 0), TabListModel::IdRole).toString(), newlyPinnedId);
        controller.toggleActivePinned();

        QCOMPARE(controller.tabs()->data(
            controller.tabs()->index(1, 0), TabListModel::IdRole).toString(), newlyPinnedId);
        QCOMPARE(controller.pinnedTabs()->rowCount(), 2);
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.tabs()->data(
        restored.tabs()->index(1, 0), TabListModel::IdRole).toString(), newlyPinnedId);
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
    QCOMPARE(controller.activeRendererFailureReason(), QStringLiteral("Renderer exited unexpectedly"));

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
    QVERIFY(manager.acceptPrivateDownloads());
    QVERIFY(!manager.recordPrivateDownloads());
    QVERIFY(!manager.privateDownloadDirectory().isEmpty());
    QVERIFY(first->setPermissionDecision(QUrl(QStringLiteral("https://camera.example")),
        QStringLiteral("camera"), BrowserController::AllowOnce));
    QCOMPARE(second->permissionDecision(QUrl(QStringLiteral("https://camera.example/path")),
                 QStringLiteral("camera")),
        BrowserController::AllowOnce);
    QCOMPARE(second->permissionDecision(QUrl(QStringLiteral("https://camera.example/path")),
                 QStringLiteral("camera")), BrowserController::Ask);

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
    QCOMPARE(fresh->permissionDecision(QUrl(QStringLiteral("https://camera.example")),
                 QStringLiteral("camera")),
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
    controller.recordVisit(QUrl(QStringLiteral("https://docs.example/work")),
        QStringLiteral("Work documentation"));

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
    controller.recordVisit(QUrl(QStringLiteral("https://one.example/first")),
        QStringLiteral("First visit"));
    QTest::qWait(2);
    const auto boundary = QDateTime::currentMSecsSinceEpoch();
    QTest::qWait(2);
    controller.recordVisit(QUrl(QStringLiteral("https://one.example/second")),
        QStringLiteral("Second visit"));
    controller.recordVisit(QUrl(QStringLiteral("https://two.example/keep")),
        QStringLiteral("Keep this"));

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
    QCOMPARE(controller.history(QStringLiteral("one.example")).first().toMap()
                 .value(QStringLiteral("url")).toUrl().port(), 444);
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
    QVERIFY(legacy.write(R"JSON({
        "default": "docs",
        "engines": [{
            "id": "docs", "name": "Docs", "queryUrl": "https://docs.example/?q={query}",
            "keyword": "d"
        }]
    })JSON") > 0);
    legacy.close();

    BrowserController controller(root.filePath(QStringLiteral("data")),
        QStringLiteral("test"), configRoot);
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
    BrowserController controller(root.filePath(QStringLiteral("data")),
        QStringLiteral("test"), root.filePath(QStringLiteral("config")));

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
        {QStringLiteral("cookies"), QStringLiteral("history")}, 0, false, {}));
    QCOMPARE(clearSpy.count(), 0);
    QCOMPARE(controller.history({}).size(), 0);
}

void BrowserControllerTest::clearsSelectedBrowsingDataWithinConfirmedScope()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto personalSpaceId = controller.activeSpaceId();
    controller.recordVisit(QUrl(QStringLiteral("https://personal-clear.example")),
        QStringLiteral("Personal"));
    QVERIFY(controller.setPermissionDecision(
        QUrl(QStringLiteral("https://personal-clear.example")), QStringLiteral("camera"),
        BrowserController::Block));
    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    controller.recordVisit(QUrl(QStringLiteral("https://work-clear.example")),
        QStringLiteral("Work"));
    QSignalSpy clearSpy(&controller, &BrowserController::engineDataClearRequested);

    QVERIFY(controller.clearBrowsingData({QStringLiteral("history")}, 0, false, {}));
    QCOMPARE(controller.history({}).size(), 0);
    QCOMPARE(clearSpy.count(), 1);
    QVERIFY(controller.switchSpace(personalSpaceId));
    QCOMPARE(controller.history({}).size(), 1);
    QCOMPARE(controller.permissionDecision(QUrl(QStringLiteral("https://personal-clear.example")),
                 QStringLiteral("camera")), BrowserController::Block);

    QVERIFY(!controller.clearBrowsingData({QStringLiteral("history"),
        QStringLiteral("permissions")}, 0, true, QStringLiteral("clear all")));
    QVERIFY(controller.clearBrowsingData({QStringLiteral("history"),
        QStringLiteral("permissions")}, 0, true, QStringLiteral("CLEAR ALL")));
    QCOMPARE(controller.history({}).size(), 0);
    QCOMPARE(controller.permissionDecision(QUrl(QStringLiteral("https://personal-clear.example")),
                 QStringLiteral("camera")), BrowserController::Ask);
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
        QCOMPARE(controller.permissionDecision(
                     QUrl(QStringLiteral("https://example.com/another")),
                     QStringLiteral("geolocation")),
            BrowserController::AllowPersistently);
        QCOMPARE(controller.permissionDecision(
                     QUrl(QStringLiteral("https://example.com:443/default-port")),
                     QStringLiteral("geolocation")),
            BrowserController::AllowPersistently);
        QCOMPARE(controller.permissionDecision(
                     QUrl(QStringLiteral("https://sub.example.com")),
                     QStringLiteral("geolocation")),
            BrowserController::Ask);
        QCOMPARE(controller.permissionDecision(
                     QUrl(QStringLiteral("https://example.com:444")),
                     QStringLiteral("geolocation")),
            BrowserController::Ask);

        QVERIFY(controller.setPermissionDecision(QUrl(QStringLiteral("https://once.example")),
            QStringLiteral("notifications"), BrowserController::AllowOnce));
        workSpaceId = controller.createSpace(QStringLiteral("Work"));
        QVERIFY(controller.switchSpace(workSpaceId));
        QCOMPARE(controller.permissionDecision(QUrl(QStringLiteral("https://example.com")),
                     QStringLiteral("geolocation")),
            BrowserController::Ask);
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    QVERIFY(restored.switchSpace(personalSpaceId));
    QCOMPARE(restored.permissionDecision(QUrl(QStringLiteral("https://example.com")),
                 QStringLiteral("geolocation")),
        BrowserController::AllowPersistently);
    QCOMPARE(restored.permissionDecision(QUrl(QStringLiteral("https://once.example")),
                 QStringLiteral("notifications")),
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
        QVERIFY(controller.updateDownload(
            downloadId, QStringLiteral("completed"), 100, 100, {}));

        BrowserController privateController(root.path(), QStringLiteral("test"), true);
        QVERIFY(privateController.recordDownload(QStringLiteral("private-download"),
            QUrl(QStringLiteral("https://files.example/private.zip")),
            QStringLiteral("/Downloads/private.zip"), QStringLiteral("completed"), 5, 5).isEmpty());
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    const auto downloads = restored.downloadHistory();
    QCOMPARE(downloads.size(), 1);
    const auto record = downloads.first().toMap();
    QVERIFY(!record.value(QStringLiteral("id")).toString().isEmpty());
    QCOMPARE(record.value(QStringLiteral("state")).toString(), QStringLiteral("completed"));
    QCOMPARE(record.value(QStringLiteral("receivedBytes")).toLongLong(), 100);
}

void BrowserControllerTest::persistsInterfacePreferencesOutsidePrivateBrowsing()
{
    QTemporaryDir root;
    {
        BrowserController controller(root.path(), QStringLiteral("test"));
        QCOMPARE(controller.preference(QStringLiteral("sidebar-width"),
            QStringLiteral("292")), QStringLiteral("292"));
        QVERIFY(controller.setPreference(QStringLiteral("sidebar-width"), QStringLiteral("360")));
        QVERIFY(controller.setPreference(QStringLiteral("sidebar-width"), QStringLiteral("412")));
    }

    BrowserController restored(root.path(), QStringLiteral("test"));
    QCOMPARE(restored.preference(QStringLiteral("sidebar-width"), QStringLiteral("292")),
        QStringLiteral("412"));

    // A Private window browses on the defaults and writes nothing back.
    BrowserController privateController(root.path(), QStringLiteral("test"), true);
    QCOMPARE(privateController.preference(QStringLiteral("sidebar-width"),
        QStringLiteral("292")), QStringLiteral("292"));
    QVERIFY(!privateController.setPreference(QStringLiteral("sidebar-width"),
        QStringLiteral("500")));
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
    QVERIFY2(controller.developerToolsTabId().isEmpty(),
        "a blank tab has no page to inspect");

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

    QVERIFY(controller.switchSpace(controller.spaces()->data(
        controller.spaces()->index(0, 0), SpaceListModel::IdRole).toString()));
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
    controller.updateTab(controller.activeTabId(), QUrl(QStringLiteral("about:blank")),
        QStringLiteral("New tab"));
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
    const auto personalSpaceId = controller.spaces()->data(
        controller.spaces()->index(0, 0), SpaceListModel::IdRole).toString();
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

QTEST_GUILESS_MAIN(BrowserControllerTest)

#include "tst_browsercontroller.moc"
