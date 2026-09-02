#include "ContentBlocker.h"
#include "EngineCapabilities.h"
#include "ExternalProtocolHandler.h"
#include "QtContentBlocker.h"
#include "EngineViewContract.h"
#include "ProcessResources.h"

#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QMetaMethod>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTemporaryDir>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QtWebEngineCore/QWebEngineNewWindowRequest>
#include <QtWebEngineQuick/QQuickWebEngineProfile>

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
    void qtSpaceProfilesKeepSiteStorageOnDisk();
    void qtRoutesOnlyDialogDestinationsToAuxiliaryWindows();
    void qtKeyboardNavigationHonorsInputContracts_data();
    void qtKeyboardNavigationHonorsInputContracts();
    void qtLinkHintsOwnSingleKeyShortcuts();
    void qtHidesCosmeticRulesBeforeThePageRuns();
    void qtRunsScriptletsBeforeThePageRuns();
    void qtRefusesTheWindowsTheListsNameAndNoOthers();
    void qtServesTheSubstitutesTheListsName();
    void qtStripsTheParametersTheListsName();
    void qtAttachesBlockingToTheProfileQmlCreates();
    void adaptersNameTheColoursTheirInspectorIsDrawnIn_data();
    void adaptersNameTheColoursTheirInspectorIsDrawnIn();
    void qtDocksAnInspectorDrawnInTantosColours();
    void qtKeepsAnInspectedTabActiveOnlyWhileAttached();
    void qtInspectsAPrivateTabInItsOwnTemporaryProfile();
    void qtPicksAnElementWhenNoContextMenuNamedOne();
    void qtDrawsMarkupStructureQuieterThanItsContent();
    void qtReportsThePageContextAndDrawsNoMenuOfItsOwn();
    void qtReportsTargetsInsideCrossOriginFrames();
    void qtOwnsJavaScriptPromptsAndReturnsTheirAnswer();
    void qtOpensOneLocalFileWithoutDirectoryWideAccess();
    void adaptersAnswerForEveryEverydayPageOperation_data();
    void adaptersAnswerForEveryEverydayPageOperation();
    void qtFindsInThePageAndKeepsTheQueryAcrossNavigation();
    void qtKeepsTheZoomItIsGivenAcrossNavigation();
    void qtSeparatesReloadBypassingCacheFromReloadAndStop();
    void qtRendersAPageForPrintingAndDrawsPdfsInline();
    void qtReportsSiteFullscreenWithItsOrigin();
    void profileAdaptersHandOverNotifications_data();
    void profileAdaptersHandOverNotifications();
    void adaptersTakeTheShellsAutoplayDecision_data();
    void adaptersTakeTheShellsAutoplayDecision();
    void qtReportsTheProcessDrawingThePage();
};

namespace {

// A palette with nothing in common with either Tanto's own default or the
// frontend's, so a colour that arrives could only have come from here.
QVariantMap inspectorPalette()
{
    return {
        {QStringLiteral("windowOpaque"), QStringLiteral("#0b1a0b")},
        {QStringLiteral("sidebarOpaque"), QStringLiteral("#112b11")},
        {QStringLiteral("surface"), QStringLiteral("#1a3d1a")},
        {QStringLiteral("surfaceHover"), QStringLiteral("#245224")},
        {QStringLiteral("border"), QStringLiteral("#2f6b2f")},
        {QStringLiteral("text"), QStringLiteral("#e8ffe8")},
        {QStringLiteral("mutedText"), QStringLiteral("#9dc79d")},
        {QStringLiteral("accent"), QStringLiteral("#00ff88")},
        {QStringLiteral("urgent"), QStringLiteral("#ff0044")},
        {QStringLiteral("syntax"), QVariantMap{
            {QStringLiteral("keyword"), QStringLiteral("#123456")},
            {QStringLiteral("string"), QStringLiteral("#654321")},
            {QStringLiteral("number"), QStringLiteral("#abcdef")},
            {QStringLiteral("comment"), QStringLiteral("#fedcba")},
            {QStringLiteral("tag"), QStringLiteral("#010203")},
            {QStringLiteral("attribute"), QStringLiteral("#040506")},
            {QStringLiteral("variable"), QStringLiteral("#0a0b0c")},
            {QStringLiteral("function"), QStringLiteral("#0d0e0f")},
            {QStringLiteral("type"), QStringLiteral("#101112")},
            {QStringLiteral("punctuation"), QStringLiteral("#778899")},
        }},
        {QStringLiteral("font"), QVariantMap{
            {QStringLiteral("family"), QStringLiteral("Menlo")},
            {QStringLiteral("size"), 12},
        }},
    };
}

} // namespace

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

// A QML-declared WebEngineProfile is off-the-record unless it says otherwise,
// and an off-the-record one keeps every cookie in memory however loudly the
// storage name and cookie policy ask for disk. Nothing about a Space profile
// looks wrong until the browser restarts and every login is gone, so the
// contract is checked where it shows: on the profile and on the directory.
void QtEngineContractTest::qtSpaceProfilesKeepSiteStorageOnDisk()
{
    QTemporaryDir root;
    const auto spacePath = root.filePath(QStringLiteral("space"));

    QQmlEngine engine;
    QQmlComponent profileComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_PROFILE_PATH)));
    const std::unique_ptr<QObject> spaceHost(profileComponent.createWithInitialProperties({
        {QStringLiteral("profilePath"), spacePath},
        {QStringLiteral("privateBrowsing"), false},
    }));
    QVERIFY2(spaceHost, qPrintable(profileComponent.errorString()));
    auto *spaceProfile = spaceHost->property("profile").value<QObject *>();
    QVERIFY(spaceProfile);
    QCOMPARE(spaceProfile->property("offTheRecord").toBool(), false);
    // Read back rather than trust the write: an off-the-record profile accepts
    // the assignment and reports NoPersistentCookies anyway.
    QCOMPARE(spaceProfile->property("persistentCookiesPolicy").toInt(),
        static_cast<int>(QQuickWebEngineProfile::ForcePersistentCookies));

    const std::unique_ptr<QObject> privateHost(profileComponent.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("private"))},
    }));
    QVERIFY2(privateHost, qPrintable(profileComponent.errorString()));
    auto *privateProfile = privateHost->property("profile").value<QObject *>();
    QVERIFY(privateProfile);
    QCOMPARE(privateProfile->property("offTheRecord").toBool(), true);

    QFile page(root.filePath(QStringLiteral("persisted-storage.html")));
    QVERIFY(page.open(QIODevice::WriteOnly));
    page.write(R"HTML(<!doctype html><title>loading</title><script>
        localStorage.setItem('login', 'kept');
        document.title = localStorage.getItem('login') || 'none';
    </script>)HTML");
    page.close();

    QQmlComponent viewComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> view(viewComponent.createWithInitialProperties({
        {QStringLiteral("sharedProfile"), spaceHost->property("profile")},
    }));
    QVERIFY2(view, qPrintable(viewComponent.errorString()));
    QQuickWindow window;
    qobject_cast<QQuickItem *>(view.get())->setParentItem(window.contentItem());
    window.show();

    QVERIFY(view->setProperty("currentUrl", QUrl::fromLocalFile(page.fileName())));
    QTRY_COMPARE(view->property("pageTitle").toString(), QStringLiteral("kept"));
    QTRY_VERIFY(!QDir(spacePath).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)
        .isEmpty());
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
    QTest::newRow("d scrolls down half a page")
        << QByteArray(R"HTML(<!doctype html><title>ready</title>
            <div style="height:3000px"></div><script>
                addEventListener('scroll', () => {
                    if (scrollY >= innerHeight * .4) document.title = 'half-down';
                });
                setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                    key: 'd', bubbles: true, cancelable: true
                })), 300);
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("half-down") << false << QStringList{};
    QTest::newRow("u scrolls up half a page")
        << QByteArray(R"HTML(<!doctype html><title>loading</title>
            <div style="height:3000px"></div><script>
                addEventListener('scroll', () => {
                    if (scrollY <= innerHeight * .6) document.title = 'half-up';
                });
                setTimeout(() => {
                    scrollTo(0, innerHeight);
                    document.title = 'ready';
                    setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                        key: 'u', bubbles: true, cancelable: true
                    })), 300);
                }, 300);
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("half-up") << false << QStringList{};
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
    QTest::newRow("link hints take editable keyboard focus")
        << QByteArray(R"HTML(<!doctype html><title>ready</title>
            <a href="#target">Target</a><script>
                new MutationObserver(() => {
                    if (document.getElementById('__tanto_link_hints')
                            && document.activeElement.id === '__tanto_link_hint_input')
                        document.title = 'hint-focus';
                }).observe(document.documentElement, { childList: true, subtree: true });
                setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                    key: 'f', bubbles: true, cancelable: true
                })), 300);
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("hint-focus") << false << QStringList{};
    QTest::newRow("link hints use theme colors and font")
        << QByteArray(R"HTML(<!doctype html><title>ready</title>
            <a href="#target">Target</a><script>
                new MutationObserver(() => {
                    const hint = document.querySelector('#__tanto_link_hints > span');
                    if (!hint) return;
                    const style = getComputedStyle(hint);
                    // The hint is the sidebar's site chip: a surface plate,
                    // and the code itself in the accent the border wears.
                    if (style.backgroundColor === 'rgb(18, 52, 86)'
                            && style.borderColor === 'rgb(101, 67, 33)'
                            && style.color === 'rgb(101, 67, 33)'
                            && style.borderRadius === '2px'
                            && style.fontWeight === '600'
                            && style.fontFamily.includes('Courier')
                            && style.fontSize === '17px') document.title = 'themed';
                }).observe(document.documentElement, { childList: true, subtree: true });
                setTimeout(() => dispatchEvent(new KeyboardEvent('keydown', {
                    key: 'f', bubbles: true, cancelable: true
                })), 300);
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("themed") << false << QStringList{};
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
                    const hint = document.querySelector('#__tanto_link_hints > span');
                    dispatchEvent(new KeyboardEvent('keydown', {
                        key: hint.textContent.toLowerCase(), bubbles: true, cancelable: true
                    }));
                };
                activateBackgroundHint();
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("background") << false << QStringList{};
    QTest::newRow("short hints remain selectable beside long hints")
        << QByteArray(R"HTML(<!doctype html><title>ready</title>
            <script>
                addEventListener('click', event => {
                    if (event.target.matches('a')) {
                        event.preventDefault();
                        document.title = event.target.id === 'first'
                            ? 'short-selected' : 'wrong-hint';
                    }
                }, true);
            </script>
            <a id="first" href="#first">First</a>
            <a href="#1">1</a><a href="#2">2</a><a href="#3">3</a>
            <a href="#4">4</a><a href="#5">5</a><a href="#6">6</a>
            <a href="#7">7</a><a href="#8">8</a><a href="#9">9</a>
            <a href="#10">10</a><a href="#11">11</a><a href="#12">12</a>
            <a href="#13">13</a><a href="#14">14</a><a href="#15">15</a>
            <a href="#16">16</a><a href="#17">17</a><a href="#18">18</a>
            <a href="#19">19</a><a href="#20">20</a><a href="#21">21</a>
            <a href="#22">22</a><a href="#23">23</a><a href="#24">24</a>
            <a href="#25">25</a><a href="#26">26</a>
            <script>
                setTimeout(() => {
                    dispatchEvent(new KeyboardEvent('keydown', {
                        key: 'f', bubbles: true, cancelable: true
                    }));
                    dispatchEvent(new KeyboardEvent('keydown', {
                        key: 'a', bubbles: true, cancelable: true
                    }));
                }, 300);
            </script>)HTML")
        << int(Qt::Key_unknown) << QStringLiteral("short-selected") << false << QStringList{};
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

void QtEngineContractTest::qtLinkHintsOwnSingleKeyShortcuts()
{
    QTemporaryDir root;
    QFile page(root.filePath(QStringLiteral("hint-shortcut.html")));
    QVERIFY(page.open(QIODevice::WriteOnly));
    QByteArray html("<!doctype html><title>ready</title><script>"
        "addEventListener('click',event=>{if(event.target.matches('a')){"
        "event.preventDefault();document.title='hint-selected';}},true);</script>");
    for (auto index = 0; index < 26; ++index) {
        html += "<a href='#" + QByteArray::number(index) + "'>link</a> ";
    }
    QCOMPARE(page.write(html), html.size());
    page.close();

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const QVariantMap configuration = {
        {QStringLiteral("version"), 1},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("bindings"), QVariantMap{
            {QStringLiteral("f"), QStringLiteral("open-link")},
        }},
        {QStringLiteral("passthroughAll"), false},
        {QStringLiteral("passthroughKeys"), QStringList{}},
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
    engine.rootContext()->setContextProperty(QStringLiteral("testedEngineView"), adapter.get());
    QQmlComponent shortcutComponent(&engine);
    shortcutComponent.setData(R"QML(
        import QtQuick
        Item {
            id: root
            property int activations: 0
            Shortcut {
                sequence: "m"
                context: Qt.WindowShortcut
                enabled: !testedEngineView.keyboardNavigationHintModeActive
                onActivated: root.activations++
            }
        }
    )QML", QUrl());
    const std::unique_ptr<QObject> shortcutHost(shortcutComponent.create());
    QVERIFY2(shortcutHost, qPrintable(shortcutComponent.errorString()));
    auto *hostItem = qobject_cast<QQuickItem *>(shortcutHost.get());
    QVERIFY(hostItem);
    hostItem->setParentItem(window.contentItem());
    hostItem->setSize(QSizeF(640, 480));
    view->setParentItem(hostItem);
    view->setSize(QSizeF(640, 480));
    window.show();
    window.requestActivate();
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "focusPage"));
    QTRY_VERIFY(adapter->property("pageHasFocus").toBool());
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("ready"));

    // A page's title is set as its <title> is parsed, which is well before the
    // navigation script is injected into the finished document — and nothing
    // the view exposes says when that script arrived. A key sent on the
    // strength of the title alone can reach a page with no handler for it, and
    // a key that lands nowhere is not sent again. So it is offered until it
    // lands: an f that reaches a page without hints does nothing, and the one
    // that opens them ends the loop.
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
        if (!adapter->property("keyboardNavigationHintModeActive").toBool()) {
            QTest::keyClick(&window, Qt::Key_F);
        }
        return adapter->property("keyboardNavigationHintModeActive").toBool();
    }()), 15000);
    QTest::keyClick(&window, Qt::Key_M);

    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("hint-selected"));
    QCOMPARE(shortcutHost->property("activations").toInt(), 0);
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
            {QStringLiteral("d"), QStringLiteral("scroll-half-page-down")},
            {QStringLiteral("u"), QStringLiteral("scroll-half-page-up")},
            {QStringLiteral("gg"), QStringLiteral("scroll-top")},
            {QStringLiteral("G"), QStringLiteral("scroll-bottom")},
            {QStringLiteral("f"), QStringLiteral("open-link")},
            {QStringLiteral("Shift+F"), QStringLiteral("open-link-background")},
        }},
        {QStringLiteral("passthroughAll"), passthroughAll},
        {QStringLiteral("passthroughKeys"), passthroughKeys},
        {QStringLiteral("hintTheme"), QVariantMap{
            {QStringLiteral("surface"), QStringLiteral("#123456")},
            {QStringLiteral("text"), QStringLiteral("#eeeeee")},
            {QStringLiteral("mutedText"), QStringLiteral("#999999")},
            {QStringLiteral("accent"), QStringLiteral("#654321")},
            {QStringLiteral("font"), QVariantMap{
                {QStringLiteral("family"), QStringLiteral("Courier")},
                {QStringLiteral("size"), 17},
            }},
        }},
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
    // Several of these pages drive themselves: they wait for the hint layer
    // and press their own keys as soon as the document is up. Such a page can
    // reach what the row is waiting for before this line ever samples the
    // title, and it never says "ready" again — so the page being up is what is
    // waited for here, not one particular thing the title says.
    QTRY_VERIFY(adapter->property("pageTitle").toString() == QStringLiteral("ready")
        || adapter->property("pageTitle").toString() == expectedTitle);
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


// One page served over HTTP, because cosmetic rules are written against a
// hostname and a file:// page has none.
namespace {

class PageServer final : public QTcpServer {
public:
    explicit PageServer(QByteArray body)
        : m_body(std::move(body))
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                const auto request = socket->readAll();
                // "GET /page.html?a=b HTTP/1.1" — the address as it left the
                // browser, which is the only place a rewritten one shows up.
                const auto fields = request.split(' ');
                if (fields.size() > 1) {
                    m_requested.append(QString::fromUtf8(fields.at(1)));
                }
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "
                    + QByteArray::number(m_body.size())
                    + "\r\nConnection: close\r\n\r\n" + m_body);
                socket->flush();
                socket->disconnectFromHost();
            });
        });
    }

    QStringList requested() const { return m_requested; }

private:
    QByteArray m_body;
    QStringList m_requested;
};

} // namespace

// The page reports what it can see the moment its own script runs, and again
// once the view has settled. A hiding rule that arrives after the page's own
// scripts is a rule the reader watched an ad flash through, so the first
// report is as much of the contract as the second.
void QtEngineContractTest::qtHidesCosmeticRulesBeforeThePageRuns()
{
    PageServer server(R"HTML(<!doctype html><html><body>
        <div id="specific" class="local-ad">ad</div>
        <div id="generic" class="generic-ad">ad</div>
        <div id="article" class="story">article</div>
        <script>
            const hidden = id =>
                getComputedStyle(document.getElementById(id)).display === "none";
            const state = () => (hidden("specific") ? "S" : "-")
                + (hidden("generic") ? "G" : "-") + (hidden("article") ? "A" : "-");
            const first = state();
            const report = () => {
                document.title = first + "|" + state();
                requestAnimationFrame(report);
            };
            report();
        </script>
    </body></html>)HTML");
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QTemporaryDir root;
    QVERIFY(root.isValid());
    tanto::ContentBlocker contentBlocker(
        root.path(), tanto::ContentBlocker::DefaultLists::None);
    contentBlocker.setUserRules(QStringLiteral("127.0.0.1##.local-ad\n##.generic-ad"));
    QTRY_VERIFY_WITH_TIMEOUT(!contentBlocker.compiling(), 5000);

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
        {QStringLiteral("contentBlocker"), QVariant::fromValue<QObject *>(&contentBlocker)},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));
    QQuickWindow window;
    qobject_cast<QQuickItem *>(adapter.get())->setParentItem(window.contentItem());
    window.show();

    const QUrl pageUrl(QStringLiteral("http://127.0.0.1:%1/page.html")
                           .arg(server.serverPort()));
    QVERIFY(adapter->setProperty("currentUrl", pageUrl));

    // The hostname rule is in the document before the page's own script runs;
    // the generic rule arrives with the survey, once there is a DOM to survey.
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("S--|SG-"), 15000);

    // Turning the site off gives both back without a reload, the surveyed
    // rules included.
    contentBlocker.setSiteEnabled(pageUrl, false);
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("S--|---"), 15000);
}


// A `##+js(...)` rule is worth something only if its scriptlet has already run
// when the page's own first script does: neutralising an anti-adblock check
// after the check ran changes nothing. The page reports what it saw, so the
// assertion is about ordering rather than about the value eventually arriving.
void QtEngineContractTest::qtRunsScriptletsBeforeThePageRuns()
{
    PageServer server(R"HTML(<!doctype html><html><body>
        <script>document.title = String(window.tantoScriptletRan);</script>
    </body></html>)HTML");
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QTemporaryDir root;
    QVERIFY(root.isValid());
    tanto::ContentBlocker contentBlocker(
        root.path(), tanto::ContentBlocker::DefaultLists::None);
    contentBlocker.setUserRules(
        QStringLiteral("127.0.0.1##+js(set-constant, tantoScriptletRan, true)"));
    QTRY_VERIFY_WITH_TIMEOUT(!contentBlocker.compiling(), 5000);

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
        {QStringLiteral("contentBlocker"), QVariant::fromValue<QObject *>(&contentBlocker)},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));
    QQuickWindow window;
    qobject_cast<QQuickItem *>(adapter.get())->setParentItem(window.contentItem());
    window.show();

    const QUrl pageUrl(QStringLiteral("http://127.0.0.1:%1/page.html")
                           .arg(server.serverPort()));
    QVERIFY(adapter->setProperty("currentUrl", pageUrl));

    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("true"), 15000);

    // A scriptlet is list-named code running in the page, so a site the user
    // turned blocking off for runs none of it.
    contentBlocker.setSiteEnabled(pageUrl, false);
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "reloadPage"));
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("undefined"), 15000);
}

// A page that asks for a tracker and is handed nothing waits forever, which is
// why the lists name a substitute for some of what they refuse. The page
// reports what it got: an image that loaded and measures one pixel came from
// the vendored library, and one that failed to load was simply refused.
void QtEngineContractTest::qtServesTheSubstitutesTheListsName()
{
    PageServer server(R"HTML(<!doctype html><html><body>
        <script>
            const load = (path, mark) => new Promise(resolve => {
                const image = new Image();
                image.onload = () => resolve(image.naturalWidth === 1 ? mark : "?");
                image.onerror = () => resolve("-");
                image.src = path;
            });
            Promise.all([
                load("/tracker.gif", "S"),
                load("/banner.gif", "B"),
            ]).then(marks => { document.title = marks.join(""); });
        </script>
    </body></html>)HTML");
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QTemporaryDir root;
    QVERIFY(root.isValid());
    tanto::ContentBlocker contentBlocker(
        root.path(), tanto::ContentBlocker::DefaultLists::None);
    contentBlocker.setUserRules(QStringLiteral(
        "/tracker.gif$image,redirect=1x1.gif\n"
        "/banner.gif$image"));
    QTRY_VERIFY_WITH_TIMEOUT(!contentBlocker.compiling(), 5000);
    tanto::QtContentBlocker engineContentBlocker(&contentBlocker);

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
        {QStringLiteral("contentBlocker"), QVariant::fromValue<QObject *>(&contentBlocker)},
        {QStringLiteral("engineContentBlocker"),
            QVariant::fromValue<QObject *>(&engineContentBlocker)},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));
    QQuickWindow window;
    qobject_cast<QQuickItem *>(adapter.get())->setParentItem(window.contentItem());
    window.show();

    QVERIFY(adapter->setProperty("currentUrl", QUrl(
        QStringLiteral("http://127.0.0.1:%1/page.html").arg(server.serverPort()))));

    // The substitute stood in for the first request; the second was refused
    // outright, because no rule named anything to put in its place.
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("S-"), 15000);

    // Neither image reached the site: the substitute came out of the vendored
    // library rather than off the network, and the refusal was a refusal.
    QVERIFY(!server.requested().contains(QStringLiteral("/tracker.gif")));
    QVERIFY(!server.requested().contains(QStringLiteral("/banner.gif")));
}

// A $removeparam rule refuses nothing. The request goes out, with the tracking
// parameters the rule names stripped off the address the site receives, so the
// assertion is about what arrived at the server rather than what the page saw.
// The frame is a navigation because that, a document and an XHR are the three
// kinds of request the option applies to when a rule names no type of its own.
void QtEngineContractTest::qtStripsTheParametersTheListsName()
{
    PageServer server(R"HTML(<!doctype html><html><body>
        <script>
            if (window.top === window) {
                const frame = document.createElement("iframe");
                frame.onload = () => { document.title = "asked"; };
                frame.src = "/frame.html?utm_source=ads&id=7";
                document.body.appendChild(frame);
            }
        </script>
    </body></html>)HTML");
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QTemporaryDir root;
    QVERIFY(root.isValid());
    tanto::ContentBlocker contentBlocker(
        root.path(), tanto::ContentBlocker::DefaultLists::None);
    contentBlocker.setUserRules(QStringLiteral("$removeparam=utm_source"));
    QTRY_VERIFY_WITH_TIMEOUT(!contentBlocker.compiling(), 5000);
    tanto::QtContentBlocker engineContentBlocker(&contentBlocker);

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
        {QStringLiteral("contentBlocker"), QVariant::fromValue<QObject *>(&contentBlocker)},
        {QStringLiteral("engineContentBlocker"),
            QVariant::fromValue<QObject *>(&engineContentBlocker)},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));
    QQuickWindow window;
    qobject_cast<QQuickItem *>(adapter.get())->setParentItem(window.contentItem());
    window.show();

    QVERIFY(adapter->setProperty("currentUrl", QUrl(
        QStringLiteral("http://127.0.0.1:%1/page.html").arg(server.serverPort()))));

    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("asked"), 15000);
    QVERIFY2(server.requested().contains(QStringLiteral("/frame.html?id=7")),
        qPrintable(server.requested().join(QStringLiteral(", "))));
}

// The lists' $popup rules decide which windows a page gets to open, a link
// asking for a new tab included: that is how an ad link opens. A background
// tab takes a middle- or ctrl-click, which is the user asking rather than the
// page, and a site the user turned blocking off for opens whatever it likes.
void QtEngineContractTest::qtRefusesTheWindowsTheListsNameAndNoOthers()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    tanto::ContentBlocker contentBlocker(
        root.path(), tanto::ContentBlocker::DefaultLists::None);
    contentBlocker.setUserRules(QStringLiteral("||popads.example^$popup"));
    QTRY_VERIFY_WITH_TIMEOUT(!contentBlocker.compiling(), 5000);

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
        {QStringLiteral("contentBlocker"), QVariant::fromValue<QObject *>(&contentBlocker)},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));
    const QUrl opener(QStringLiteral("https://site.example/article"));
    QVERIFY(adapter->setProperty("currentUrl", opener));

    const auto refused = [&adapter](QWebEngineNewWindowRequest::DestinationType destination,
                             const QString &url) {
        QVariant result;
        const auto invoked = QMetaObject::invokeMethod(adapter.get(), "popupRefused",
            Q_RETURN_ARG(QVariant, result),
            Q_ARG(QVariant, QVariant::fromValue(static_cast<int>(destination))),
            Q_ARG(QVariant, QVariant::fromValue(QUrl(url))));
        return invoked && result.toBool();
    };

    QVERIFY(refused(QWebEngineNewWindowRequest::InNewWindow,
        QStringLiteral("https://popads.example/win")));
    QVERIFY(refused(QWebEngineNewWindowRequest::InNewDialog,
        QStringLiteral("https://popads.example/win")));
    QVERIFY(refused(QWebEngineNewWindowRequest::InNewTab,
        QStringLiteral("https://popads.example/win")));
    QVERIFY(!refused(QWebEngineNewWindowRequest::InNewBackgroundTab,
        QStringLiteral("https://popads.example/win")));
    QVERIFY(!refused(QWebEngineNewWindowRequest::InNewDialog,
        QStringLiteral("https://pay.example/checkout")));

    contentBlocker.setSiteEnabled(opener, false);
    QVERIFY(!refused(QWebEngineNewWindowRequest::InNewWindow,
        QStringLiteral("https://popads.example/win")));
}

// QML's WebEngineProfile is QQuickWebEngineProfile, a different class from the
// QWebEngineProfile the widget API uses, and neither derives from the other.
// Attaching to only one of them leaves every request on a Space's profile
// unintercepted while Settings still reports the rules the lists contributed —
// blocking that says it is on and is not. The return value says so, and the
// QML that calls this discards it, so the assertion belongs here.
void QtEngineContractTest::qtAttachesBlockingToTheProfileQmlCreates()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    tanto::ContentBlocker contentBlocker(
        root.path(), tanto::ContentBlocker::DefaultLists::None);
    tanto::QtContentBlocker engineContentBlocker(&contentBlocker);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtWebEngine\nWebEngineProfile { storageName: \"tanto-attach\" }",
        QUrl());
    const std::unique_ptr<QObject> profile(component.create());
    QVERIFY2(profile, qPrintable(component.errorString()));
    QVERIFY(engineContentBlocker.attachToProfile(profile.get()));

    QObject notAProfile;
    QVERIFY(!engineContentBlocker.attachToProfile(&notAProfile));
}

void QtEngineContractTest::adaptersNameTheColoursTheirInspectorIsDrawnIn_data()
{
    adaptersExposeSharedContract_data();
}

// Every adapter that reports the capability takes a palette for its inspector
// and hands the inspector back attached to one tab at a time. The shell gives
// the same palette to whichever engine is running, so neither one is allowed to
// leave the inspector looking like something other than this window.
void QtEngineContractTest::adaptersNameTheColoursTheirInspectorIsDrawnIn()
{
    QFETCH(QString, path);
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(path));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));
    QVERIFY(adapter->property("capabilities").toInt() & EngineCapabilities::DeveloperTools);

    QVERIFY(adapter->setProperty("developerToolsColors", inspectorPalette()));
    QVERIFY(!adapter->property("developerToolsAttached").toBool());
    QVERIFY(adapter->property("developerToolsView").value<QObject *>() == nullptr);

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "attachDeveloperTools"));
    QVERIFY(adapter->property("developerToolsAttached").toBool());
    auto *first = adapter->property("developerToolsView").value<QObject *>();
    QVERIFY(first);

    // Asking twice does not build a second inspector.
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "attachDeveloperTools"));
    QCOMPARE(adapter->property("developerToolsView").value<QObject *>(), first);

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "detachDeveloperTools"));
    QVERIFY(!adapter->property("developerToolsAttached").toBool());
    QVERIFY(adapter->property("developerToolsView").value<QObject *>() == nullptr);
}

// Chromium's inspector is a webpage whose whole design system is custom
// properties on its own root element, so Tanto names them again rather than
// patching the frontend or speaking a protocol to it. The assertion is what the
// frontend actually computes, because a sheet that lost the cascade would look
// exactly like one that was never injected.
void QtEngineContractTest::qtDocksAnInspectorDrawnInTantosColours()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
        {QStringLiteral("developerToolsColors"), inspectorPalette()},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));

    auto *view = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(view);
    QQuickWindow window;
    window.resize(1200, 800);
    view->setParentItem(window.contentItem());
    view->setSize(QSizeF(700, 800));
    window.show();

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Inspected</title><p id=target>inspect me</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Inspected"));

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "attachDeveloperTools"));
    auto *tools = adapter->property("developerToolsView").value<QObject *>();
    QVERIFY(tools);
    auto *toolsItem = qobject_cast<QQuickItem *>(tools);
    QVERIFY(toolsItem);
    // The shell docks the inspector beside the page; here the test window
    // stands in for it.
    toolsItem->setParentItem(window.contentItem());
    toolsItem->setPosition(QPointF(700, 0));
    toolsItem->setSize(QSizeF(500, 800));

    // The frontend is served from Chromium's own scheme out of the engine's
    // resources, so this is also the check that this build ships one.
    QTRY_VERIFY_WITH_TIMEOUT(
        tools->property("url").toUrl().scheme() == QLatin1String("devtools"), 20000);
    QTRY_VERIFY_WITH_TIMEOUT(!tools->property("loading").toBool(), 20000);

    // What the frontend resolves for one of its own tokens. The frontend owns
    // its own document title and rewrites it as the inspected page reports one,
    // so the answer comes back over the console instead: nothing else writes to
    // it under this prefix, and nothing overwrites what was already said.
    QMetaMethod consoleSignal;
    for (int index = 0; index < tools->metaObject()->methodCount(); ++index) {
        const auto method = tools->metaObject()->method(index);
        if (method.name() == "javaScriptConsoleMessage") {
            consoleSignal = method;
            break;
        }
    }
    QVERIFY(consoleSignal.isValid());
    QSignalSpy consoleSpy(tools, consoleSignal);
    QVERIFY(consoleSpy.isValid());

    const auto reports = [&](const QString &expression, const QString &expected) {
        const auto script = QStringLiteral("console.log('tanto-token:' + (%1))")
            .arg(expression);
        const auto wanted = QStringLiteral("tanto-token:") + expected;
        consoleSpy.clear();
        for (int attempt = 0; attempt < 40; ++attempt) {
            QMetaObject::invokeMethod(tools, "runJavaScript", Q_ARG(QString, script));
            QTest::qWait(250);
            for (const auto &message : consoleSpy) {
                if (message.at(1).toString() == wanted) {
                    return true;
                }
            }
        }
        return false;
    };
    const auto resolves = [&](const char *token, const QString &colour) {
        return reports(QStringLiteral("getComputedStyle(document.documentElement)"
                                      ".getPropertyValue('%1').trim()")
                .arg(QString::fromLatin1(token)),
            colour);
    };

    QVERIFY(resolves("--sys-color-token-keyword", QStringLiteral("#123456")));
    QVERIFY(resolves("--sys-color-token-string", QStringLiteral("#654321")));
    QVERIFY(resolves("--sys-color-token-comment", QStringLiteral("#fedcba")));
    // An attribute's value is a string, and reads as one. The theme names no
    // separate colour for it, and the inspector must not invent one.
    QVERIFY(resolves("--sys-color-token-attribute-value", QStringLiteral("#654321")));
    QVERIFY(resolves("--sys-color-token-attribute", QStringLiteral("#040506")));
    QVERIFY(resolves("--sys-color-token-tag", QStringLiteral("#010203")));
    QVERIFY(resolves("--sys-color-cdt-base-container", QStringLiteral("#0b1a0b")));
    QVERIFY(resolves("--sys-color-primary", QStringLiteral("#00ff88")));

    // Chromium themes its inspector through the tonal ramp underneath the
    // tokens, which is served from the browser's own UI theme, so a token
    // Tanto never names still has to land in Tanto's palette rather than in
    // Chrome's. `--sys-color-purple` is one Tanto says nothing about: it is a
    // tone of the ramp, and the ramp is the theme's.
    QVERIFY(!resolves("--ref-palette-primary80", QStringLiteral("#a8c7fa")));
    QVERIFY(resolves("--ref-palette-neutral10", QStringLiteral("#171c17")));
    QVERIFY(resolves("--sys-color-purple", QStringLiteral("#abcced")));

    // The inspector's own interface takes the theme's type, not the platform's:
    // Tanto's chrome is drawn in one family and the dock is part of it.
    QVERIFY(reports(QStringLiteral(
        "getComputedStyle(document.documentElement)"
        ".getPropertyValue('--default-font-family').trim()"
        ".includes('Menlo')"), QStringLiteral("true")));
    // The frontend has a light face and a dark one, and Tanto's own window
    // colour is what decides which of the two this is.
    QVERIFY(reports(QStringLiteral("document.documentElement.classList"
                                   ".contains('theme-with-dark-background')"),
        QStringLiteral("true")));

    // A live theme change reaches an inspector that is already open.
    auto palette = inspectorPalette();
    auto syntax = palette.value(QStringLiteral("syntax")).toMap();
    syntax.insert(QStringLiteral("keyword"), QStringLiteral("#ff00ff"));
    palette.insert(QStringLiteral("syntax"), syntax);
    QVERIFY(adapter->setProperty("developerToolsColors", palette));
    QVERIFY(resolves("--sys-color-token-keyword", QStringLiteral("#ff00ff")));

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "detachDeveloperTools"));
    QVERIFY(!adapter->property("developerToolsAttached").toBool());
    QVERIFY(adapter->property("developerToolsView").value<QObject *>() == nullptr);
}

// An inspected tab keeps running while another tab or Space is on show, and
// stops being exempt the moment Developer tools go. Qt is what enforces this:
// a hidden view is ordinarily recommended for freezing and accepts it, and one
// with an inspector attached is recommended Active and refuses. So this is a
// test of the engine contract Tanto is relying on rather than of Tanto's code —
// and it is the thing that would break silently if Qt changed its mind.
void QtEngineContractTest::qtKeepsAnInspectedTabActiveOnlyWhileAttached()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(item);
    QQuickWindow window;
    window.resize(800, 600);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(800, 600));
    window.show();

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Retained</title><p>retained</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Retained"));

    auto *webView = adapter->findChild<QObject *>(QStringLiteral("qtWebView"));
    QVERIFY(webView);
    const auto activeState = webView->property("lifecycleState");
    QVERIFY(activeState.isValid());
    const auto frozenState = QVariant::fromValue(activeState.toInt() + 1);

    // Hidden and uninspected: Qt says this page may be frozen, and freezes it.
    webView->setProperty("visible", false);
    QTRY_COMPARE(webView->property("recommendedState"), frozenState);
    QVERIFY(webView->setProperty("lifecycleState", frozenState));
    QCOMPARE(webView->property("lifecycleState"), frozenState);

    // Attached, and still hidden: Qt says Active and refuses anything less.
    QVERIFY(webView->setProperty("lifecycleState", activeState));
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "attachDeveloperTools"));
    QTRY_COMPARE(webView->property("recommendedState"), activeState);
    webView->setProperty("lifecycleState", frozenState);
    QCOMPARE(webView->property("lifecycleState"), activeState);

    // Detached: the exception goes with the inspector.
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "detachDeveloperTools"));
    QTRY_COMPARE(webView->property("recommendedState"), frozenState);
}

// A Private window's pages run in one temporary off-the-record profile, and the
// inspector is a page: it runs in the same profile, so what it stores about the
// pages it inspected goes when the Private session does.
void QtEngineContractTest::qtInspectsAPrivateTabInItsOwnTemporaryProfile()
{
    QQmlEngine engine;
    QQmlComponent profileComponent(&engine);
    profileComponent.setData(
        "import QtWebEngine\nWebEngineProfile { offTheRecord: true }", QUrl());
    const std::unique_ptr<QObject> privateProfile(profileComponent.create());
    QVERIFY2(privateProfile, qPrintable(profileComponent.errorString()));
    QVERIFY(privateProfile->property("offTheRecord").toBool());

    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("sharedProfile"), QVariant::fromValue(privateProfile.get())},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "attachDeveloperTools"));
    auto *tools = adapter->property("developerToolsView").value<QObject *>();
    QVERIFY(tools);
    // The same profile object the inspected page runs in, not a profile of the
    // inspector's own and not the default one.
    QCOMPARE(tools->property("profile").value<QObject *>(), privateProfile.get());
    QCOMPARE(adapter->property("browserProfile").value<QObject *>(), privateProfile.get());
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "detachDeveloperTools"));
}

// Chromium's InspectElement reads the node a context menu was opened over and
// dereferences it without checking, so asking for it from the keyboard — where
// no menu has been opened — used to take the whole browser down. This test
// crashes the suite if that comes back.
void QtEngineContractTest::qtPicksAnElementWhenNoContextMenuNamedOne()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(item);
    QQuickWindow window;
    window.resize(1200, 800);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(700, 800));
    window.show();

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Pick me</title><p id=target>pick me</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Pick me"));
    QVERIFY(!adapter->property("contextMenuTargetKnown").toBool());

    // The keyboard route: the inspector opens, and the frontend's own element
    // picker is asked for once it has loaded, because nothing named a node.
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "inspectElement"));
    QVERIFY(adapter->property("developerToolsAttached").toBool());
    auto *tools = adapter->property("developerToolsView").value<QObject *>();
    QVERIFY(tools);
    auto *toolsItem = qobject_cast<QQuickItem *>(tools);
    QVERIFY(toolsItem);
    toolsItem->setParentItem(window.contentItem());
    toolsItem->setPosition(QPointF(700, 0));
    toolsItem->setSize(QSizeF(500, 800));
    QTRY_VERIFY_WITH_TIMEOUT(!adapter->property("elementPickPending").toBool(), 20000);

    // A context menu names a node, and then the action that reads it is the one
    // that runs. Navigating away takes the node with it: what sits at those
    // coordinates on the next page is not what the reader pointed at.
    auto *webView = adapter->findChild<QObject *>(QStringLiteral("qtWebView"));
    QVERIFY(webView);
    QMetaMethod contextMenuSignal;
    for (int index = 0; index < webView->metaObject()->methodCount(); ++index) {
        const auto method = webView->metaObject()->method(index);
        if (method.name() == "contextMenuRequested") {
            contextMenuSignal = method;
            break;
        }
    }
    QVERIFY(contextMenuSignal.isValid());
    void *request = nullptr;
    QVERIFY(contextMenuSignal.invoke(webView, Qt::DirectConnection,
        QGenericArgument(contextMenuSignal.parameterMetaType(0).name(), &request)));
    QVERIFY(adapter->property("contextMenuTargetKnown").toBool());

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Elsewhere</title><p>elsewhere</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Elsewhere"));
    QVERIFY(!adapter->property("contextMenuTargetKnown").toBool());

    // And asking again on the page that named nothing still opens the picker
    // rather than reading a node Chromium no longer has.
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "inspectElement"));
    QVERIFY(adapter->property("developerToolsAttached").toBool());
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "detachDeveloperTools"));
}

// An editor keeps a document's structure quiet and lets its content speak: the
// brackets, the equals signs and the quotes recede, and the names between them
// carry the colour. The frontend's DOM tree draws all of it in one token,
// because the name is a span inside the `<...>` that token wraps, so no design
// token can tell them apart — only a rule can, and the spans live in shadow
// trees a rule in the document cannot reach. This is the test that the way in
// still works: the frontend's own `attachShadow` is wrapped before its scripts
// run, and every tree it opens takes one more stylesheet.
void QtEngineContractTest::qtDrawsMarkupStructureQuieterThanItsContent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
        {QStringLiteral("developerToolsColors"), inspectorPalette()},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(item);
    QQuickWindow window;
    window.resize(1300, 800);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(700, 800));
    window.show();

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Markup</title><p id=target class=x>read me</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Markup"));

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "attachDeveloperTools"));
    auto *tools = adapter->property("developerToolsView").value<QObject *>();
    QVERIFY(tools);
    auto *toolsItem = qobject_cast<QQuickItem *>(tools);
    QVERIFY(toolsItem);
    toolsItem->setParentItem(window.contentItem());
    toolsItem->setPosition(QPointF(700, 0));
    toolsItem->setSize(QSizeF(600, 800));
    QTRY_VERIFY_WITH_TIMEOUT(!tools->property("loading").toBool(), 20000);

    QMetaMethod consoleSignal;
    for (int index = 0; index < tools->metaObject()->methodCount(); ++index) {
        const auto method = tools->metaObject()->method(index);
        if (method.name() == "javaScriptConsoleMessage") {
            consoleSignal = method;
            break;
        }
    }
    QVERIFY(consoleSignal.isValid());
    QSignalSpy consoleSpy(tools, consoleSignal);
    QVERIFY(consoleSpy.isValid());

    // The tree is built inside shadow roots, so the walk follows them down.
    const auto script = QStringLiteral(R"JS(
(() => {
  const seen = new Set();
  const walk = (node, depth) => {
    if (depth > 40) return;
    for (const element of node.querySelectorAll('*')) {
      const classes = element.classList;
      if (classes && classes.contains('webkit-html-tag-name'))
        seen.add('name=' + getComputedStyle(element).color);
      else if (classes && classes.contains('webkit-html-tag'))
        seen.add('punctuation=' + getComputedStyle(element).color);
      if (element.shadowRoot) walk(element.shadowRoot, depth + 1);
    }
  };
  walk(document, 0);
  console.log('tanto-markup:' + [...seen].join(' '));
})()
)JS");

    QString report;
    for (int attempt = 0; attempt < 40 && report.isEmpty(); ++attempt) {
        QMetaObject::invokeMethod(tools, "runJavaScript", Q_ARG(QString, script));
        QTest::qWait(250);
        for (const auto &message : consoleSpy) {
            const auto text = message.at(1).toString();
            if (text.startsWith(QLatin1String("tanto-markup:"))
                && text.contains(QLatin1String("name="))) {
                report = text;
            }
        }
    }
    QVERIFY2(!report.isEmpty(), "the markup tree never rendered");
    // The name carries the theme's tag colour; everything structural around it
    // carries the quieter one the theme names for punctuation.
    QVERIFY2(report.contains(QLatin1String("name=rgb(1, 2, 3)")), qPrintable(report));
    QVERIFY2(report.contains(QLatin1String("punctuation=rgb(119, 136, 153)")),
        qPrintable(report));

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "detachDeveloperTools"));
}

// Tanto draws the page's context menu, so the engine must report what was
// under the pointer as plain values and then draw nothing itself. Accepting
// the request is what does the second half, and a menu appearing over Tanto's
// own is exactly what this catches.
void QtEngineContractTest::qtReportsThePageContextAndDrawsNoMenuOfItsOwn()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(item);
    QQuickWindow window;
    window.resize(800, 600);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(800, 600));
    window.show();

    QSignalSpy contextSpy(adapter.get(), SIGNAL(pageContextRequested(QVariant)));
    QVERIFY(contextSpy.isValid());
    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Context</title>"
        "<a id=link href='https://example.com/target' "
        "style='position:absolute;top:0;left:0;font-size:40px'>a link</a>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Context"));
    QTest::qWait(500);

    QTest::mouseClick(&window, Qt::RightButton, {}, QPoint(40, 20));
    QTRY_VERIFY_WITH_TIMEOUT(contextSpy.count() > 0, 10000);
    const auto context = contextSpy.first().first().toMap();
    QCOMPARE(context.value(QStringLiteral("linkUrl")).toUrl(),
        QUrl(QStringLiteral("https://example.com/target")));
    QCOMPARE(context.value(QStringLiteral("mediaType")).toString(), QStringLiteral("none"));
    QVERIFY(!context.value(QStringLiteral("editable")).toBool());
    // The position is the engine's own, for the shell to place its menu at.
    QVERIFY(context.value(QStringLiteral("x")).toInt() > 0);

    // Nothing of the engine's own is left standing over the page.
    int menuRows = 0;
    for (auto *top : QGuiApplication::allWindows()) {
        for (auto *node : top->findChildren<QObject *>()) {
            if (QString::fromLatin1(node->metaObject()->className())
                    .contains(QLatin1String("MenuItem"))) {
                ++menuRows;
            }
        }
    }
    QCOMPARE(menuRows, 0);

    // Chromium is holding the node now, so Inspect element can read it back.
    QVERIFY(adapter->property("contextMenuTargetKnown").toBool());
}

void QtEngineContractTest::qtReportsTargetsInsideCrossOriginFrames()
{
    PageServer frameServer(R"HTML(<!doctype html><style>body{margin:0}</style>
        <a href="https://target.example/from-frame"
           style="display:block;width:240px;height:80px;font-size:24px">frame link</a>
        <script>document.querySelector('a').focus()</script>)HTML");
    QVERIFY(frameServer.listen(QHostAddress::LocalHost));
    PageServer pageServer(QStringLiteral(
        "<!doctype html><title>Cross-origin frame</title>"
        "<style>body{margin:0}iframe{border:0;width:300px;height:120px}</style>"
        "<iframe src=\"http://127.0.0.1:%1/frame.html\"></iframe>")
                              .arg(frameServer.serverPort())
                              .toUtf8());
    QVERIFY(pageServer.listen(QHostAddress::LocalHost));

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(item);
    QQuickWindow window;
    window.resize(640, 480);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(640, 480));
    window.show();

    QSignalSpy contextSpy(adapter.get(), SIGNAL(pageContextRequested(QVariant)));
    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "http://127.0.0.1:%1/page.html").arg(pageServer.serverPort()))));
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("Cross-origin frame"), 10000);
    // A click is a one-shot event. Sent before the frame inside the page is up,
    // it lands on a document that has no link to report, and no amount of
    // waiting afterwards brings the gesture back — the count simply stays at
    // zero until the test times out. So the gesture is repeated until the page
    // answers for the frame, which is what a reader does when a click seems not
    // to have landed, rather than the test guessing at how long a frame takes.
    const auto rightClickReportsTheFrameLink = [&window, &contextSpy] {
        QTest::mouseClick(&window, Qt::RightButton, {}, QPoint(40, 30));
        QTest::qWait(200);
        return contextSpy.count() > 0
            && contextSpy.last().first().toMap()
                    .value(QStringLiteral("linkUrl")).toUrl()
                == QUrl(QStringLiteral("https://target.example/from-frame"));
    };
    QTRY_VERIFY_WITH_TIMEOUT(rightClickReportsTheFrameLink(), 10000);

    contextSpy.clear();
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "requestPageContextMenu"));
    QTRY_VERIFY_WITH_TIMEOUT(contextSpy.count() > 0, 10000);
    QCOMPARE(contextSpy.last().first().toMap().value(QStringLiteral("linkUrl")).toUrl(),
        QUrl(QStringLiteral("https://target.example/from-frame")));
}

void QtEngineContractTest::qtOwnsJavaScriptPromptsAndReturnsTheirAnswer()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(item);
    QQuickWindow window;
    window.resize(640, 480);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(640, 480));
    window.show();

    QSignalSpy promptSpy(adapter.get(), SIGNAL(browserPromptRequested(QString,QVariant)));
    QVERIFY(promptSpy.isValid());
    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>waiting</title><script>setTimeout(()=>{"
        "document.title=prompt('Name','suggested')===null?'cancelled':'accepted'},0)"
        "</script>"))));
    QTRY_VERIFY_WITH_TIMEOUT(promptSpy.count() > 0, 10000);
    const auto request = promptSpy.takeFirst();
    const auto prompt = request.at(1).toMap();
    QCOMPARE(prompt.value(QStringLiteral("kind")).toString(),
        QStringLiteral("javascript-prompt"));
    QCOMPARE(prompt.value(QStringLiteral("defaultText")).toString(),
        QStringLiteral("suggested"));
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "respondToBrowserPrompt",
        Q_ARG(QVariant, request.at(0)), Q_ARG(QVariant, false),
        Q_ARG(QVariant, QVariantMap{})));
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("cancelled"), 10000);
}

void QtEngineContractTest::qtOpensOneLocalFileWithoutDirectoryWideAccess()
{
    QTemporaryDir root;
    QFile sibling(root.filePath(QStringLiteral("sibling.txt")));
    QVERIFY(sibling.open(QIODevice::WriteOnly));
    sibling.write("not granted");
    sibling.close();
    QFile page(root.filePath(QStringLiteral("chosen.html")));
    QVERIFY(page.open(QIODevice::WriteOnly));
    page.write(R"HTML(<!doctype html><title>waiting</title><script>
        fetch('sibling.txt').then(() => document.title = 'wide access')
            .catch(() => document.title = 'isolated');
    </script>)HTML");
    page.close();

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(item);
    QQuickWindow window;
    window.resize(640, 480);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(640, 480));
    window.show();

    QVERIFY(adapter->setProperty("currentUrl", QUrl::fromLocalFile(page.fileName())));
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("isolated"), 10000);
}

void QtEngineContractTest::adaptersAnswerForEveryEverydayPageOperation_data()
{
    adaptersExposeSharedContract_data();
}

// Every adapter answers for the everyday page operations one way or the other.
// The contract above already requires the properties and the operations; this
// is the capability beside them, which is what tells the shell whether a
// command can run here or has to say that it cannot.
void QtEngineContractTest::adaptersAnswerForEveryEverydayPageOperation()
{
    QFETCH(QString, path);
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(path));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));

    const auto capabilities = adapter->property("capabilities").toInt();
    QVERIFY(capabilities & EngineCapabilities::PageFind);
    QVERIFY(capabilities & EngineCapabilities::PageZoom);
    QVERIFY(capabilities & EngineCapabilities::Printing);
    QVERIFY(capabilities & EngineCapabilities::SiteFullscreen);
    QVERIFY(capabilities & EngineCapabilities::InlinePdfViewing);

    // Every tab starts at 100 percent and searching for nothing.
    QCOMPARE(adapter->property("zoomFactor").toDouble(), 1.0);
    QCOMPARE(adapter->property("findQuery").toString(), QString{});
    QCOMPARE(adapter->property("findMatchCount").toInt(), 0);
    QVERIFY(!adapter->property("siteFullscreenActive").toBool());
}

// Find belongs to the tab: the query and the match position live on the
// adapter, a navigation takes the matches and leaves the query, and clearing
// takes both.
void QtEngineContractTest::qtFindsInThePageAndKeepsTheQueryAcrossNavigation()
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

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Findable</title>"
        "<p>alpha beta alpha gamma alpha</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Findable"));
    // A title arrives before the document is finished, and a search is answered
    // once against whatever is there when it is asked: a query issued mid-load
    // is told nothing was found and is never run again. That is the contract —
    // matches belong to the document that was searched — so the test waits for
    // the page rather than searching one that is still arriving.
    QTRY_VERIFY(!adapter->property("loading").toBool());

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "findText",
        Q_ARG(QVariant, QStringLiteral("alpha")), Q_ARG(QVariant, true)));
    QCOMPARE(adapter->property("findQuery").toString(), QStringLiteral("alpha"));
    QTRY_COMPARE(adapter->property("findMatchCount").toInt(), 3);
    QCOMPARE(adapter->property("findActiveMatch").toInt(), 1);

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "findText",
        Q_ARG(QVariant, QStringLiteral("alpha")), Q_ARG(QVariant, true)));
    QTRY_COMPARE(adapter->property("findActiveMatch").toInt(), 2);

    // The matches were in the page being replaced; the query is the reader's.
    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Elsewhere</title><p>nothing here</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Elsewhere"));
    QTRY_VERIFY(!adapter->property("loading").toBool());
    QCOMPARE(adapter->property("findMatchCount").toInt(), 0);
    QCOMPARE(adapter->property("findQuery").toString(), QStringLiteral("alpha"));

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "clearFind"));
    QCOMPARE(adapter->property("findQuery").toString(), QString{});
    QCOMPARE(adapter->property("findMatchCount").toInt(), 0);
}

// Zoom is the tab's rather than the page's: it is set once and every page the
// tab goes on to show is drawn at it.
void QtEngineContractTest::qtKeepsTheZoomItIsGivenAcrossNavigation()
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

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Zoomed</title><p>page</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Zoomed"));

    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "setZoomFactor",
        Q_ARG(QVariant, 1.5)));
    QCOMPARE(adapter->property("zoomFactor").toDouble(), 1.5);

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Still zoomed</title><p>next</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Still zoomed"));
    QCOMPARE(adapter->property("zoomFactor").toDouble(), 1.5);

    // A factor that is not a size is not a zoom.
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "setZoomFactor", Q_ARG(QVariant, 0)));
    QCOMPARE(adapter->property("zoomFactor").toDouble(), 1.5);
}

namespace {

// A page whose one subresource is cacheable and reports how many times it has
// actually been fetched. Normal reload keeps the cached copy; reload bypassing
// cache does not, which is the whole difference between the two commands.
class CountingServer final : public QTcpServer {
public:
    CountingServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                const auto request = socket->readAll();
                const auto fields = request.split(' ');
                const auto path = fields.size() > 1 ? fields.at(1) : QByteArray();
                QByteArray headers;
                QByteArray body;
                if (path.startsWith("/counter.js")) {
                    ++m_scriptRequests;
                    body = "document.title = '" + QByteArray::number(m_scriptRequests) + "';";
                    headers = "Content-Type: application/javascript\r\n"
                              "Cache-Control: max-age=600\r\n";
                } else if (path.startsWith("/slow")) {
                    // Answered by nothing at all: a load to stop.
                    return;
                } else {
                    body = "<!doctype html><title>waiting</title>"
                           "<script src=\"/counter.js\"></script>";
                    headers = "Content-Type: text/html\r\nCache-Control: no-store\r\n";
                }
                socket->write("HTTP/1.1 200 OK\r\n" + headers + "Content-Length: "
                    + QByteArray::number(body.size())
                    + "\r\nConnection: close\r\n\r\n" + body);
                socket->flush();
                socket->disconnectFromHost();
            });
        });
    }

    int scriptRequests() const { return m_scriptRequests; }

private:
    int m_scriptRequests = 0;
};

} // namespace

void QtEngineContractTest::qtSeparatesReloadBypassingCacheFromReloadAndStop()
{
    CountingServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QTemporaryDir root;
    QVERIFY(root.isValid());
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));

    auto *view = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(view);
    QQuickWindow window;
    window.resize(640, 480);
    view->setParentItem(window.contentItem());
    view->setSize(QSizeF(640, 480));
    window.show();

    const QUrl pageUrl(QStringLiteral("http://127.0.0.1:%1/page.html")
                           .arg(server.serverPort()));
    QVERIFY(adapter->setProperty("currentUrl", pageUrl));
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("1"), 15000);

    // Reading the page again keeps what the cache already holds.
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "reloadPage"));
    QTRY_VERIFY_WITH_TIMEOUT(!adapter->property("loading").toBool(), 15000);
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("1"), 15000);
    QCOMPARE(server.scriptRequests(), 1);

    // Reading it again from the network does not.
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "reloadPageBypassingCache"));
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("2"), 15000);
    QCOMPARE(server.scriptRequests(), 2);

    // Stopping ends the load and leaves the page that was there standing.
    const QUrl slowUrl(QStringLiteral("http://127.0.0.1:%1/slow").arg(server.serverPort()));
    QVERIFY(adapter->setProperty("currentUrl", slowUrl));
    QTRY_VERIFY_WITH_TIMEOUT(adapter->property("loading").toBool(), 15000);
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "stopLoading"));
    QTRY_VERIFY_WITH_TIMEOUT(!adapter->property("loading").toBool(), 15000);
    QCOMPARE(adapter->property("pageTitle").toString(), QStringLiteral("2"));
}

// The adapter renders the page into a PDF for the platform's print dialog to
// present, and draws a PDF in Chromium's own sandboxed viewer rather than
// handing it to the download stack.
void QtEngineContractTest::qtRendersAPageForPrintingAndDrawsPdfsInline()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
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

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Printable</title><h1>Invoice</h1>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Printable"));

    QSignalSpy printSpy(adapter.get(), SIGNAL(printFinished(QString,bool)));
    QVERIFY(printSpy.isValid());

    // A render with nowhere to go is reported rather than attempted.
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "printPage", Q_ARG(QVariant, QString{})));
    QCOMPARE(printSpy.count(), 1);
    QVERIFY(!printSpy.takeFirst().at(1).toBool());

    const auto destination = root.filePath(QStringLiteral("page.pdf"));
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "printPage",
        Q_ARG(QVariant, destination)));
    QTRY_VERIFY_WITH_TIMEOUT(printSpy.count() == 1, 15000);
    const auto rendered = printSpy.takeFirst();
    QCOMPARE(rendered.at(0).toString(), destination);
    QVERIFY(rendered.at(1).toBool());

    QFile pdf(destination);
    QVERIFY(pdf.open(QIODevice::ReadOnly));
    QVERIFY(pdf.read(4).startsWith("%PDF"));
    pdf.close();

    // And the same document opens inside the engine rather than downloading.
    auto *webView = adapter->findChild<QObject *>(QStringLiteral("qtWebView"));
    QVERIFY(webView);
    const auto settings = webView->property("settings").value<QObject *>();
    QVERIFY(settings);
    QVERIFY(settings->property("pdfViewerEnabled").toBool());

    // Zoom reaches the document as it reaches a page: the viewer is drawn
    // inside the view, not in an application of its own.
    QVERIFY(adapter->setProperty("currentUrl", QUrl::fromLocalFile(destination)));
    QTRY_VERIFY_WITH_TIMEOUT(!adapter->property("loading").toBool(), 15000);
    QCOMPARE(adapter->property("currentUrl").toUrl(), QUrl::fromLocalFile(destination));
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "setZoomFactor", Q_ARG(QVariant, 1.5)));
    QCOMPARE(adapter->property("zoomFactor").toDouble(), 1.5);
}

// A page asking for the screen is the one path the shell cannot drive itself,
// and the adapter is what turns the engine's request into a state Tanto is in.
// Chromium refuses a fullscreen request that no gesture asked for, so the page
// asks from a key press.
void QtEngineContractTest::qtReportsSiteFullscreenWithItsOrigin()
{
    PageServer server(R"HTML(<!doctype html><html><body>
        <script>
            document.title = "ready";
            document.addEventListener("keydown", () => {
                if (document.fullscreenElement) document.exitFullscreen();
                else document.documentElement.requestFullscreen();
            });
            document.addEventListener("fullscreenchange", () => {
                document.title = document.fullscreenElement ? "full" : "windowed";
            });
        </script>
    </body></html>)HTML");
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QTemporaryDir root;
    QVERIFY(root.isValid());
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
    }));
    QVERIFY2(adapter, qPrintable(component.errorString()));

    auto *view = qobject_cast<QQuickItem *>(adapter.get());
    QVERIFY(view);
    QQuickWindow window;
    window.resize(640, 480);
    view->setParentItem(window.contentItem());
    view->setSize(QSizeF(640, 480));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.requestActivate();

    QVERIFY(adapter->setProperty("currentUrl",
        QUrl(QStringLiteral("http://127.0.0.1:%1/page.html").arg(server.serverPort()))));
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("ready"), 15000);
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "focusPage"));
    QTRY_VERIFY(adapter->property("pageHasFocus").toBool());

    QVERIFY(!adapter->property("siteFullscreenActive").toBool());

    // A page's title is set as its <title> is parsed, well before its scripts
    // have run, and a key that lands on a page with no handler for it is not
    // sent again. So it is offered until it lands.
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
        if (!adapter->property("siteFullscreenActive").toBool()) {
            QTest::keyClick(&window, Qt::Key_F);
        }
        return adapter->property("siteFullscreenActive").toBool();
    }()), 15000);

    // The shell hears that a page took the screen, and whose page it was.
    QVERIFY(adapter->property("siteFullscreenActive").toBool());
    QCOMPARE(adapter->property("siteFullscreenOrigin").toString(),
        QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("full"), 15000);

    // A page that gives the screen back of its own accord is the same request
    // the other way, and the shell has to leave the state with it.
    QTest::keyClick(&window, Qt::Key_F);
    QTRY_VERIFY_WITH_TIMEOUT(!adapter->property("siteFullscreenActive").toBool(), 15000);
    QCOMPARE(adapter->property("siteFullscreenOrigin").toString(), QString{});
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("windowed"), 15000);

    // And the page is told when the reader takes it back instead.
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
        if (!adapter->property("siteFullscreenActive").toBool()) {
            QTest::keyClick(&window, Qt::Key_F);
        }
        return adapter->property("siteFullscreenActive").toBool();
    }()), 15000);
    QVERIFY(QMetaObject::invokeMethod(adapter.get(), "exitSiteFullscreen"));
    QVERIFY(!adapter->property("siteFullscreenActive").toBool());
    QCOMPARE(adapter->property("siteFullscreenOrigin").toString(), QString{});
    QTRY_COMPARE_WITH_TIMEOUT(adapter->property("pageTitle").toString(),
        QStringLiteral("windowed"), 15000);
}

void QtEngineContractTest::profileAdaptersHandOverNotifications_data()
{
    QTest::addColumn<QString>("path");
    QTest::newRow("UI-lab mock") << QStringLiteral(TANTO_MOCK_ENGINE_PROFILE_PATH);
    QTest::newRow("QtWebEngine") << QStringLiteral(TANTO_QT_ENGINE_PROFILE_PATH);
}

// A notification arrives from a Space's profile rather than from one page, so
// the profile is where the shell meets it: the origin and the words come out,
// and the answer — shown and clicked, or closed unseen — goes back in. Whether
// the reader is entitled to be interrupted is the shell's to decide, which it
// cannot do if the engine has already shown the notification.
void QtEngineContractTest::profileAdaptersHandOverNotifications()
{
    QFETCH(QString, path);
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(path));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const std::unique_ptr<QObject> profile(component.create());
    QVERIFY2(profile, qPrintable(component.errorString()));

    const auto *metaObject = profile->metaObject();
    const auto presented = metaObject->indexOfSignal(
        "notificationPresented(QString,QUrl,QString,QString)");
    QVERIFY2(presented >= 0, "profile does not report notifications to the shell");
    QVERIFY(metaObject->indexOfMethod("activateNotification(QVariant)") >= 0);
    QVERIFY(metaObject->indexOfMethod("dismissNotification(QVariant)") >= 0);
}

void QtEngineContractTest::adaptersTakeTheShellsAutoplayDecision_data()
{
    QTest::addColumn<QString>("path");
    QTest::newRow("UI-lab mock") << QStringLiteral(TANTO_MOCK_ENGINE_VIEW_PATH);
    QTest::newRow("QtWebEngine") << QStringLiteral(TANTO_QT_ENGINE_VIEW_PATH);
}

// Whether a page may start playing on its own is the shell's decision, not the
// engine's default: muted playback interrupts nobody, and audible playback
// waits until the reader has dealt with the origin. Every adapter starts out
// requiring a gesture and takes the answer it is given.
void QtEngineContractTest::adaptersTakeTheShellsAutoplayDecision()
{
    QFETCH(QString, path);
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(path));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));

    QVERIFY(!adapter->property("autoplayAllowed").toBool());
    QVERIFY(adapter->setProperty("autoplayAllowed", true));
    QVERIFY(adapter->property("autoplayAllowed").toBool());

    // The Qt adapter turns it into Chromium's own policy, which is per view.
    if (auto *view = adapter->findChild<QObject *>(QStringLiteral("qtWebView"))) {
        const auto settings = view->property("settings").value<QObject *>();
        QVERIFY(settings);
        QTRY_VERIFY(!settings->property("playbackRequiresUserGesture").toBool());
        QVERIFY(adapter->setProperty("autoplayAllowed", false));
        QTRY_VERIFY(settings->property("playbackRequiresUserGesture").toBool());
    }
}

// A retained tab holds a renderer the reader cannot see, so the browser has to
// be able to say what it costs. The adapter names the process; the platform
// answers for it.
void QtEngineContractTest::qtReportsTheProcessDrawingThePage()
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

    // Nothing is loaded, so nothing is drawing and there is no process to name.
    QCOMPARE(adapter->property("renderProcessPid").toInt(), 0);

    QVERIFY(adapter->setProperty("currentUrl", QUrl(QStringLiteral(
        "data:text/html,<title>Costed</title><p>page</p>"))));
    QTRY_COMPARE(adapter->property("pageTitle").toString(), QStringLiteral("Costed"));
    QTRY_VERIFY(adapter->property("renderProcessPid").toInt() > 0);

    tanto::ProcessResources resources;
    QVERIFY(resources.available());
    QVERIFY(resources.residentBytes(adapter->property("renderProcessPid").toInt()) > 0);
    // A process that is not there costs nothing, and is not guessed at.
    QCOMPARE(resources.residentBytes(0), 0);
}

int main(int argc, char *argv[])
{
    tanto::QtContentBlocker::registerSubstituteScheme();
    QtWebEngineQuick::initialize();
    QGuiApplication application(argc, argv);
    tanto::registerExternalProtocolHandler();
    QtEngineContractTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_qtenginecontract.moc"
