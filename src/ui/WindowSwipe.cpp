#include "WindowSwipe.h"

#include <QNativeGestureEvent>
#include <QQuickWindow>

#include <algorithm>
#include <cmath>

namespace omaweb {

WindowSwipe::WindowSwipe(QQuickItem *parent)
    : QQuickItem(parent)
{
}

WindowSwipe::~WindowSwipe() { watch(nullptr); }

bool WindowSwipe::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::NativeGesture) {
        return gesture(static_cast<QNativeGestureEvent *>(event));
    }
    return QQuickItem::eventFilter(watched, event);
}

void WindowSwipe::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if (change == ItemSceneChange) {
        watch(value.window);
    } else if (change == ItemEnabledHasChanged && !value.boolValue) {
        m_tracking = false;
    }
}

bool WindowSwipe::gesture(QNativeGestureEvent *event)
{
    if (event->fingerCount() != m_fingers || !isEnabled()) {
        return false;
    }
    switch (event->gestureType()) {
    case Qt::BeginNativeGesture:
        m_tracking = true;
        m_axis = Axis::Undecided;
        m_travel = {};
        m_samples.clear();
        break;
    case Qt::PanNativeGesture:
        if (!m_tracking) {
            return false;
        }
        m_travel += event->delta();
        if (m_axis == Axis::Undecided
            && std::max(std::abs(m_travel.x()), std::abs(m_travel.y())) >= axisLock) {
            m_axis = std::abs(m_travel.x()) >= std::abs(m_travel.y()) ? Axis::Across : Axis::Down;
        }
        m_samples.push_back({event->timestamp(), m_travel.x()});
        while (m_samples.size() > 1 && event->timestamp() - m_samples.front().at > speedWindow) {
            m_samples.pop_front();
        }
        break;
    case Qt::EndNativeGesture: {
        if (!m_tracking) {
            return false;
        }
        m_tracking = false;
        const qreal along = m_travel.x();
        const qreal speed = speedAt(event->timestamp());
        const bool farEnough = std::abs(along) >= m_distance;
        const bool flicked = along != 0 && speed * along > 0 && std::abs(speed) > flickSpeed;
        if (m_axis == Axis::Across && (farEnough || flicked)) {
            emit committed(along < 0 ? 1 : -1);
        }
        break;
    }
    default:
        return false;
    }
    event->accept();
    return true;
}

void WindowSwipe::watch(QQuickWindow *window)
{
    if (m_window == window) {
        return;
    }
    if (m_window) {
        m_window->removeEventFilter(this);
    }
    m_window = window;
    m_tracking = false;
    if (m_window) {
        m_window->installEventFilter(this);
    }
}

qreal WindowSwipe::speedAt(quint64 at) const
{
    const auto from = std::find_if(m_samples.cbegin(), m_samples.cend(),
        [at](const Sample &sample) { return at - sample.at <= speedWindow; });
    if (from == m_samples.cend() || from->at == m_samples.back().at) {
        return 0;
    }
    const auto &to = m_samples.back();
    return (to.offset - from->offset) * 1000.0 / static_cast<qreal>(to.at - from->at);
}

void registerWindowSwipe() { qmlRegisterType<WindowSwipe>("Omaweb", 1, 0, "WindowSwipe"); }

} // namespace omaweb
