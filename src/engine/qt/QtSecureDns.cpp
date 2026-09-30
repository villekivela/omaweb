#include "QtSecureDns.h"

#include "SecureDns.h"

#include <utility>

namespace omaweb {

QtSecureDns::QtSecureDns(SecureDns *secureDns, QObject *parent)
    : QtSecureDns(secureDns, &QWebEngineGlobalSettings::setDnsMode, parent)
{
}

QtSecureDns::QtSecureDns(SecureDns *secureDns, SetDnsMode setDnsMode, QObject *parent)
    : QObject(parent)
    , m_secureDns(secureDns)
    , m_setDnsMode(std::move(setDnsMode))
{
    connect(m_secureDns, &SecureDns::changed, this, &QtSecureDns::apply);
    apply();
}

bool QtSecureDns::applied() const { return m_applied; }

// Secure mode only. Chromium's automatic mode falls back to the system in the
// clear when the resolver fails, which gives up what the reader turned on.
void QtSecureDns::apply()
{
    namespace Settings = QWebEngineGlobalSettings;
    const auto serverTemplate = m_secureDns->serverTemplate();
    const auto applied = serverTemplate.isEmpty()
        ? m_setDnsMode({Settings::SecureDnsMode::SystemOnly, {}})
        : m_setDnsMode({Settings::SecureDnsMode::SecureOnly, {serverTemplate}});
    if (!applied && !serverTemplate.isEmpty()) {
        // A resolver the engine refused must not leave the last one in force
        // under a name the reader has moved away from.
        m_setDnsMode({Settings::SecureDnsMode::SystemOnly, {}});
    }
    if (applied != m_applied) {
        m_applied = applied;
        emit appliedChanged();
    }
}

} // namespace omaweb
