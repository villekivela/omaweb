#include "EngineCapabilities.h"
#include "EngineViewContract.h"

#include <QGuiApplication>
#include <QFile>
#include <QMetaMethod>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QtWebEngineCore/QWebEngineNewWindowRequest>

#include <memory>

using tanto::validateEngineViewContract;
using tanto::EngineCapabilities;

class QtEngineContractTest final : public QObject {
    Q_OBJECT

private slots:
    void adaptersExposeSharedContract_data();
    void adaptersExposeSharedContract();
    void mockReportsLifecycleEvents();
    void mockReportsNewWindowPurpose();
    void qtAdapterPropagatesPageState();
    void qtProfilesIsolateSiteStorage();
    void qtPrivateWindowsShareOneProfile();
    void qtRoutesOnlyDialogDestinationsToAuxiliaryWindows();
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

void QtEngineContractTest::mockReportsNewWindowPurpose()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_MOCK_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));

    QSignalSpy requestSpy(adapter.get(), SIGNAL(auxiliaryWindowRequested(QVariant,QUrl)));
    QVERIFY(requestSpy.isValid());
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "simulateNewWindowRequest",
        Q_ARG(QVariant, QVariant::fromValue(QUrl(QStringLiteral("https://example.com/popup")))),
        Q_ARG(QVariant, QVariant::fromValue(true))));
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.first().at(1).toUrl(), QUrl(QStringLiteral("https://example.com/popup")));
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
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "goForward"));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Second page"));

    loadingSpy.clear();
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "reloadPage"));
    QTRY_VERIFY(loadingSpy.count() > 0);
    QTRY_VERIFY(!adapter->property("loading").toBool());
    QVERIFY(loadingSpy.count() > 0);

    QSignalSpy failureSpy(adapter.get(), SIGNAL(rendererFailed(QString)));
    QVERIFY(failureSpy.isValid());
    auto *webView = adapter->findChild<QObject *>(QStringLiteral("qtWebView"));
    QVERIFY(webView);
    QMetaMethod terminationSignal;
    for (int index = 0; index < webView->metaObject()->methodCount(); ++index) {
        const auto method = webView->metaObject()->method(index);
        if (method.name() == "renderProcessTerminated") {
            terminationSignal = method;
            break;
        }
    }
    QVERIFY(terminationSignal.isValid());
    int terminationStatus = 0;
    const int exitCode = 17;
    QVERIFY(terminationSignal.invoke(webView, Qt::DirectConnection,
        QGenericArgument(terminationSignal.parameterMetaType(0).name(), &terminationStatus),
        QGenericArgument("int", &exitCode)));
    QCOMPARE(failureSpy.count(), 1);
    QVERIFY(failureSpy.takeFirst().first().toString().contains(QString::number(exitCode)));
}

void QtEngineContractTest::qtProfilesIsolateSiteStorage()
{
    QTemporaryDir root;
    QFile page(root.filePath(QStringLiteral("storage.html")));
    QVERIFY(page.open(QIODevice::WriteOnly));
    page.write(R"HTML(<!doctype html><title>loading</title><script>
        const value = new URLSearchParams(location.search).get('value');
        if (value) localStorage.setItem('login', value);
        document.title = localStorage.getItem('login') || 'none';
    </script>)HTML");
    page.close();

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> personal(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("personal"))},
    }));
    const std::unique_ptr<QObject> work(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("work"))},
    }));
    QVERIFY2(personal, qPrintable(component.errorString()));
    QVERIFY2(work, qPrintable(component.errorString()));

    QQuickWindow personalWindow;
    QQuickWindow workWindow;
    qobject_cast<QQuickItem *>(personal.get())->setParentItem(personalWindow.contentItem());
    qobject_cast<QQuickItem *>(work.get())->setParentItem(workWindow.contentItem());
    personalWindow.show();
    workWindow.show();

    auto personalUrl = QUrl::fromLocalFile(page.fileName());
    personalUrl.setQuery(QStringLiteral("value=personal"));
    QVERIFY(personal->setProperty("currentUrl", personalUrl));
    QTRY_COMPARE(personal->property("pageTitle").toString(), QStringLiteral("personal"));

    const auto readUrl = QUrl::fromLocalFile(page.fileName());
    QVERIFY(work->setProperty("currentUrl", readUrl));
    QTRY_COMPARE(work->property("pageTitle").toString(), QStringLiteral("none"));

    auto workUrl = readUrl;
    workUrl.setQuery(QStringLiteral("value=work"));
    QVERIFY(work->setProperty("currentUrl", workUrl));
    QTRY_COMPARE(work->property("pageTitle").toString(), QStringLiteral("work"));
    QVERIFY(personal->setProperty("currentUrl", readUrl));
    QTRY_COMPARE(personal->property("pageTitle").toString(), QStringLiteral("personal"));
}

void QtEngineContractTest::qtPrivateWindowsShareOneProfile()
{
    QTemporaryDir root;
    QFile page(root.filePath(QStringLiteral("private-storage.html")));
    QVERIFY(page.open(QIODevice::WriteOnly));
    page.write(R"HTML(<!doctype html><title>loading</title><script>
        const value = new URLSearchParams(location.search).get('value');
        if (value) localStorage.setItem('private-login', value);
        document.title = localStorage.getItem('private-login') || 'none';
    </script>)HTML");
    page.close();

    QQmlEngine engine;
    QQmlComponent profileComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_PROFILE_PATH)));
    const std::unique_ptr<QObject> profileHost(profileComponent.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("private"))},
    }));
    QVERIFY2(profileHost, qPrintable(profileComponent.errorString()));
    const auto sharedProfile = profileHost->property("profile");
    QVERIFY(sharedProfile.isValid());

    QQmlComponent viewComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> first(viewComponent.createWithInitialProperties({
        {QStringLiteral("sharedProfile"), sharedProfile},
    }));
    const std::unique_ptr<QObject> second(viewComponent.createWithInitialProperties({
        {QStringLiteral("sharedProfile"), sharedProfile},
    }));
    QVERIFY2(first, qPrintable(viewComponent.errorString()));
    QVERIFY2(second, qPrintable(viewComponent.errorString()));
    QCOMPARE(first->property("browserProfile"), second->property("browserProfile"));

    QQuickWindow firstWindow;
    QQuickWindow secondWindow;
    qobject_cast<QQuickItem *>(first.get())->setParentItem(firstWindow.contentItem());
    qobject_cast<QQuickItem *>(second.get())->setParentItem(secondWindow.contentItem());
    firstWindow.show();
    secondWindow.show();

    auto loginUrl = QUrl::fromLocalFile(page.fileName());
    loginUrl.setQuery(QStringLiteral("value=private"));
    QVERIFY(first->setProperty("currentUrl", loginUrl));
    QTRY_COMPARE(first->property("pageTitle").toString(), QStringLiteral("private"));

    const auto readUrl = QUrl::fromLocalFile(page.fileName());
    QVERIFY(second->setProperty("currentUrl", readUrl));
    QTRY_COMPARE(second->property("pageTitle").toString(), QStringLiteral("private"));
}

void QtEngineContractTest::qtRoutesOnlyDialogDestinationsToAuxiliaryWindows()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));

    const auto routesToAuxiliary = [&adapter](QWebEngineNewWindowRequest::DestinationType type) {
        QVariant result;
        const auto destination = QVariant::fromValue(static_cast<int>(type));
        const auto invoked = QMetaObject::invokeMethod(adapter.get(), "isAuxiliaryDestination",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, destination));
        return invoked && result.toBool();
    };

    QVERIFY(!routesToAuxiliary(QWebEngineNewWindowRequest::InNewWindow));
    QVERIFY(!routesToAuxiliary(QWebEngineNewWindowRequest::InNewTab));
    QVERIFY(routesToAuxiliary(QWebEngineNewWindowRequest::InNewDialog));
    QVERIFY(!routesToAuxiliary(QWebEngineNewWindowRequest::InNewBackgroundTab));
}

int main(int argc, char *argv[])
{
    QtWebEngineQuick::initialize();
    QGuiApplication application(argc, argv);
    QtEngineContractTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_qtenginecontract.moc"
