#pragma once

#include <QPointF>
#include <QPointer>
#include <QQuickItem>

#include <deque>

class QNativeGestureEvent;

namespace omaweb {

// A sideways swipe of `fingers` fingers anywhere in the window (#933).
//
// Qt's Wayland plugin hands a compositor's `zwp_pointer_gestures_v1` swipe to
// the window as native gestures: `BeginNativeGesture`, a `PanNativeGesture`
// for each movement carrying the compositor's delta, and `EndNativeGesture`,
// each with its finger count. A cancelled swipe ends the same way, because Qt
// drops the cancel. Qt Quick would deliver them to the item under the pointer,
// which over a page is the engine's, so this watches its window's events
// before delivery instead, and takes those with its finger count while
// enabled. Others go on as they came.
//
// Whichever axis first moves `axisLock` pixels owns the swipe until it ends.
// A sideways swipe commits when the fingers have moved `distance` pixels, or
// are moving faster than `flickSpeed` over the last `speedWindow` ms, and does
// nothing otherwise.
//
// A direction is the footer's: 1 is the next, -1 the previous. Fingers going
// left, as a swipe that would scroll a wide page to the right, go on, the way
// `SpaceSwipe` reads two fingers over the sidebar.
//
// Not final: QML derives from a type it creates.
class WindowSwipe : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(int fingers MEMBER m_fingers NOTIFY fingersChanged)
    // In logical pixels: the reader's, from Settings.
    Q_PROPERTY(qreal distance MEMBER m_distance NOTIFY distanceChanged)

public:
    static constexpr qreal axisLock = 6;
    static constexpr qreal flickSpeed = 1000;
    static constexpr quint64 speedWindow = 100;
    static constexpr qreal defaultDistance = 120;

    explicit WindowSwipe(QQuickItem *parent = nullptr);
    ~WindowSwipe() override;

signals:
    void fingersChanged();
    void distanceChanged();
    // The fingers lifted far enough along, or flicked, in `direction`.
    void committed(int direction);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    enum class Axis { Undecided, Across, Down };
    struct Sample {
        quint64 at;
        qreal offset;
    };

    bool gesture(QNativeGestureEvent *event);
    void watch(QQuickWindow *window);
    // How fast the fingers were moving at `at`, in pixels a second, from the
    // movement in the `speedWindow` before it.
    qreal speedAt(quint64 at) const;

    QPointer<QQuickWindow> m_window;
    int m_fingers = 3;
    qreal m_distance = defaultDistance;
    bool m_tracking = false;
    Axis m_axis = Axis::Undecided;
    QPointF m_travel;
    std::deque<Sample> m_samples;
};

// Makes `WindowSwipe` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerWindowSwipe();

} // namespace omaweb
