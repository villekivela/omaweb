#include "WindowSwipe.h"

#include <QPointingDevice>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

using omaweb::WindowSwipe;

// Three fingers sideways reach a Wayland client as native gestures: a begin,
// a pan for each movement with the compositor's delta, and an end (#933). No
// touchpad can be driven here, so these are the events Qt would deliver.
class WindowSwipeTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void commitsTheWayTheFingersWent_data();
    void commitsTheWayTheFingersWent();
    void goesAtTheReadersDistanceAndNotShortOfIt();
    void goesOnAFlickHoweverShort();
    void leavesOtherFingerCountsAlone();
    void leavesAnUpwardSwipeAlone();
    void doesNothingWhileDisabled();

private:
    // Sends a whole gesture of `fingers` across the window in `steps` pans of
    // `delta`, `interval` ms apart, and answers whether the window's filter
    // took the begin.
    bool swipe(int fingers, QPointF delta, int steps, int interval = 50);
    void send(Qt::NativeGestureType type, int fingers, QPointF delta, int interval);

    QPointingDevice touchpad {QStringLiteral("touchpad"), 1, QInputDevice::DeviceType::TouchPad,
        QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position, 5, 0};
    QQuickWindow window;
    WindowSwipe recognizer;
    quint64 clock = 0;
};

void WindowSwipeTest::send(Qt::NativeGestureType type, int fingers, QPointF delta, int interval)
{
    QNativeGestureEvent event(
        type, &touchpad, fingers, QPointF(40, 40), QPointF(40, 40), QPointF(140, 140), 0, delta);
    clock += interval;
    event.setTimestamp(clock);
    QCoreApplication::sendEvent(&window, &event);
}

bool WindowSwipeTest::swipe(int fingers, QPointF delta, int steps, int interval)
{
    QNativeGestureEvent begin(Qt::BeginNativeGesture, &touchpad, fingers, QPointF(40, 40),
        QPointF(40, 40), QPointF(140, 140), 0, {});
    clock += 200;
    begin.setTimestamp(clock);
    QCoreApplication::sendEvent(&window, &begin);
    const bool taken = begin.isAccepted();
    for (int step = 0; step < steps; ++step) {
        send(Qt::PanNativeGesture, fingers, delta, interval);
    }
    send(Qt::EndNativeGesture, fingers, {}, 1);
    return taken;
}

void WindowSwipeTest::init()
{
    recognizer.setParentItem(window.contentItem());
    recognizer.setEnabled(true);
    recognizer.setProperty("distance", 120);
}

// A swipe that would scroll a wide page to the right, fingers going left, is
// the next one, as a two-finger swipe over the sidebar brings in the next
// Space.
void WindowSwipeTest::commitsTheWayTheFingersWent_data()
{
    QTest::addColumn<qreal>("dx");
    QTest::addColumn<int>("direction");
    QTest::newRow("left, the next") << -20.0 << 1;
    QTest::newRow("right, the previous") << 20.0 << -1;
}

void WindowSwipeTest::commitsTheWayTheFingersWent()
{
    QFETCH(qreal, dx);
    QFETCH(int, direction);
    QSignalSpy committed(&recognizer, &WindowSwipe::committed);
    swipe(3, {dx, 0}, 7);
    QCOMPARE(committed.size(), 1);
    QCOMPARE(committed.first().first().toInt(), direction);
}

void WindowSwipeTest::goesAtTheReadersDistanceAndNotShortOfIt()
{
    QSignalSpy committed(&recognizer, &WindowSwipe::committed);
    swipe(3, {-10, 0}, 11);
    QCOMPARE(committed.size(), 0);
    swipe(3, {-10, 0}, 12);
    QCOMPARE(committed.size(), 1);

    recognizer.setProperty("distance", 60);
    swipe(3, {-10, 0}, 6);
    QCOMPARE(committed.size(), 2);
    recognizer.setProperty("distance", 240);
    swipe(3, {-10, 0}, 20);
    QCOMPARE(committed.size(), 2);
}

void WindowSwipeTest::goesOnAFlickHoweverShort()
{
    QSignalSpy committed(&recognizer, &WindowSwipe::committed);
    swipe(3, {15, 0}, 4, 10);
    QCOMPARE(committed.size(), 1);
    QCOMPARE(committed.first().first().toInt(), -1);
}

// Two fingers are the page's and the sidebar's, and four the desktop's.
void WindowSwipeTest::leavesOtherFingerCountsAlone()
{
    QSignalSpy committed(&recognizer, &WindowSwipe::committed);
    QVERIFY(!swipe(2, {-20, 0}, 10));
    QVERIFY(!swipe(4, {-20, 0}, 10));
    QCOMPARE(committed.size(), 0);
    QVERIFY(swipe(3, {-20, 0}, 10));
    QCOMPARE(committed.size(), 1);
}

// Whichever way the fingers first go six pixels owns the swipe.
void WindowSwipeTest::leavesAnUpwardSwipeAlone()
{
    QSignalSpy committed(&recognizer, &WindowSwipe::committed);
    QNativeGestureEvent begin(Qt::BeginNativeGesture, &touchpad, 3, QPointF(40, 40),
        QPointF(40, 40), QPointF(140, 140), 0, {});
    QCoreApplication::sendEvent(&window, &begin);
    send(Qt::PanNativeGesture, 3, {0, -8}, 50);
    for (int step = 0; step < 15; ++step) {
        send(Qt::PanNativeGesture, 3, {-20, 0}, 50);
    }
    send(Qt::EndNativeGesture, 3, {}, 1);
    QCOMPARE(committed.size(), 0);
}

// "Nothing" in Settings leaves the swipe to whatever else wants it.
void WindowSwipeTest::doesNothingWhileDisabled()
{
    QSignalSpy committed(&recognizer, &WindowSwipe::committed);
    recognizer.setEnabled(false);
    QVERIFY(!swipe(3, {-20, 0}, 10));
    QCOMPARE(committed.size(), 0);
}

QTEST_MAIN(WindowSwipeTest)
#include "tst_windowswipe.moc"
