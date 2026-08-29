#include "EngineCapabilities.h"
#include "EngineViewContract.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include <memory>

using tanto::validateEngineViewContract;
using tanto::EngineCapabilities;

class QtEngineContractTest final : public QObject {
    Q_OBJECT

private slots:
    void adaptersExposeSharedContract_data();
    void adaptersExposeSharedContract();
    void mockReportsLifecycleEvents();
    void qtAdapterPropagatesPageState();
};

void QtEngineContractTest::adaptersExposeSharedContract_data()
{
    QTest::addColumn<QString>("path");
    QTest::newRow("UI-lab mock") << QStringLiteral(TANTO_MOCK_ENGINE_VIEW_PATH);
    QTest::newRow("QtWebEngine") << QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH);
}

void QtEngineContractTest::adaptersExposeSharedContract()
{
    QFETCH(QString, path);
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(path));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));
    QCOMPARE(validateEngineViewContract(*adapter), QStringList{});
    const auto capabilities = adapter->property("capabilities").toInt();
    QVERIFY(capabilities & EngineCapabilities::Navigation);
    QVERIFY(capabilities & EngineCapabilities::RendererRecovery);
}

void QtEngineContractTest::mockReportsLifecycleEvents()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_MOCK_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));

    QSignalSpy failureSpy(adapter.get(), SIGNAL(rendererFailed(QString)));
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "simulateRendererFailure"));
    QCOMPARE(failureSpy.count(), 1);

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "reloadPage"));
    QVERIFY(adapter->property("loading").toBool());
    QTRY_VERIFY(!adapter->property("loading").toBool());
}

void QtEngineContractTest::qtAdapterPropagatesPageState()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));

    auto *view = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(view);
    QQuickWindow window;
    view->setParentItem(window.contentItem());
    view->setSize(QSizeF(640, 480));
    window.show();

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "focusPage"));
    QTRY_VERIFY(adapter->property("pageHasFocus").toBool());

    QSignalSpy loadingSpy(adapter.get(), SIGNAL(loadingChanged()));
    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>First page</title><p>first</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("First page"));
    QTRY_VERIFY(!adapter->property("loading").toBool());

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Second page</title><p>second</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Second page"));
    QTRY_VERIFY(adapter->property("canGoBack").toBool());
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "goBack"));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("First page"));
    QVERIFY(loadingSpy.count() > 0);

    QSignalSpy failureSpy(adapter.get(), SIGNAL(rendererFailed(QString)));
    QVERIFY(failureSpy.isValid());
}

int main(int argc, char *argv[])
{
    QtWebEngineQuick::initialize();
    QGuiApplication application(argc, argv);
    QtEngineContractTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_qtenginecontract.moc"
