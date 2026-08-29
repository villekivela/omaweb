#include "BrowserController.h"
#include "TabListModel.h"

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using tanto::BrowserController;
using tanto::TabListModel;

class BrowserControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void createsPersonalSpaceAndBlankTab();
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
