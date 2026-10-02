#include "SystemMotion.h"

#include <QProcessEnvironment>
#include <QTest>

using namespace omaweb;

class SystemMotionTest final : public QObject {
    Q_OBJECT

private slots:
    void readsThePortalsReducedMotion();
    void readsHyprlandsAnimationsSwitch();
    void hearsHyprlandReloadItsConfiguration();
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
