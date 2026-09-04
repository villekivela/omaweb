#include "DevelopmentLaunch.h"

#include <QTest>

using omaweb::readDevelopmentLaunch;

class DevelopmentLaunchTest final : public QObject {
    Q_OBJECT

private slots:
    void ordinaryLaunchesOpenNoListener();
    void bindsTheAskedForListenerToLoopback();
    void refusesAPortNothingCanBeReachedOn();
    void refusesEngineDebuggingFlagsFromEitherRoute();
    void takesPrivateWindowsAwayFromADebuggedSession();
    void refusesEveryRouteToAGlobalInsecureContentOverride();
    void refusesEveryRouteToATurnedOffRendererSandbox();
};

// The whole of an ordinary launch: nothing asked for, nothing listening.
void DevelopmentLaunchTest::ordinaryLaunchesOpenNoListener()
{
    const auto launch = readDevelopmentLaunch({QStringLiteral("omaweb")});
    QVERIFY(!launch.remoteDebugging);
    QVERIFY(launch.listenAddress.isEmpty());
    QVERIFY(launch.privateWindowsAvailable);
    QVERIFY(launch.refusal.isEmpty());
}

void DevelopmentLaunchTest::bindsTheAskedForListenerToLoopback()
{
    const auto named = readDevelopmentLaunch(
        {QStringLiteral("omaweb"), QStringLiteral("--remote-debugging=9333")});
    QVERIFY(named.remoteDebugging);
    QCOMPARE(named.listenAddress, QStringLiteral("127.0.0.1:9333"));
    QVERIFY(named.refusal.isEmpty());

    // Asking without naming a port is asking for Chromium's own.
    const auto bare = readDevelopmentLaunch(
        {QStringLiteral("omaweb"), QStringLiteral("--remote-debugging")});
    QVERIFY(bare.remoteDebugging);
    QCOMPARE(bare.listenAddress, QStringLiteral("127.0.0.1:9222"));
}

void DevelopmentLaunchTest::refusesAPortNothingCanBeReachedOn()
{
    for (const auto *port : {"0", "80", "70000", "http"}) {
        const auto launch = readDevelopmentLaunch({QStringLiteral("omaweb"),
            QStringLiteral("--remote-debugging=%1").arg(QString::fromLatin1(port))});
        QVERIFY2(!launch.refusal.isEmpty(), port);
        QVERIFY2(!launch.remoteDebugging, port);
    }
}

// Omaweb's own option is the only way to a listener. Chromium's own switches
// would name their own interface and leave Private windows on offer, so they
// are a refusal to start rather than a listener Omaweb did not choose — through
// the command line or through the environment's engine flags.
void DevelopmentLaunchTest::refusesEngineDebuggingFlagsFromEitherRoute()
{
    const auto argument = readDevelopmentLaunch(
        {QStringLiteral("omaweb"), QStringLiteral("--remote-debugging-port=9222")});
    QVERIFY(!argument.refusal.isEmpty());
    QVERIFY(!argument.remoteDebugging);
    QVERIFY(argument.privateWindowsAvailable);

    const auto environment = readDevelopmentLaunch({QStringLiteral("omaweb")},
        {QStringLiteral("--remote-debugging-pipe")});
    QVERIFY(!environment.refusal.isEmpty());

    // A flag that only configures a channel Omaweb never opened is not a
    // refusal: with no listener there is nothing for it to widen.
    const auto harmless = readDevelopmentLaunch({QStringLiteral("omaweb")},
        {QStringLiteral("--remote-allow-origins=https://example.com")});
    QVERIFY(harmless.refusal.isEmpty());
    QVERIFY(!harmless.remoteDebugging);

    const auto address = readDevelopmentLaunch({QStringLiteral("omaweb"),
        QStringLiteral("--remote-debugging=9333"),
        QStringLiteral("--remote-debugging-address=0.0.0.0")});
    QVERIFY(!address.refusal.isEmpty());
    QVERIFY(!address.remoteDebugging);
}

void DevelopmentLaunchTest::takesPrivateWindowsAwayFromADebuggedSession()
{
    const auto launch = readDevelopmentLaunch(
        {QStringLiteral("omaweb"), QStringLiteral("--remote-debugging=9333")});
    QVERIFY(!launch.privateWindowsAvailable);
}

// Active mixed content stays blocked, and there is no launch that turns the
// blocking off browser-wide. A switch that lowers the web's own security
// boundaries for every page is a refusal to start rather than a preference,
// because nothing on the page can tell the reader it was given away.
void DevelopmentLaunchTest::refusesEveryRouteToAGlobalInsecureContentOverride()
{
    static const QStringList overrides = {
        QStringLiteral("--allow-running-insecure-content"),
        QStringLiteral("--disable-web-security"),
        QStringLiteral("--ignore-certificate-errors"),
        QStringLiteral("--ignore-certificate-errors-spki-list=abc"),
        QStringLiteral("--allow-insecure-localhost"),
        QStringLiteral("--unsafely-treat-insecure-origin-as-secure=http://example.com"),
        QStringLiteral("--reduce-security-for-testing"),
        QStringLiteral("--disable-site-isolation-trials"),
    };
    for (const auto &override : overrides) {
        const auto argument = readDevelopmentLaunch({QStringLiteral("omaweb"), override});
        QVERIFY2(!argument.refusal.isEmpty(), qPrintable(override));
        QVERIFY2(argument.refusal.contains(override.section(u'=', 0, 0)),
            qPrintable(argument.refusal));

        const auto environment = readDevelopmentLaunch({QStringLiteral("omaweb")}, {override});
        QVERIFY2(!environment.refusal.isEmpty(), qPrintable(override));
    }

    // The blocking is the default, so an ordinary launch says nothing about it.
    const auto ordinary = readDevelopmentLaunch({QStringLiteral("omaweb")});
    QVERIFY(ordinary.refusal.isEmpty());
}

// There is no Omaweb that runs page code outside a sandbox, so a switch that
// would is a refusal to start — and the environment is as much a route to one
// as the command line, because whatever set the variable is not the reader.
void DevelopmentLaunchTest::refusesEveryRouteToATurnedOffRendererSandbox()
{
    static const QStringList switches = {
        QStringLiteral("--no-sandbox"),
        QStringLiteral("--disable-sandbox"),
        QStringLiteral("--disable-gpu-sandbox"),
        QStringLiteral("--disable-setuid-sandbox"),
        QStringLiteral("--disable-namespace-sandbox"),
        QStringLiteral("--disable-seccomp-filter-sandbox"),
        QStringLiteral("--no-zygote"),
        QStringLiteral("--single-process"),
        QStringLiteral("--in-process-gpu"),
        QStringLiteral("--in-process-network-service"),
    };
    for (const auto &disabling : switches) {
        const auto argument = readDevelopmentLaunch({QStringLiteral("omaweb"), disabling});
        QVERIFY2(!argument.refusal.isEmpty(), qPrintable(disabling));
        QVERIFY2(argument.refusal.contains(disabling), qPrintable(argument.refusal));

        const auto environment = readDevelopmentLaunch({QStringLiteral("omaweb")}, {disabling});
        QVERIFY2(!environment.refusal.isEmpty(), qPrintable(disabling));
        // A refused launch asks for nothing else: no listener, no Private
        // windows, nothing but the reason.
        QVERIFY(!environment.remoteDebugging);
        QVERIFY(environment.listenAddress.isEmpty());
    }

    // A switch that only happens to contain one of those words is not one of
    // them: the refusal is a list, not a substring search.
    const auto unrelated = readDevelopmentLaunch(
        {QStringLiteral("omaweb"), QStringLiteral("--enable-sandbox-logging")});
    QVERIFY(unrelated.refusal.isEmpty());
}

QTEST_GUILESS_MAIN(DevelopmentLaunchTest)

#include "tst_developmentlaunch.moc"
