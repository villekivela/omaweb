#include "GestureSettings.h"

#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <tuple>
#include <utility>

namespace omaweb {
namespace {

    constexpr auto key = "gestures";
    constexpr auto distanceMember = "distance";

    struct Gesture {
        QLatin1StringView name;
        QLatin1StringView fallback;
    };

    // Two fingers kept what they did before the reader had a say: Back and
    // Forward over the page, a Space over the sidebar.
    constexpr Gesture gestures[] {
        {QLatin1StringView("page"), QLatin1StringView("back-and-forward")},
        {QLatin1StringView("sidebar"), QLatin1StringView("switch-space")},
        {QLatin1StringView("three-fingers"), QLatin1StringView("switch-tab")},
    };

    const Gesture *find(const QString &name)
    {
        const auto *found = std::ranges::find(gestures, name, &Gesture::name);
        return found == std::ranges::end(gestures) ? nullptr : found;
    }

} // namespace

GestureSettings::GestureSettings(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_settings(std::move(configRoot))
{
    read();
    connect(&m_settings, &SettingsFile::changed, this, [this](const QStringList &keys) {
        if (!keys.contains(QLatin1String(key))) {
            return;
        }
        if (read()) {
            emit changed();
        }
    });
}

QString GestureSettings::pageAction() const { return m_page; }

QString GestureSettings::sidebarAction() const { return m_sidebar; }

QString GestureSettings::threeFingerAction() const { return m_threeFingers; }

int GestureSettings::distance() const { return m_distance; }

// The file's change handler applies a write, so a write it refused changes
// nothing, and what a reader edited in a moment ago is not overwritten from a
// stale copy.
void GestureSettings::setAction(const QString &gesture, const QString &action)
{
    const auto *named = find(gesture);
    if (!named || !SettingsFile::accepts(QLatin1String(key), QJsonObject {{gesture, action}})) {
        return;
    }
    const auto value = action == named->fallback ? QJsonValue() : QJsonValue(action);
    if (!m_settings.setMember(QLatin1String(key), gesture, value)) {
        emit changed();
    }
}

void GestureSettings::setDistance(int distance)
{
    const auto clamped = std::clamp(distance, minimumDistance(), maximumDistance());
    const auto snapped = (clamped + distanceStep() / 2) / distanceStep() * distanceStep();
    if (snapped == m_distance) {
        return;
    }
    const auto value = snapped == defaultDistance() ? QJsonValue() : QJsonValue(snapped);
    if (!m_settings.setMember(QLatin1String(key), QLatin1String(distanceMember), value)) {
        emit changed();
    }
}

bool GestureSettings::read()
{
    const auto stored = m_settings.value(QLatin1String(key)).toObject();
    const auto action = [&stored](const QString &name) {
        return stored.value(name).toString(find(name)->fallback);
    };
    const auto before = std::tuple(m_page, m_sidebar, m_threeFingers, m_distance);
    m_page = action(QStringLiteral("page"));
    m_sidebar = action(QStringLiteral("sidebar"));
    m_threeFingers = action(QStringLiteral("three-fingers"));
    m_distance = stored.value(QLatin1String(distanceMember)).toInt(defaultDistance());
    return std::tuple(m_page, m_sidebar, m_threeFingers, m_distance) != before;
}

} // namespace omaweb
