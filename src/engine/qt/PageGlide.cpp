#include "PageGlide.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace omaweb {

void GlideSpeed::add(quint64 at, QPointF delta)
{
    m_samples.push_back({at, delta});
    // The fingers lift after their last movement, so nothing older than the window before it
    // can be in the window before the lift.
    while (at - m_samples.front().at > window) {
        m_samples.pop_front();
    }
}

void GlideSpeed::clear() { m_samples.clear(); }

QPointF GlideSpeed::at(quint64 liftedAt) const
{
    const auto first = std::find_if(m_samples.cbegin(), m_samples.cend(),
        [liftedAt](const Sample &sample) { return liftedAt - sample.at <= window; });
    if (first == m_samples.cend() || first->at >= liftedAt) {
        return {};
    }
    // A sample's delta is the movement up to it, so the first one in the window moved before
    // the window's time began.
    QPointF moved;
    for (auto sample = std::next(first); sample != m_samples.cend(); ++sample) {
        moved += sample->delta;
    }
    return moved * 1000.0 / static_cast<qreal>(liftedAt - first->at);
}

GlideCurve::GlideCurve(QPointF velocity)
    : m_velocity(velocity)
{
}

bool GlideCurve::glides() const { return duration() > 0; }

// What is left to travel at t is speed / friction * exp(-friction t), which falls to `rest` at
// ln(speed / (friction rest)) / friction.
qreal GlideCurve::duration() const
{
    const qreal speed = std::hypot(m_velocity.x(), m_velocity.y());
    const qreal toGo = speed / friction;
    return toGo > rest ? std::log(toGo / rest) / friction : 0;
}

QPointF GlideCurve::travelled(qreal seconds) const
{
    return m_velocity / friction * (1 - std::exp(-friction * seconds));
}

} // namespace omaweb
