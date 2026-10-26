#pragma once

#include <QPointF>
#include <QtGlobal>

#include <deque>

namespace omaweb {

// How fast the fingers were scrolling when they lifted, from the movement they made in the
// `window` ms before it. Units are whatever the deltas are in; the page glide adds Qt's angle
// deltas, which are what QtWebEngine scrolls a page by.
//
// The window is GTK 4's: `SCROLL_CAPTURE_THRESHOLD_MS` in `gtkeventcontrollerscroll.c`. GTK
// measures it back from the last movement. This measures it back from the lift, so fingers that
// came to rest before lifting have no speed left and the page does not glide.
class GlideSpeed {
public:
    static constexpr quint64 window = 150;

    // Movement by `delta` at `at` ms on the events' clock.
    void add(quint64 at, QPointF delta);
    void clear();
    // Units a second on each axis, as the fingers lift at `liftedAt`.
    QPointF at(quint64 liftedAt) const;

private:
    struct Sample {
        quint64 at;
        QPointF delta;
    };
    std::deque<Sample> m_samples;
};

// A glide from `velocity`, slowing as GTK 4's kinetic scrolling does: the speed falls as
// exp(-friction t), so the glide covers velocity / friction in all. `friction` is GTK's
// `DECELERATION_FRICTION` in `gtkscrolledwindow.c`, per second.
//
// GTK stops once the speed is under 0.1 pixels a second, which is long after anything moves on
// screen. This stops once less than `rest` is left to travel, so the glide's last step is the
// last one a reader could see.
class GlideCurve {
public:
    static constexpr qreal friction = 4;
    // Two angle units are a page pixel at Qt's three lines a wheel step.
    static constexpr qreal rest = 2;

    explicit GlideCurve(QPointF velocity = {});

    // False when the glide would travel no more than `rest`.
    bool glides() const;
    // How long the glide runs, in seconds.
    qreal duration() const;
    // How far it has gone `seconds` after the lift, on each axis.
    QPointF travelled(qreal seconds) const;

private:
    QPointF m_velocity;
};

} // namespace omaweb
