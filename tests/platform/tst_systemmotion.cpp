#include "SystemMotion.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

using namespace omaweb;

class SystemMotionTest final : public QObject {
    Q_OBJECT

private slots:
    void readsThePortalsReducedMotion();
    void readsHyprlandsAnimationsSwitch();
    void hearsHyprlandReloadItsConfiguration();
    void asksHyprlandBeforeTheEventLoopTurns();
    void findsHyprlandsSockets();
    void readsGnomesEnableAnimations();
};

// The portal's `org.freedesktop.appearance` `reduced-motion` is 1 for reduced
// motion and 0 for no preference, and an unknown value is read as no
// preference, as the portal's own documentation asks. A desktop whose portal
// has no such key has said nothing.
void SystemMotionTest::readsThePortalsReducedMotion()
{
    QVERIFY(portalAsksToReduceMotion(QVariant(1U)));
    QVERIFY(!portalAsksToReduceMotion(QVariant(0U)));
    QVERIFY(!portalAsksToReduceMotion(QVariant(2U)));
    QVERIFY(!portalAsksToReduceMotion(QVariant()));
    QVERIFY(!portalAsksToReduceMotion(QVariant(QStringLiteral("1"))));
}

// What `j/getoption animations:enabled` answers on Hyprland's socket. Turned
// off, the compositor draws no animation of its own, and the reader who turned
// it off wants none from the windows either.
void SystemMotionTest::readsHyprlandsAnimationsSwitch()
{
    QVERIFY(
        hyprlandAsksToReduceMotion(R"({"option": "animations:enabled", "int": 0, "set": true})"));
    QVERIFY(
        !hyprlandAsksToReduceMotion(R"({"option": "animations:enabled", "int": 1, "set": false})"));
    // Hyprland 0.56 answers a boolean option as `bool`, spaced as it sends it.
    QVERIFY(hyprlandAsksToReduceMotion(
        R"({"option": "animations:enabled", "bool": false, "set": true })"));
    QVERIFY(!hyprlandAsksToReduceMotion(
        R"({"option": "animations:enabled", "bool": true, "set": true })"));
    // An answer that is not the option's is no answer: an error, a compositor
    // that did not understand, or a socket that closed early.
    QVERIFY(!hyprlandAsksToReduceMotion("no such option"));
    QVERIFY(!hyprlandAsksToReduceMotion(R"({"option": "animations:enabled"})"));
    QVERIFY(!hyprlandAsksToReduceMotion(""));
}

// Hyprland's event socket announces a reload as one line among the rest, and
// a read can hold several lines or end partway through one.
void SystemMotionTest::hearsHyprlandReloadItsConfiguration()
{
    QVERIFY(hyprlandConfigurationReloaded("configreloaded>>\n"));
    QVERIFY(hyprlandConfigurationReloaded("workspace>>2\nconfigreloaded>>\nactivewindow>>a,b\n"));
    // A window whose title names the event is not the event.
    QVERIFY(!hyprlandConfigurationReloaded("workspace>>2\nactivewindow>>kitty,configreloaded>>\n"));
    QVERIFY(!hyprlandConfigurationReloaded(""));
}

// Hyprland waits up to five seconds for a request on a connection it has
// accepted, with the whole compositor stopped, and the window's first frame
// waits on the compositor. The request is therefore on the wire before the
// constructor returns, with no turn of the event loop to send it.
void SystemMotionTest::asksHyprlandBeforeTheEventLoopTurns()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    QVERIFY(QDir(runtime.path()).mkpath(QStringLiteral("hypr/test")));
    const auto environment = QProcessEnvironment::systemEnvironment();
    const auto restore = qScopeGuard([environment] {
        for (const auto *name :
            {"XDG_RUNTIME_DIR", "HYPRLAND_INSTANCE_SIGNATURE", "DBUS_SESSION_BUS_ADDRESS"}) {
            const auto value = environment.value(QString::fromLatin1(name));
            value.isNull() ? qunsetenv(name) : qputenv(name, value.toUtf8());
        }
    });
    qputenv("XDG_RUNTIME_DIR", runtime.path().toUtf8());
    qputenv("HYPRLAND_INSTANCE_SIGNATURE", "test");
    // No session bus, so only Hyprland's answer can reduce motion here.
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent");
    QLocalServer hyprland;
    QVERIFY(hyprland.listen(runtime.filePath(QStringLiteral("hypr/test/.socket.sock"))));

    SystemMotion motion;
    QVERIFY(!motion.reduced());
    // Both waits poll the socket and run no events.
    QVERIFY(hyprland.waitForNewConnection(1000));
    auto *connection = hyprland.nextPendingConnection();
    QVERIFY(connection->waitForReadyRead(1000));
    QCOMPARE(connection->readAll(), QByteArray("j/getoption animations:enabled"));

    connection->write(R"({"option": "animations:enabled", "bool": false, "set": true })");
    connection->disconnectFromServer();
    QTRY_VERIFY(motion.reduced());
}

// The sockets live under the runtime directory, named by the instance the
// session runs, and a session that is not Hyprland's has neither.
void SystemMotionTest::findsHyprlandsSockets()
{
    QProcessEnvironment hyprland;
    hyprland.insert(QStringLiteral("XDG_RUNTIME_DIR"), QStringLiteral("/run/user/1000"));
    hyprland.insert(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE"), QStringLiteral("abc_123"));
    QCOMPARE(hyprlandSocketPath(hyprland, HyprlandSocket::Requests),
        QStringLiteral("/run/user/1000/hypr/abc_123/.socket.sock"));
    QCOMPARE(hyprlandSocketPath(hyprland, HyprlandSocket::Events),
        QStringLiteral("/run/user/1000/hypr/abc_123/.socket2.sock"));

    QProcessEnvironment elsewhere;
    elsewhere.insert(QStringLiteral("XDG_RUNTIME_DIR"), QStringLiteral("/run/user/1000"));
    QVERIFY(hyprlandSocketPath(elsewhere, HyprlandSocket::Requests).isEmpty());

    // A signature is a name, never a way out of the directory.
    QProcessEnvironment crafted = hyprland;
    crafted.insert(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE"), QStringLiteral("../../tmp"));
    QVERIFY(hyprlandSocketPath(crafted, HyprlandSocket::Requests).isEmpty());
}

// GNOME's `org.gnome.desktop.interface` `enable-animations`, which GTK
// follows and the GTK portal passes on to every other toolkit.
void SystemMotionTest::readsGnomesEnableAnimations()
{
    QVERIFY(gnomeAsksToReduceMotion(QVariant(false)));
    QVERIFY(!gnomeAsksToReduceMotion(QVariant(true)));
    QVERIFY(!gnomeAsksToReduceMotion(QVariant()));
}

QTEST_GUILESS_MAIN(SystemMotionTest)
#include "tst_systemmotion.moc"
