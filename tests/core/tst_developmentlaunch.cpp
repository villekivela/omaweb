#include "DevelopmentLaunch.h"

#include <QTest>

using tanto::readDevelopmentLaunch;

class DevelopmentLaunchTest final : public QObject {
    Q_OBJECT

private slots:
    void ordinaryLaunchesOpenNoListener();
    void bindsTheAskedForListenerToLoopback();
    void refusesAPortNothingCanBeReachedOn();
    void refusesEngineDebuggingFlagsFromEitherRoute();
    void takesPrivateWindowsAwayFromADebuggedSession();
};

// The whole of an ordinary launch: nothing asked for, nothing listening.
void DevelopmentLaunchTest::ordinaryLaunchesOpenNoListener()
{
    const auto launch = readDevelopmentLaunch({QStringLiteral("tanto")});
    QVERIFY(!launch.remoteDebugging);
    QVERIFY(launch.listenAddress.isEmpty());
    QVERIFY(launch.privateWindowsAvailable);
    QVERIFY(launch.refusal.isEmpty());
}

void DevelopmentLaunchTest::bindsTheAskedForListenerToLoopback()
{
    const auto named = readDevelopmentLaunch(
        {QStringLiteral("tanto"), QStringLiteral("--remote-debugging=9333")});
    QVERIFY(named.remoteDebugging);
    QCOMPARE(named.listenAddress, QStringLiteral("127.0.0.1:9333"));
    QVERIFY(named.refusal.isEmpty());

    // Asking without naming a port is asking for Chromium's own.
    const auto bare = readDevelopmentLaunch(
        {QStringLiteral("tanto"), QStringLiteral("--remote-debugging")});
    QVERIFY(bare.remoteDebugging);
    QCOMPARE(bare.listenAddress, QStringLiteral("127.0.0.1:9222"));
}

void DevelopmentLaunchTest::refusesAPortNothingCanBeReachedOn()
{
    for (const auto *port : {"0", "80", "70000", "http"}) {
        const auto launch = readDevelopmentLaunch({QStringLiteral("tanto"),
            QStringLiteral("--remote-debugging=%1").arg(QString::fromLatin1(port))});
        QVERIFY2(!launch.refusal.isEmpty(), port);
        QVERIFY2(!launch.remoteDebugging, port);
    }
}

// Tanto's own option is the only way to a listener. Chromium's own switches
// would name their own interface and leave Private windows on offer, so they
// are a refusal to start rather than a listener Tanto did not choose — through
// the command line or through the environment's engine flags.
void DevelopmentLaunchTest::refusesEngineDebuggingFlagsFromEitherRoute()
{
    const auto argument = readDevelopmentLaunch(
        {QStringLiteral("tanto"), QStringLiteral("--remote-debugging-port=9222")});
    QVERIFY(!argument.refusal.isEmpty());
    QVERIFY(!argument.remoteDebugging);
    QVERIFY(argument.privateWindowsAvailable);

    const auto environment = readDevelopmentLaunch({QStringLiteral("tanto")},
        {QStringLiteral("--remote-debugging-pipe")});
    QVERIFY(!environment.refusal.isEmpty());

    // A flag that only configures a channel Tanto never opened is not a
    // refusal: with no listener there is nothing for it to widen.
    const auto harmless = readDevelopmentLaunch({QStringLiteral("tanto")},
        {QStringLiteral("--remote-allow-origins=https://example.com")});
    QVERIFY(harmless.refusal.isEmpty());
    QVERIFY(!harmless.remoteDebugging);

    const auto address = readDevelopmentLaunch({QStringLiteral("tanto"),
        QStringLiteral("--remote-debugging=9333"),
        QStringLiteral("--remote-debugging-address=0.0.0.0")});
    QVERIFY(!address.refusal.isEmpty());
    QVERIFY(!address.remoteDebugging);
}

void DevelopmentLaunchTest::takesPrivateWindowsAwayFromADebuggedSession()
{
    const auto launch = readDevelopmentLaunch(
        {QStringLiteral("tanto"), QStringLiteral("--remote-debugging=9333")});
    QVERIFY(!launch.privateWindowsAvailable);
}

QTEST_GUILESS_MAIN(DevelopmentLaunchTest)

#include "tst_developmentlaunch.moc"
