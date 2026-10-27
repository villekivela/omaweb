#include "QtPageGlide.h"

#include "QtPageInput.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QQuickItem>
#include <QWheelEvent>

#include <cmath>

namespace omaweb {
namespace {

    // Silent unless asked, as with `QT_LOGGING_RULES=omaweb.glide.info=true`: what a touchpad
    // really delivers is the platform's, which no test here can stand in for.
    Q_LOGGING_CATEGORY(glideLog, "omaweb.glide", QtWarningMsg)

    QDebug operator<<(QDebug debug, const QWheelEvent &event)
    {
        const QDebugStateSaver saver(debug);
        debug.nospace() << event.phase() << " source " << event.source() << " angle "
                        << event.angleDelta() << " pixel " << event.pixelDelta() << " at "
                        << event.timestamp();
        return debug;
    }

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

void QtPageGlide::stop() { halt("asked"); }

void QtPageGlide::halt(const char *cause)
{
    if (!gliding()) {
        return;
    }
    qCInfo(glideLog) << "glide stopped:" << cause << "after" << m_since.elapsed() << "ms, sent"
                     << m_sent;
    m_frames.stop();
    if (m_page) {
        send(Qt::ScrollEnd, {});
    }
    m_page.clear();
}

void QtPageGlide::stopIn(QQuickItem *view)
{
    if (gliding() && m_page && view != nullptr && view->isAncestorOf(m_page)) {
        halt("page load");
    }
}

bool QtPageGlide::eventFilter(QObject *watched, QEvent *event)
{
    if (m_sending) {
        return false;
    }
    switch (event->type()) {
    case QEvent::Wheel: {
        auto *wheel = static_cast<QWheelEvent *>(event);
        auto *page = pageOf(watched);
        qCInfo(glideLog).nospace() << "wheel to " << watched->metaObject()->className()
                                   << (page != nullptr ? " (page)" : "") << ": " << *wheel;
        // Qt 6.12's `QWindowSystemInterface::handleWheelEvent` sends an event whose delta is
        // neither only vertical nor only horizontal twice, so the lift that started the glide
        // comes again. Fingers that come back begin first, so an end with no movement is that
        // copy. The page must not hear it either: to Chromium it would end the glide's gesture.
        if (gliding() && wheel->phase() == Qt::ScrollEnd && wheel->angleDelta().isNull()) {
            if (page != nullptr) {
                wheel->accept();
                return true;
            }
            return false;
        }
        // Qt hands an event to the window before the item under the pointer, so a glide stops
        // before the page hears what stopped it.
        halt("wheel event");
        return page != nullptr && fromTouchpad(wheel) && follow(page, wheel);
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::TouchBegin:
    case QEvent::TabletPress:
    case QEvent::KeyPress:
    case QEvent::ShortcutOverride:
    case QEvent::WindowDeactivate:
        if (gliding()) {
            qCInfo(glideLog) << "stopping for" << event->type() << "to"
                             << watched->metaObject()->className();
        }
        halt("input");
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
        m_moves = 0;
    }
    m_pointer = {event->position(), event->globalPosition(), event->modifiers(), event->inverted(),
        event->pointingDevice()};
    if (event->phase() != Qt::ScrollEnd) {
        m_speed.add(event->timestamp(), event->angleDelta());
        m_movedAt = event->timestamp();
        ++m_moves;
        return false;
    }
    const QPointF speed = m_speed.at(event->timestamp());
    m_curve = GlideCurve(speed);
    m_speed.clear();
    qCInfo(glideLog).nospace() << "lift at " << event->timestamp() << " after " << m_moves
                               << " moves, the last at " << m_movedAt << ": speed " << speed
                               << " a second, duration " << m_curve.duration() << " s";
    if (!m_curve.glides()) {
        qCInfo(glideLog) << "no glide: under" << GlideCurve::rest << "units to travel";
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
    if (!m_page) {
        halt("page gone");
        return;
    }
    if (!onShow(m_page)) {
        halt("page hidden");
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
        halt("came to rest");
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
