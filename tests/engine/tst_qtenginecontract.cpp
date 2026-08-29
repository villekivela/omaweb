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

static QString keyboardNavigationPageScript()
{
    QFile file(QStringLiteral(TANTO_KEYBOARD_NAVIGATION_SCRIPT_PATH));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

class QtEngineContractTest final : public QObject {
    Q_OBJECT

private slots:
    void adaptersExposeSharedContract_data();
    void adaptersExposeSharedContract();
    void mockReportsLifecycleEvents();
    void mockReportsNewWindowPurpose();
    void adaptersExposeKeyboardNavigationCommands_data();
    void adaptersExposeKeyboardNavigationCommands();
    void qtAdapterPropagatesPageState();
    void qtProfilesIsolateSiteStorage();
    void qtPrivateWindowsShareOneProfile();
    void qtRoutesOnlyDialogDestinationsToAuxiliaryWindows();
    void qtKeyboardNavigationHonorsInputContracts_data();
    void qtKeyboardNavigationHonorsInputContracts();
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
    const auto missingContract = validateEngineViewContract(*adapter);
    QVERIFY2(missingContract.isEmpty(), qPrintable(missingContract.join(QStringLiteral("; "))));
    const auto capabilities = adapter->property("capabilities").toInt();
    QVERIFY(capabilities & EngineCapabilities::Navigation);
    QVERIFY(capabilities & EngineCapabilities::ContentBlocking);
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

void QtEngineContractTest::adaptersExposeKeyboardNavigationCommands_data()
{
    adaptersExposeSharedContract_data();
}

void QtEngineContractTest::adaptersExposeKeyboardNavigationCommands()
{
    QFETCH(QString, path);
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(path));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));

    const QVariantMap configuration = {
        {QStringLiteral("version"), 1},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("bindings"), QVariantMap{
            {QStringLiteral("j"), QStringLiteral("scroll-down")},
            {QStringLiteral("f"), QStringLiteral("open-link")},
        }},
        {QStringLiteral("passthroughAll"), false},
        {QStringLiteral("passthroughKeys"), QStringList{}},
    };
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "configureKeyboardNavigation",
        Q_ARG(QVariant, configuration)));
    QCOMPARE(adapter->property("keyboardNavigationConfiguration").toMap(), configuration);
    QVERIFY(adapter->setProperty("keyboardNavigationScriptSource",
        keyboardNavigationPageScript()));
    QVERIFY(adapter->property("capabilities").toInt()
        & EngineCapabilities::KeyboardPageCommands);
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
    window.resize(640, 480);
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

void QtEngineContractTest::qtKeyboardNavigationHonorsInputContracts_data()
{
    QTest::addColumn<QByteArray>("html");
    QTest::addColumn<int>("key");
    QTest::addColumn<QString>("expectedTitle");
    QTest::addColumn<bool>("passthroughAll");
    QTest::addColumn<QStringList>("passthroughKeys");

    QTest::newRow("editable controls receive typing")
        << QByteArray(R"HTML(<!doctype html><title>ready</title>
            <input autofocus aria-label="Editor">
            <script>document.querySelector('input').addEventListener('input',
                event => document.title = event.target.value);</script>)HTML")
        << int(Qt::Key_J) << QStringLiteral("j") << false << QStringList{};
    QTest::newRow("IME composition passes through")
        << QByteArray(R"HTML(<!doctype html><title>ready</title><script>
            addEventListener('keydown', event => setTimeout(() =>
                document.title = event.defaultPrevented ? 'blocked' : 'composing-passed'));
            setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                key: 'j', isComposing: true, bubbles: true, cancelable: true
            })), 250);
        </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("composing-passed") << false << QStringList{};
    QTest::newRow("non-English keys pass through")
        << QByteArray(R"HTML(<!doctype html><title>ready</title><script>
            addEventListener('keydown', event => setTimeout(() =>
                document.title = event.defaultPrevented ? 'blocked' : event.key));
            setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                key: 'å', bubbles: true, cancelable: true
            })), 250);
        </script>)HTML")
        << int(Qt::Key_unknown) << QString::fromUtf8("å") << false << QStringList{};
    QTest::newRow("j scrolls down")
        << QByteArray(R"HTML(<!doctype html><title>ready</title>
            <div style="height:3000px"></div><script>
                addEventListener('scroll', () => {
                    if (scrollY > 0) document.title = 'down';
                });
                setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                    key: 'j', bubbles: true, cancelable: true
                })), 300);
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("down") << false << QStringList{};
    QTest::newRow("k scrolls up")
        << QByteArray(R"HTML(<!doctype html><title>loading</title>
            <div style="height:3000px"></div><script>
                scrollTo(0, 600); document.title = 'ready';
                addEventListener('scroll', () => {
                    if (scrollY < 550) document.title = 'up';
                });
                setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                    key: 'k', bubbles: true, cancelable: true
                })), 300);
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("up") << false << QStringList{};
    QTest::newRow("gg scrolls to the top")
        << QByteArray(R"HTML(<!doctype html><title>loading</title>
            <div style="height:3000px"></div><script>
                scrollTo(0, 600); document.title = 'ready';
                addEventListener('scroll', () => {
                    if (scrollY === 0) document.title = 'top';
                });
                setTimeout(() => {
                    dispatchEvent(new KeyboardEvent('keydown', { key: 'g', bubbles: true, cancelable: true }));
                    dispatchEvent(new KeyboardEvent('keydown', { key: 'g', bubbles: true, cancelable: true }));
                }, 1000);
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("top") << false << QStringList{};
    QTest::newRow("G scrolls to the bottom")
        << QByteArray(R"HTML(<!doctype html><title>ready</title>
            <div style="height:3000px"></div><script>
                addEventListener('scroll', () => {
                    if (scrollY + innerHeight >= document.documentElement.scrollHeight - 1)
                        document.title = 'bottom';
                });
                setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                    key: 'G', shiftKey: true, bubbles: true, cancelable: true
                })), 300);
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("bottom") << false << QStringList{};
    QTest::newRow("link hints expose readable screen-reader text")
        << QByteArray(R"HTML(<!doctype html><title>ready</title>
            <a href="#target" aria-label="Read documentation">Docs</a>
            <script>
                document.documentElement.style.zoom = '175%';
                new MutationObserver(() => {
                    const overlay = document.getElementById('__tanto_link_hints');
                    const hint = overlay && overlay.querySelector('span');
                    if (overlay && overlay.getAttribute('role') === 'status'
                            && overlay.getAttribute('aria-live') === 'polite'
                            && hint && hint.getAttribute('role') === 'note'
                            && hint.getAttribute('aria-label').includes('Read documentation')
                            && parseFloat(getComputedStyle(hint).fontSize) >= 12
                            && hint.style.forcedColorAdjust === 'auto') document.title = 'accessible';
                }).observe(document.documentElement, { childList: true, subtree: true });
                setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                    key: 'f', bubbles: true, cancelable: true
                })), 300);
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("accessible") << false << QStringList{};
    QTest::newRow("Shift+F selects only background-capable targets")
        << QByteArray(R"HTML(<!doctype html><title>ready</title>
            <button aria-label="Not a link">Button</button>
            <a href="#target" aria-label="Open target">Target</a>
            <script>
                addEventListener('click', event => {
                    if (event.target.matches('a[href]')) {
                        event.preventDefault();
                        document.title = event.metaKey && event.ctrlKey
                            ? 'background' : 'wrong-activation';
                    }
                }, true);
                const activateBackgroundHint = () => {
                    dispatchEvent(new KeyboardEvent('keydown', {
                        key: 'F', shiftKey: true, bubbles: true, cancelable: true
                    }));
                    if (!document.getElementById('__tanto_link_hints')) {
                        setTimeout(activateBackgroundHint, 100);
                        return;
                    }
                    dispatchEvent(new KeyboardEvent('keydown', {
                        key: 'a', bubbles: true, cancelable: true
                    }));
                };
                activateBackgroundHint();
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("background") << false << QStringList{};
    QTest::newRow("per-key passthrough keeps site shortcuts")
        << QByteArray(R"HTML(<!doctype html><title>ready</title><script>
            addEventListener('keydown', event => document.title =
                event.defaultPrevented ? 'blocked' : 'site-key');
            setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                key: 'k', bubbles: true, cancelable: true
            })), 300);
        </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("site-key") << false
        << QStringList{QStringLiteral("k")};
    QTest::newRow("all-page passthrough keeps site shortcuts")
        << QByteArray(R"HTML(<!doctype html><title>ready</title><script>
            addEventListener('keydown', event => document.title =
                event.defaultPrevented ? 'blocked' : 'site-page');
            setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                key: 'j', bubbles: true, cancelable: true
            })), 300);
        </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("site-page") << true << QStringList{};
}

void QtEngineContractTest::qtKeyboardNavigationHonorsInputContracts()
{
    QFETCH(QByteArray, html);
    QFETCH(int, key);
    QFETCH(QString, expectedTitle);
    QFETCH(bool, passthroughAll);
    QFETCH(QStringList, passthroughKeys);
    QTemporaryDir root;
    QFile page(root.filePath(QStringLiteral("keyboard.html")));
    QVERIFY(page.open(QIODevice::WriteOnly));
    QCOMPARE(page.write(html), html.size());
    page.close();

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const QVariantMap configuration = {
        {QStringLiteral("version"), 1},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("bindings"), QVariantMap{
            {QStringLiteral("j"), QStringLiteral("scroll-down")},
            {QStringLiteral("k"), QStringLiteral("scroll-up")},
            {QStringLiteral("gg"), QStringLiteral("scroll-top")},
            {QStringLiteral("G"), QStringLiteral("scroll-bottom")},
            {QStringLiteral("f"), QStringLiteral("open-link")},
            {QStringLiteral("Shift+F"), QStringLiteral("open-link-background")},
        }},
        {QStringLiteral("passthroughAll"), passthroughAll},
        {QStringLiteral("passthroughKeys"), passthroughKeys},
    };
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("currentUrl"), QUrl::fromLocalFile(page.fileName())},
        {QStringLiteral("keyboardNavigationConfiguration"), configuration},
        {QStringLiteral("keyboardNavigationScriptSource"), keyboardNavigationPageScript()},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));

    auto *view = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(view);
    QQuickWindow window;
    window.resize(640, 480);
    view->setParentItem(window.contentItem());
    view->setSize(QSizeF(640, 480));
    window.show();
    window.requestActivate();
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "focusPage"));
    QTRY_VERIFY(adapter->property("pageHasFocus").toBool());
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("ready"));
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "configureKeyboardNavigation",
        Q_ARG(QVariant, configuration)));
    QTest::qWait(50);

    if (key != Qt::Key_unknown) {
        if (expectedTitle == QStringLiteral("j")) {
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(40, 15));
        }
        QTest::keyClick(&window, static_cast<Qt::Key>(key));
    }
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(), expectedTitle, 15000);
}


int main(int argc, char *argv[])
{
    QtWebEngineQuick::initialize();
    QGuiApplication application(argc, argv);
    QtEngineContractTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_qtenginecontract.moc"
