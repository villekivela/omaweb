#include "SidebarOpacity.h"

#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <utility>

namespace omaweb {
namespace {

    constexpr auto key = "sidebar-opacity";

} // namespace

SidebarOpacity::SidebarOpacity(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_settings(std::move(configRoot))
{
    const auto read = [this] {
        const auto stored = m_settings.value(QLatin1String(key));
        return stored.isDouble() ? std::optional<double>(stored.toDouble()) : std::nullopt;
    };
    m_opacity = read();
    connect(&m_settings, &SettingsFile::changed, this, [this, read](const QStringList &keys) {
        if (!keys.contains(QLatin1String(key))) {
            return;
        }
        if (const auto stored = read(); stored != m_opacity) {
            m_opacity = stored;
            emit changed();
        }
    });
}

std::optional<double> SidebarOpacity::opacity() const { return m_opacity; }

bool SidebarOpacity::overridden() const { return m_opacity.has_value(); }

double SidebarOpacity::value() const { return m_opacity.value_or(0); }

// The file's change handler applies a write, so a write it refused changes
// nothing, and a value a reader edited in a moment ago is not overwritten from
// a stale copy.
void SidebarOpacity::set(double opacity)
{
    const auto steps = std::round(std::clamp(opacity, minimum(), maximum()) / step());
    // Whole percent, so the file never holds what a float sum leaves behind.
    const auto snapped = std::round(steps * step() * 100) / 100;
    if (m_opacity == snapped) {
        return;
    }
    if (!m_settings.set(QLatin1String(key), snapped)) {
        emit changed();
    }
}

void SidebarOpacity::reset()
{
    if (!m_opacity) {
        return;
    }
    if (!m_settings.set(QLatin1String(key), QJsonValue())) {
        emit changed();
    }
}

} // namespace omaweb
