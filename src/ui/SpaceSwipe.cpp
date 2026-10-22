#include "SpaceSwipe.h"

#include <QQmlEngine>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace omaweb {

SpaceSwipe::SpaceSwipe(QQuickItem *parent)
    : QQuickItem(parent)
{
    m_lift.setSingleShot(true);
    connect(&m_lift, &QTimer::timeout, this, [this] { release(m_lastEventAt); });
    m_clock.start();
}

qreal SpaceSwipe::offset() const { return m_offset; }

bool SpaceSwipe::swiping() const { return m_swiping; }

bool SpaceSwipe::settling() const { return m_settling; }

void SpaceSwipe::setSettling(bool settling)
{
    if (settling == m_settling) {
        return;
    }
    m_settling = settling;
    if (!settling) {
        m_quietUntil = m_clock.elapsed() + notchQuiet;
    }
    emit settlingChanged();
}

void SpaceSwipe::wheelEvent(QWheelEvent *event)
{
    if (!isEnabled()) {
        event->ignore();
        return;
    }
    // Wayland gives a wheel pixels too, and only a touchpad's fingers phases,
    // so what tells a touchpad from a wheel is that its movement is the
    // system's rendering of fingers or a continuous source rather than a
    // wheel's own steps.
    const bool fromTouchpad = event->phase() != Qt::NoScrollPhase
        || (!event->pixelDelta().isNull() && event->source() == Qt::MouseEventSynthesizedBySystem);
    if (fromTouchpad) {
        touchpad(event);
    } else {
        notch(event);
    }
}

void SpaceSwipe::touchpad(QWheelEvent *event)
{
    // A platform that carries the list on after the fingers lift sends the
    // rest as momentum, which belongs to the gesture that ended.
    if (event->phase() == Qt::ScrollMomentum) {
        event->setAccepted(m_lastAcross);
        return;
    }
    if (event->phase() == Qt::ScrollBegin || !m_inGesture) {
        begin();
    }
    m_phased = m_phased || event->phase() != Qt::NoScrollPhase;
    m_lastEventAt = event->timestamp();
    const bool lifted = event->phase() == Qt::ScrollEnd;

    const QPointF delta = event->pixelDelta();
    if (m_axis == Axis::Undecided) {
        m_travel += delta;
        if (std::abs(m_travel.x()) >= axisLock && std::abs(m_travel.x()) > std::abs(m_travel.y())) {
            m_axis = Axis::Across;
            m_lastAcross = true;
            setSwiping(true);
            setOffset(m_travel.x());
            m_samples.push_back({m_lastEventAt, m_offset});
        } else if (std::abs(m_travel.y()) >= axisLock) {
            m_axis = Axis::Down;
        }
    } else if (m_axis == Axis::Across && !delta.isNull()) {
        setOffset(m_offset + delta.x());
        m_samples.push_back({m_lastEventAt, m_offset});
        while (m_samples.size() > 1 && m_lastEventAt - m_samples.front().at > speedWindow) {
            m_samples.pop_front();
        }
    }
    // The list's own scrolling heard the gesture begin while no axis owned it,
    // and holds itself pressed until it hears the end, so the end goes on to
    // it whichever way the gesture went.
    event->setAccepted(m_axis == Axis::Across && !lifted);

    if (lifted) {
        m_lift.stop();
        release(m_lastEventAt);
        return;
    }
    m_lift.start(m_phased ? lostRelease : idleRelease);
}

void SpaceSwipe::notch(QWheelEvent *event)
{
    const QPoint angle = event->angleDelta();
    if (angle.x() == 0 || std::abs(angle.x()) < std::abs(angle.y())) {
        event->ignore();
        return;
    }
    event->accept();
    m_notchAngle += angle.x();
    if (std::abs(m_notchAngle) < notchAngle) {
        return;
    }
    const int direction = m_notchAngle < 0 ? 1 : -1;
    m_notchAngle = 0;
    if (m_settling || m_clock.elapsed() < m_quietUntil) {
        return;
    }
    m_quietUntil = m_clock.elapsed() + notchQuiet;
    emit stepped(direction);
}

void SpaceSwipe::begin()
{
    if (m_inGesture) {
        cancel();
    }
    m_inGesture = true;
    m_phased = false;
    m_axis = Axis::Undecided;
    m_lastAcross = false;
    m_travel = {};
    m_samples.clear();
}

void SpaceSwipe::release(quint64 liftedAt)
{
    if (!m_inGesture) {
        return;
    }
    m_inGesture = false;
    m_lift.stop();
    if (m_axis != Axis::Across) {
        return;
    }
    const qreal distance = m_offset;
    const qreal speed = speedAt(liftedAt);
    const bool farEnough = std::abs(distance) >= width() / 2;
    const bool flicked = distance != 0 && speed * distance > 0 && std::abs(speed) > flickSpeed;
    setSwiping(false);
    if (distance != 0 && (farEnough || flicked)) {
        emit committed(distance < 0 ? 1 : -1);
    } else {
        emit sprangBack();
    }
    setOffset(0);
}

void SpaceSwipe::cancel()
{
    m_inGesture = false;
    m_lift.stop();
    if (m_axis == Axis::Across) {
        setSwiping(false);
        emit sprangBack();
        setOffset(0);
    }
    m_axis = Axis::Undecided;
}

qreal SpaceSwipe::speedAt(quint64 at) const
{
    const auto from = std::find_if(m_samples.cbegin(), m_samples.cend(),
        [at](const Sample &sample) { return at - sample.at <= speedWindow; });
    if (from == m_samples.cend() || from->at == m_samples.back().at) {
        return 0;
    }
    const auto &to = m_samples.back();
    return (to.offset - from->offset) * 1000.0 / static_cast<qreal>(to.at - from->at);
}

void SpaceSwipe::setOffset(qreal offset)
{
    if (offset == m_offset) {
        return;
    }
    m_offset = offset;
    emit offsetChanged();
}

void SpaceSwipe::setSwiping(bool swiping)
{
    if (swiping == m_swiping) {
        return;
    }
    m_swiping = swiping;
    emit swipingChanged();
}

// A swipe the sidebar stops taking, for a tab drag or a Space left alone,
// springs back rather than staying where the fingers left it.
void SpaceSwipe::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemEnabledHasChanged && !value.boolValue && m_inGesture) {
        cancel();
    }
    QQuickItem::itemChange(change, value);
}

void registerSpaceSwipe() { qmlRegisterType<SpaceSwipe>("Omaweb", 1, 0, "SpaceSwipe"); }

} // namespace omaweb
