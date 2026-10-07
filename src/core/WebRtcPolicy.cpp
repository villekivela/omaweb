#include "WebRtcPolicy.h"

#include "SettingsFile.h"

#include <utility>

namespace omaweb {
namespace {

    constexpr QLatin1StringView publicInterfacesOnlyKey("webrtc-public-interfaces-only");

} // namespace

WebRtcPolicy::WebRtcPolicy(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_settings(std::move(configRoot))
{
    connect(&m_settings, &SettingsFile::changed, this, [this](const QStringList &keys) {
        if (keys.contains(publicInterfacesOnlyKey)) {
            load();
        }
    });
    load();
}

bool WebRtcPolicy::publicInterfacesOnly() const { return m_publicInterfacesOnly; }

void WebRtcPolicy::setPublicInterfacesOnly(bool publicInterfacesOnly)
{
    if (publicInterfacesOnly == m_publicInterfacesOnly) {
        return;
    }
    // Applied when the file says it was written, the way an edit made there is.
    m_settings.set(publicInterfacesOnlyKey, publicInterfacesOnly);
}

// A file that cannot be read the way it is written opens the other
// interfaces to nobody: only an explicit `false` does.
void WebRtcPolicy::load()
{
    const auto value = m_settings.value(publicInterfacesOnlyKey).toBool(true);
    if (value != m_publicInterfacesOnly) {
        m_publicInterfacesOnly = value;
        emit publicInterfacesOnlyChanged();
    }
}

} // namespace omaweb
