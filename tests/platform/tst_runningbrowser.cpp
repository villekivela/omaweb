#include "RunningBrowser.h"
#include "WindowActivation.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QGuiApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QWindow>

#include <optional>

using omaweb::RunningBrowser;

namespace {

const auto kBusName = QStringLiteral("dev.omaweb.browser");
const auto kObjectPath = QStringLiteral("/dev/omaweb/browser");
const auto kInterface = QStringLiteral("org.freedesktop.Application");
const auto kActivationToken = QStringLiteral("activation-token");
// The connection the stand-in for the running browser holds the name on.
const auto kRunningConnection = QStringLiteral("running-browser");

// What a launch handed over, as the browser already running received it.
struct HandedOver {
    QString method;
    QStringList uris;
    QVariantMap platformData;
};

// The browser already running, standing in for a real one so the test can
// read what a later launch sends it.
class RecordingBrowser final : public QObject {
    Q_OBJECT

public:
    std::optional<HandedOver> received() const
    {
        const QMutexLocker lock(&m_mutex);
        return m_received;
    }

    void record(HandedOver handedOver)
    {
        const QMutexLocker lock(&m_mutex);
        m_received = std::move(handedOver);
    }

    void forget()
    {
        const QMutexLocker lock(&m_mutex);
        m_received.reset();
    }

private:
    mutable QMutex m_mutex;
    std::optional<HandedOver> m_received;
};

class RecordingAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Application")

public:
    explicit RecordingAdaptor(RecordingBrowser *browser)
        : QDBusAbstractAdaptor(browser)
        , m_browser(browser)
    {
    }

public slots:
    void Activate(const QVariantMap &platformData)
    {
        m_browser->record({QStringLiteral("Activate"), {}, platformData});
    }

    void Open(const QStringList &uris, const QVariantMap &platformData)
    {
        m_browser->record({QStringLiteral("Open"), uris, platformData});
    }

private:
    RecordingBrowser *m_browser;
};

// Sends what a launcher sends the browser that holds the name, from a
// connection of its own, as another process would.
void sendAsLauncher(const QString &method, const QVariantList &arguments)
{
    auto launcher
        = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("launcher"));
    auto message = QDBusMessage::createMethodCall(kBusName, kObjectPath, kInterface, method);
    message.setArguments(arguments);
    launcher.asyncCall(message);
}

QVariantMap withToken(const QString &token) { return {{kActivationToken, token}}; }

} // namespace

class RunningBrowserTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    void handsOverTheLaunchersActivationToken();
    void asksForTheBrowserWithTheLaunchersActivationToken();
    void comesForwardWithTheTokenALinkArrivesWith();
    void comesForwardWithTheTokenALaunchWithoutAnAddressArrivesWith();
    void asksForTheWindowBehindAnotherApp();
    void spendsTheTokenOnOneActivation();

private:
    // The running browser holds the name on a connection of its own, on a
    // thread of its own: a hand-over blocks the thread it is made from until
    // the answer comes, so the answer has to come from somewhere else.
    QThread m_runningThread;
    RecordingBrowser *m_running = nullptr;
};

void RunningBrowserTest::initTestCase()
{
    auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, kRunningConnection);
    QVERIFY(bus.isConnected());
    m_running = new RecordingBrowser;
    new RecordingAdaptor(m_running);
    m_running->moveToThread(&m_runningThread);
    m_runningThread.start();
    QVERIFY(bus.registerObject(kObjectPath, m_running));
    QVERIFY(bus.registerService(kBusName));
}

void RunningBrowserTest::cleanupTestCase()
{
    QDBusConnection::disconnectFromBus(kRunningConnection);
    // Deleted on its own thread, as it finishes.
    m_running->deleteLater();
    m_runningThread.quit();
    m_runningThread.wait();
}

void RunningBrowserTest::cleanup()
{
    m_running->forget();
    qunsetenv("XDG_ACTIVATION_TOKEN");
    // A test that let the browser hold the name has destroyed it by now, and
    // the stand-in takes the name back for the next.
    QVERIFY(QDBusConnection(kRunningConnection).registerService(kBusName));
}

// A terminal or a launcher that hands the browser an address gives the new
// process the token the compositor issued for it. The process exits once it
// has handed the address over, so the token goes with the address or not at
// all, and without it the running browser cannot come forward.
void RunningBrowserTest::handsOverTheLaunchersActivationToken()
{
    qputenv("XDG_ACTIVATION_TOKEN", "token-from-the-launcher");
    RunningBrowser browser;
    QVERIFY(!browser.isPrimary());

    QVERIFY(browser.handOver(QUrl(QStringLiteral("https://example.org/"))));

    const auto received = m_running->received();
    QVERIFY(received.has_value());
    QCOMPARE(received->method, QStringLiteral("Open"));
    QCOMPARE(received->uris, QStringList {QStringLiteral("https://example.org/")});
    QCOMPARE(received->platformData.value(kActivationToken).toString(),
        QStringLiteral("token-from-the-launcher"));
}

void RunningBrowserTest::asksForTheBrowserWithTheLaunchersActivationToken()
{
    qputenv("XDG_ACTIVATION_TOKEN", "token-from-the-launcher");
    RunningBrowser browser;
    QVERIFY(!browser.isPrimary());

    QVERIFY(browser.handOver(QUrl()));

    const auto received = m_running->received();
    QVERIFY(received.has_value());
    QCOMPARE(received->method, QStringLiteral("Activate"));
    QCOMPARE(received->platformData.value(kActivationToken).toString(),
        QStringLiteral("token-from-the-launcher"));
}

// The running browser is the one on show once a link reaches it, so the
// launcher's token is what it comes forward with.
void RunningBrowserTest::comesForwardWithTheTokenALinkArrivesWith()
{
    auto running = QDBusConnection(kRunningConnection);
    QVERIFY(running.unregisterService(kBusName));
    RunningBrowser browser;
    QVERIFY(browser.isPrimary());
    QSignalSpy opened(&browser, &RunningBrowser::openRequested);
    QSignalSpy activated(&browser, &RunningBrowser::activationRequested);

    sendAsLauncher(QStringLiteral("Open"),
        {QStringList {QStringLiteral("https://example.org/")},
            withToken(QStringLiteral("token-from-the-launcher"))});

    QVERIFY(activated.wait());
    QCOMPARE(opened.count(), 1);
    QCOMPARE(opened.at(0).value(0).toUrl(), QUrl(QStringLiteral("https://example.org/")));
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.at(0).value(0).toString(), QStringLiteral("token-from-the-launcher"));
}

void RunningBrowserTest::comesForwardWithTheTokenALaunchWithoutAnAddressArrivesWith()
{
    auto running = QDBusConnection(kRunningConnection);
    QVERIFY(running.unregisterService(kBusName));
    RunningBrowser browser;
    QVERIFY(browser.isPrimary());
    QSignalSpy opened(&browser, &RunningBrowser::openRequested);
    QSignalSpy activated(&browser, &RunningBrowser::activationRequested);

    sendAsLauncher(
        QStringLiteral("Activate"), {withToken(QStringLiteral("token-from-the-launcher"))});

    QVERIFY(activated.wait());
    QCOMPARE(opened.count(), 0);
    QCOMPARE(activated.at(0).value(0).toString(), QStringLiteral("token-from-the-launcher"));
}

// The browser's window is behind the app a link came from. Offscreen has no
// compositor to refuse it, so the window asking is the window on show.
void RunningBrowserTest::asksForTheWindowBehindAnotherApp()
{
    QWindow browser;
    browser.show();
    QVERIFY(QTest::qWaitForWindowExposed(&browser));
    QWindow otherApp;
    otherApp.show();
    otherApp.requestActivate();
    QTRY_COMPARE(QGuiApplication::focusWindow(), &otherApp);

    omaweb::bringForward(&browser, QStringLiteral("token-from-the-launcher"));

    QTRY_COMPARE(QGuiApplication::focusWindow(), &browser);
}

// A token is good for one activation. One left where Qt looks for it would be
// offered again for a later request it was never issued for.
void RunningBrowserTest::spendsTheTokenOnOneActivation()
{
    QWindow browser;
    browser.show();
    QVERIFY(QTest::qWaitForWindowExposed(&browser));

    omaweb::bringForward(&browser, QStringLiteral("token-from-the-launcher"));

    QVERIFY(!qEnvironmentVariableIsSet("XDG_ACTIVATION_TOKEN"));
}

QTEST_MAIN(RunningBrowserTest)

#include "tst_runningbrowser.moc"
