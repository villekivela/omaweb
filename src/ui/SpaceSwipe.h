#pragma once

#include <QElapsedTimer>
#include <QQuickItem>
#include <QTimer>

#include <deque>

class QWheelEvent;

namespace omaweb {

// A sideways swipe or wheel over the sidebar, read as a switch of Space. It
// stands over the sidebar and takes the wheel events that are its own, passing
// every other one on to the items under it, the tab list's scrolling first.
//
// Two fingers on a touchpad send pixel deltas the system synthesized, with
// scroll phases where the platform reports them, as Wayland does. Whichever
// axis first moves `axisLock` pixels owns the gesture until the fingers lift,
// so a diagonal scroll down the list never moves Spaces and a swipe never
// scrolls the list. While a sideways gesture owns them, `offset` follows the
// fingers one pixel for one. The fingers lift on `ScrollEnd`; a gesture
// without phases ends after `idleRelease` ms without an event, and one whose
// `ScrollEnd` never comes after `lostRelease` ms. On release the swipe commits
// towards the Space in view when it is at least half the item's width in, or
// moving towards it faster than `flickSpeed` over the last `speedWindow` ms,
// and springs back otherwise.
//
// A wheel sends angle steps of its own, and on Wayland pixels beside them.
// Each notch of `notchAngle` steps one Space, and a notch while `settling` or
// within `notchQuiet` ms after it is dropped.
//
// A direction is the footer's: 1 is the next Space, -1 the previous. Qt's
// deltas already follow the reader's scroll setting, and a swipe that would
// scroll a wide page to the right, a negative delta, brings in the next Space.
//
// Not final: QML derives from a type it creates.
class SpaceSwipe : public QQuickItem {
    Q_OBJECT
    // How far the fingers have taken the list, in pixels: negative towards the
    // next Space, positive towards the previous one. Zero outside a swipe.
    Q_PROPERTY(qreal offset READ offset NOTIFY offsetChanged)
    // Whether a sideways gesture owns the fingers.
    Q_PROPERTY(bool swiping READ swiping NOTIFY swipingChanged)
    // Whether the Space slide is running, which a notch waits out.
    Q_PROPERTY(bool settling READ settling WRITE setSettling NOTIFY settlingChanged)

public:
    static constexpr qreal axisLock = 6;
    static constexpr int idleRelease = 120;
    static constexpr int lostRelease = 1000;
    static constexpr qreal flickSpeed = 1000;
    static constexpr quint64 speedWindow = 100;
    static constexpr int notchAngle = 120;
    static constexpr int notchQuiet = 200;

    explicit SpaceSwipe(QQuickItem *parent = nullptr);

    qreal offset() const;
    bool swiping() const;
    bool settling() const;
    void setSettling(bool settling);

signals:
    void offsetChanged();
    void swipingChanged();
    void settlingChanged();
    // The fingers lifted with the Space in `direction` far enough in, or
    // flicked towards it.
    void committed(int direction);
    // The fingers lifted short of a switch, or the swipe was called off.
    void sprangBack();
    // One notch of the wheel towards the Space in `direction`.
    void stepped(int direction);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    enum class Axis { Undecided, Across, Down };
    struct Sample {
        quint64 at;
        qreal offset;
    };

    void touchpad(QWheelEvent *event);
    void notch(QWheelEvent *event);
    void begin();
    // Ends the gesture as the fingers lift, at `liftedAt` on the events' clock.
    void release(quint64 liftedAt);
    void cancel();
    void setOffset(qreal offset);
    void setSwiping(bool swiping);
    // How fast the list was moving at `at`, in pixels a second, from the
    // movement in the `speedWindow` before it.
    qreal speedAt(quint64 at) const;

    bool m_inGesture = false;
    bool m_phased = false;
    Axis m_axis = Axis::Undecided;
    // The last gesture went sideways, so the momentum a platform sends after
    // it is the swipe's too.
    bool m_lastAcross = false;
    QPointF m_travel;
    qreal m_offset = 0;
    bool m_swiping = false;
    std::deque<Sample> m_samples;
    quint64 m_lastEventAt = 0;
    QTimer m_lift;

    bool m_settling = false;
    int m_notchAngle = 0;
    QElapsedTimer m_clock;
    qint64 m_quietUntil = 0;
};

// Makes `SpaceSwipe` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerSpaceSwipe();

} // namespace omaweb
