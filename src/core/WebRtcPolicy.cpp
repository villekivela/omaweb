#include "WebRtcPolicy.h"

#include "PrivacyFile.h"

#include <utility>

namespace omaweb {
namespace {

    constexpr QLatin1StringView publicInterfacesOnlyKey("webrtc-public-interfaces-only");

} // namespace

WebRtcPolicy::WebRtcPolicy(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_configRoot(std::move(configRoot))
{
    load();
}

bool WebRtcPolicy::publicInterfacesOnly() const { return m_publicInterfacesOnly; }

void WebRtcPolicy::setPublicInterfacesOnly(bool publicInterfacesOnly)
{
    if (publicInterfacesOnly == m_publicInterfacesOnly) {
        return;
    }
    m_publicInterfacesOnly = publicInterfacesOnly;
    save();
    emit publicInterfacesOnlyChanged();
}

// A file that cannot be read the way it is written opens the other
// interfaces to nobody: only an explicit `false` does.
void WebRtcPolicy::load()
{
    const auto value = PrivacyFile::read(m_configRoot, publicInterfacesOnlyKey);
    m_publicInterfacesOnly = value.isBool() ? value.toBool() : true;
}

void WebRtcPolicy::save() const
{
    PrivacyFile::write(m_configRoot, publicInterfacesOnlyKey, m_publicInterfacesOnly);
}

} // namespace omaweb
