#include "QtPageGlide.h"

#include "QtPageInput.h"

#include <QCoreApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QWheelEvent>

#include <cmath>

namespace omaweb {
namespace {

    QQuickItem *pageOf(QObject *object)
    {
        auto *item = qobject_cast<QQuickItem *>(object);
        return isPageInput(item) ? item : nullptr;
    }

    // Qt's Wayland plugin gives phases to fingers' scrolling alone, not to a wheel's notches or
    // a continuous source's movement. Its `ScrollBegin` says the source is not synthesized, so
    // the phase is all there is to go by. `SpaceSwipe` also takes a system's movement without
    // phases, and ends it after a pause; a glide needs the lift itself.
    bool fromTouchpad(const QWheelEvent *event) { return event->phase() != Qt::NoScrollPhase; }

    // Visible, and drawn: a tab the reader left that an Agent works in stays visible to the
    // engine at no opacity (#376).
    bool onShow(const QQuickItem *item)
    {
        if (!item->isVisible()) {
            return false;
        }
        for (; item != nullptr; item = item->parentItem()) {
            if (item->opacity() <= 0) {
                return false;
            }
        }
        return true;
    }

} // namespace

QtPageGlide::QtPageGlide(QObject *parent)
    : QObject(parent)
{
    m_frames.setInterval(frame);
    m_frames.setTimerType(Qt::PreciseTimer);
    connect(&m_frames, &QTimer::timeout, this, &QtPageGlide::step);
    if (auto *application = QCoreApplication::instance()) {
        application->installEventFilter(this);
    }
}

bool QtPageGlide::gliding() const { return m_frames.isActive(); }

void QtPageGlide::stop()
{
    if (!gliding()) {
        return;
    }
    m_frames.stop();
    if (m_page) {
        send(Qt::ScrollEnd, {});
    }
    m_page.clear();
}

void QtPageGlide::stopIn(QQuickItem *view)
{
    if (gliding() && m_page && view != nullptr && view->isAncestorOf(m_page)) {
        stop();
    }
}

bool QtPageGlide::eventFilter(QObject *watched, QEvent *event)
{
    if (m_sending) {
        return false;
    }
    switch (event->type()) {
    case QEvent::Wheel: {
        // Qt hands an event to the window before the item under the pointer, so a glide stops
        // before the page hears what stopped it.
        stop();
        auto *wheel = static_cast<QWheelEvent *>(event);
        auto *page = pageOf(watched);
        return page != nullptr && fromTouchpad(wheel) && follow(page, wheel);
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::TouchBegin:
    case QEvent::TabletPress:
    case QEvent::KeyPress:
    case QEvent::ShortcutOverride:
    case QEvent::WindowDeactivate:
        stop();
        break;
    default:
        break;
    }
    return false;
}

bool QtPageGlide::follow(QQuickItem *page, QWheelEvent *event)
{
    if (event->phase() == Qt::ScrollBegin || page != m_page) {
        m_page = page;
        m_speed.clear();
    }
    m_pointer = {event->position(), event->globalPosition(), event->modifiers(), event->inverted(),
        event->pointingDevice()};
    if (event->phase() != Qt::ScrollEnd) {
        m_speed.add(event->timestamp(), event->angleDelta());
        return false;
    }
    m_curve = GlideCurve(m_speed.at(event->timestamp()));
    m_speed.clear();
    if (!m_curve.glides()) {
        return false;
    }
    m_liftedAt = event->timestamp();
    m_sent = {};
    m_since.start();
    m_frames.start();
    // Taken here, the end goes no further, to an item under the page that would hear a gesture
    // end it never heard begin.
    event->accept();
    return true;
}

void QtPageGlide::step()
{
    if (!m_page || !onShow(m_page)) {
        stop();
        return;
    }
    const qreal seconds = static_cast<qreal>(m_since.nsecsElapsed()) / 1e9;
    const bool last = seconds >= m_curve.duration();
    const QPointF due = m_curve.travelled(std::min(seconds, m_curve.duration())) - m_sent;
    // Whole angle units only, with what is left over carried to the next frame.
    const QPoint whole(static_cast<int>(due.x()), static_cast<int>(due.y()));
    if (!whole.isNull()) {
        m_sent += QPointF(whole);
        send(Qt::ScrollUpdate, whole);
    }
    if (last) {
        stop();
    }
}

void QtPageGlide::send(Qt::ScrollPhase phase, QPoint angleDelta)
{
    QWheelEvent event(m_pointer.position, m_pointer.globalPosition, QPoint(), angleDelta,
        Qt::NoButton, m_pointer.modifiers, phase, m_pointer.inverted,
        Qt::MouseEventSynthesizedBySystem, m_pointer.device);
    event.setTimestamp(m_liftedAt + static_cast<quint64>(m_since.elapsed()));
    m_sending = true;
    QCoreApplication::sendEvent(m_page, &event);
    m_sending = false;
}

// The events it watches are the whole process's, so every QML engine is handed the one glide,
// which none of them owns. Qt lets a singleton instance belong to one engine only.
void registerQtPageGlide(QtPageGlide *glide)
{
    QJSEngine::setObjectOwnership(glide, QJSEngine::CppOwnership);
    qmlRegisterSingletonType<QtPageGlide>("Omaweb.Engine", 1, 0, "QtPageGlide",
        [glide](QQmlEngine *, QJSEngine *) -> QObject * { return glide; });
}

} // namespace omaweb
