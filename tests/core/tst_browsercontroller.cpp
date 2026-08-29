#include "BrowserController.h"
#include "TabListModel.h"

#include <QAbstractItemModel>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using tanto::BrowserController;
using tanto::TabListModel;

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
    void separatesEngineStorageBySpaceAndEngine();
    void createsTabOnlyAfterCommittedInput();
    void persistsTabsAndPins();
    void keepsFinalTabAsBlankTab();
    void keepsRendererFailureOnAffectedTab();
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

void BrowserControllerTest::keepsFinalTabAsBlankTab()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
    controller.openInput(QStringLiteral("https://example.com"), false);
    controller.closeActiveTab();
    QCOMPARE(controller.tabs()->rowCount(), 1);
    QCOMPARE(controller.activeUrl(), QUrl(QStringLiteral("about:blank")));
}

void BrowserControllerTest::keepsRendererFailureOnAffectedTab()
{
    QTemporaryDir root;
    BrowserController controller(root.path(), QStringLiteral("test"));
    const auto failedTabId = controller.activeTabId();

    controller.reportRendererFailure(QStringLiteral("Renderer exited unexpectedly"));
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

QTEST_GUILESS_MAIN(BrowserControllerTest)

#include "tst_browsercontroller.moc"
