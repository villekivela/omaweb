#include "TouchpadHold.h"

#include <QGuiApplication>
#include <QTest>

using namespace omaweb;

class TouchpadHoldTest final : public QObject {
    Q_OBJECT

private slots:
    void listensForNothingOffWayland();
    void listensOnWayland();
};

// The offscreen platform a test runs under has no compositor to ask.
void TouchpadHoldTest::listensForNothingOffWayland()
{
    if (QGuiApplication::platformName() == u"wayland") {
        QSKIP("This is about a platform other than Wayland.");
    }
    TouchpadHold hold;
    QTest::qWait(50);
    QVERIFY(!hold.listening());
}

// The compositor answers the binding with no event of its own, so the hold gesture is made as
// soon as it offers the protocol. Run under a Wayland session whose compositor offers version 3,
// as Hyprland and wlroots do, which CI's offscreen run is not. No window is shown.
void TouchpadHoldTest::listensOnWayland()
{
    if (QGuiApplication::platformName() != u"wayland") {
        QSKIP("Needs a Wayland session.");
    }
    TouchpadHold hold;
    QTRY_VERIFY_WITH_TIMEOUT(hold.listening(), 2000);
}

QTEST_MAIN(TouchpadHoldTest)
#include "tst_touchpadhold.moc"
