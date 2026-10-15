#include "DesktopTextScaling.h"

#include <cmath>

namespace omaweb {

double DesktopTextScaling::factor() const { return m_factor; }

void DesktopTextScaling::setFactor(double factor)
{
    if (qFuzzyCompare(factor, m_factor)) {
        return;
    }
    m_factor = factor;
    emit factorChanged();
}

double portalTextScalingFactor(const QVariant &value)
{
    // The key is a `d`; a string that happens to read as one is not it.
    if (value.typeId() != QMetaType::Double) {
        return 1.0;
    }
    const auto factor = value.toDouble();
    return std::isfinite(factor) && factor > 0 ? factor : 1.0;
}

} // namespace omaweb
