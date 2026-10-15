#include "PageScale.h"

#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <utility>

namespace omaweb {
namespace {

    constexpr auto key = "page-scale";
    // Factors arrive as the doubles a sum or the desktop left, so a value
    // this close to a step is on it.
    constexpr double tolerance = 0.001;

    // Whole percent, so the file never holds what a float sum leaves behind.
    double snapped(double scale)
    {
        const auto steps = std::round(
            std::clamp(scale, PageScale::minimum(), PageScale::maximum()) / PageScale::step());
        return std::round(steps * PageScale::step() * 100) / 100;
    }

} // namespace

PageScale::PageScale(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_settings(std::move(configRoot))
{
    const auto read = [this] {
        const auto stored = m_settings.value(QLatin1String(key));
        return stored.isDouble() ? std::optional<double>(stored.toDouble()) : std::nullopt;
    };
    m_scale = read();
    connect(&m_settings, &SettingsFile::changed, this, [this, read](const QStringList &keys) {
        if (!keys.contains(QLatin1String(key))) {
            return;
        }
        if (const auto stored = read(); stored != m_scale) {
            m_scale = stored;
            emit changed();
        }
    });
}

double PageScale::value() const { return m_scale.value_or(m_desktopFactor); }

bool PageScale::overridden() const { return m_scale.has_value(); }

double PageScale::desktopFactor() const { return m_desktopFactor; }

void PageScale::setDesktopFactor(double factor)
{
    if (qFuzzyCompare(factor, m_desktopFactor)) {
        return;
    }
    m_desktopFactor = factor;
    emit desktopFactorChanged();
    if (!m_scale) {
        emit changed();
    }
}

double PageScale::drawn(double scale, double zoom)
{
    return std::clamp(scale * zoom, minimumDrawn(), maximumDrawn());
}

double PageScale::drawnAt(double scale, double zoom) const { return drawn(scale, zoom); }

// The file's change handler applies a write, so a write it refused changes
// nothing, and a value a reader edited in a moment ago is not overwritten from
// a stale copy.
void PageScale::set(double scale)
{
    const auto value = snapped(scale);
    if (m_scale == value) {
        return;
    }
    if (!m_settings.set(QLatin1String(key), value)) {
        emit changed();
    }
}

// A step past an end is no step, so a desktop beyond the range is never moved
// back towards it by a step the other way.
void PageScale::increase()
{
    if (value() < maximum() - tolerance) {
        set((std::floor((value() + tolerance) / step()) + 1) * step());
    }
}

void PageScale::decrease()
{
    if (value() > minimum() + tolerance) {
        set((std::ceil((value() - tolerance) / step()) - 1) * step());
    }
}

void PageScale::reset()
{
    if (!m_scale) {
        return;
    }
    if (!m_settings.set(QLatin1String(key), QJsonValue())) {
        emit changed();
    }
}

} // namespace omaweb
