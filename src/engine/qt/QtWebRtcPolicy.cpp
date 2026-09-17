#include "QtWebRtcPolicy.h"

#include "QtProfileSettings.h"
#include "WebRtcPolicy.h"

#include <QWebEngineSettings>
#include <QtGlobal>

#include <algorithm>

namespace omaweb {

QtWebRtcPolicy::QtWebRtcPolicy(WebRtcPolicy *policy, QObject *parent)
    : QObject(parent)
    , m_policy(policy)
{
    if (!available()) {
        qWarning("Omaweb was built against Qt %s and is running on Qt %s, so a page's calls "
                 "are offered every interface, the engine's own default.",
            QT_VERSION_STR, qVersion());
        return;
    }
    connect(m_policy, &WebRtcPolicy::publicInterfacesOnlyChanged, this, &QtWebRtcPolicy::apply);
}

bool QtWebRtcPolicy::available() const { return m_policy && QtProfileSettings::reachable(); }

void QtWebRtcPolicy::attachToProfile(QObject *profile)
{
    if (!available()) {
        return;
    }
    auto *settings = QtProfileSettings::of(profile);
    if (!settings) {
        qWarning("The WebRTC address policy could not reach the profile's settings.");
        return;
    }
    const auto attached = std::ranges::any_of(
        m_profiles, [profile](const QPointer<QObject> &known) { return known == profile; });
    if (attached) {
        return;
    }
    m_profiles.emplace_back(profile);
    settings->setAttribute(
        QWebEngineSettings::WebRTCPublicInterfacesOnly, m_policy->publicInterfacesOnly());
}

void QtWebRtcPolicy::apply()
{
    std::erase_if(m_profiles, [](const QPointer<QObject> &profile) { return profile.isNull(); });
    for (const auto &profile : m_profiles) {
        if (auto *settings = QtProfileSettings::of(profile.data())) {
            settings->setAttribute(
                QWebEngineSettings::WebRTCPublicInterfacesOnly, m_policy->publicInterfacesOnly());
        }
    }
}

} // namespace omaweb
