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
// A list or pane in the chrome, any Qt Quick `Flickable`, glides the same way and stops on the
// same input (#864), measured and sent in pixels, which are what a `Flickable` scrolls a touchpad
// by. It stops at the view's end, as a page's stops at Chromium's.
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
    // The angle Qt's Wayland plugin gives each pixel of fingers' scrolling, which a chrome view's
    // glide gives its own.
    static constexpr int anglePerPixel = 12;

    // Watches the application's events from now on.
    explicit QtPageGlide(QObject *parent = nullptr);

    bool gliding() const;
    Q_INVOKABLE void stop();
    // Stops a glide in the page `view` draws.
    Q_INVOKABLE void stopIn(QQuickItem *view);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // What the fingers scroll: a page, by a wheel's angle, or a view in the chrome, by its pixels.
    enum class Scroller { Page, ChromeView };

    // Follows a touchpad's wheel event to a page or a chrome view, and starts the glide when it
    // is the lift. True to keep the event from it.
    bool follow(QQuickItem *scrolled, Scroller scroller, QWheelEvent *event);
    // Stops the glide, logging `cause`.
    void halt(const char *cause);
    void step();
    // Sends `delta` in the units `m_scroller` scrolls by.
    void send(Qt::ScrollPhase phase, QPoint delta);

    // Where the fingers last were over what they scroll, which the glide's events say too.
    struct Pointer {
        QPointF position;
        QPointF globalPosition;
        Qt::KeyboardModifiers modifiers;
        bool inverted = false;
        const QPointingDevice *device = nullptr;
    };

    // The page or chrome view the fingers are scrolling.
    QPointer<QQuickItem> m_scrolled;
    Scroller m_scroller = Scroller::Page;
    Pointer m_pointer;
    quint64 m_liftedAt = 0;
    GlideSpeed m_speed;
    // For the log: the fingers' movements in this gesture, and when they last moved.
    int m_moves = 0;
    quint64 m_movedAt = 0;

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
