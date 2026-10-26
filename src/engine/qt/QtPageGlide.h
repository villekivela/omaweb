#pragma once

#include "PageGlide.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QTimer>

class QPointingDevice;
class QQuickItem;
class QWheelEvent;

namespace omaweb {

// Keeps a touchpad scroll in a page going after the fingers lift, slowing to a stop, as GTK 4's
// scrolled views do (#808). A mouse wheel scrolls as it always has. "Touchpad glide" in
// `docs/architecture.md` says why the glide is made here rather than by Chromium.
//
// When fingers lift from a page fast enough, this holds the lift's `ScrollEnd` back from the
// item QtWebEngine takes input through and goes on sending it `ScrollUpdate`s from `GlideSpeed`
// and `GlideCurve`, every `frame` ms, then the `ScrollEnd`. To Chromium the gesture is still the
// fingers', so the glide scrolls whatever they were scrolling and stops at its end as they would.
// The updates carry angle deltas only, because those are all QtWebEngine scrolls a page by.
//
// The glide stops, and Chromium hears the gesture end, on any wheel event (the fingers moving
// again or a mouse wheel), a press, a key, the window losing focus, the page hiding (a tab or
// Space switch), `stop()` (the fingers resting on the touchpad) and `stopIn()` (a page load).
class QtPageGlide final : public QObject {
    Q_OBJECT

public:
    // Two steps to a 60 Hz display's frame, so a 120 Hz display has one in each of its own.
    static constexpr int frame = 8;

    // Watches the application's events from now on.
    explicit QtPageGlide(QObject *parent = nullptr);

    bool gliding() const;
    Q_INVOKABLE void stop();
    // Stops a glide in the page `view` draws.
    Q_INVOKABLE void stopIn(QQuickItem *view);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // Follows a touchpad's wheel event to a page, and starts the glide when it is the lift.
    // True to keep the event from the page.
    bool follow(QQuickItem *page, QWheelEvent *event);
    void step();
    void send(Qt::ScrollPhase phase, QPoint angleDelta);

    // Where the fingers last were over the page, which the glide's events say too.
    struct Pointer {
        QPointF position;
        QPointF globalPosition;
        Qt::KeyboardModifiers modifiers;
        bool inverted = false;
        const QPointingDevice *device = nullptr;
    };

    // The page the fingers are scrolling.
    QPointer<QQuickItem> m_page;
    Pointer m_pointer;
    quint64 m_liftedAt = 0;
    GlideSpeed m_speed;

    GlideCurve m_curve;
    QElapsedTimer m_since;
    QPointF m_sent;
    QTimer m_frames;
    bool m_sending = false;
};

// Makes the application's glide available to every QML engine as the `QtPageGlide` singleton of
// `import Omaweb.Engine`. Call once per process, before loading QML that uses it, and keep the
// glide until the last engine is gone.
void registerQtPageGlide(QtPageGlide *glide);

} // namespace omaweb
