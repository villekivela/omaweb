#include "SessionSiteState.h"

#include <QVariantMap>

#include <algorithm>

namespace omaweb {

bool SessionSiteState::namedPurpose(const QString &purpose)
{
    return purpose == QStringLiteral("authentication") || purpose == QStringLiteral("payment");
}

bool SessionSiteState::allowThirdPartyCookies(
    const QString &spaceId, const QString &origin, const QString &purpose)
{
    if (origin.isEmpty() || !namedPurpose(purpose)) {
        return false;
    }
    m_cookieAllowances[spaceId].insert(origin, purpose);
    return true;
}

bool SessionSiteState::revokeThirdPartyCookies(const QString &spaceId, const QString &origin)
{
    const auto space = m_cookieAllowances.find(spaceId);
    if (space == m_cookieAllowances.end() || space->remove(origin) == 0) {
        return false;
    }
    if (space->isEmpty()) {
        m_cookieAllowances.erase(space);
    }
    return true;
}

bool SessionSiteState::thirdPartyCookiesAllowed(const QString &spaceId, const QString &origin) const
{
    return !origin.isEmpty() && m_cookieAllowances.value(spaceId).contains(origin);
}

QVariantList SessionSiteState::thirdPartyCookieAllowances(const QString &spaceId) const
{
    const auto &granted = m_cookieAllowances.value(spaceId);
    auto origins = granted.keys();
    std::sort(origins.begin(), origins.end());
    QVariantList allowances;
    allowances.reserve(origins.size());
    for (const auto &origin : origins) {
        allowances.append(QVariantMap {
            { QStringLiteral("origin"), origin },
            { QStringLiteral("purpose"), granted.value(origin) },
        });
    }
    return allowances;
}

QStringList SessionSiteState::allowedThirdPartyCookieOrigins(const QString &spaceId) const
{
    return m_cookieAllowances.value(spaceId).keys();
}

void SessionSiteState::allowCertificate(const QString &spaceId, const QString &origin)
{
    if (origin.isEmpty()) {
        return;
    }
    m_certificateExceptions[spaceId].insert(origin);
}

bool SessionSiteState::certificateAllowed(const QString &spaceId, const QString &origin) const
{
    return !origin.isEmpty() && m_certificateExceptions.value(spaceId).contains(origin);
}

QStringList SessionSiteState::allowedCertificateOrigins(const QString &spaceId) const
{
    auto origins = QStringList(m_certificateExceptions.value(spaceId).values());
    std::sort(origins.begin(), origins.end());
    return origins;
}

void SessionSiteState::forgetThirdParty(const QString &spaceId, const QString &origin)
{
    revokeThirdPartyCookies(spaceId, origin);
}

void SessionSiteState::forgetSpace(const QString &spaceId)
{
    m_cookieAllowances.remove(spaceId);
    m_certificateExceptions.remove(spaceId);
}

} // namespace omaweb
