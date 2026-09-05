#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace omaweb {

// The site state that lives for exactly as long as a browsing session and is
// never written down.
//
// Two things belong here. The third-party allowances a sign-in or a payment was
// given, and the certificate exceptions the reader granted. Both are decisions
// about one origin inside one Space, both are visible in Site information, and
// neither may reach a disk: written down, either would become a check the
// reader stopped being asked about. A shared private session hands one of these
// round its windows, which is what makes those windows one identity and makes
// the state go when the last of them closes.
//
// Spaces are the outer key rather than part of a composite string, so nothing
// outside has to know how a key is spelled to read one Space's answers.
class SessionSiteState final {
public:
    // The flows a third party may legitimately complete on another site's
    // behalf. An allowance Omaweb cannot name to the reader is not one it
    // gives, so a purpose outside this list is refused.
    static bool namedPurpose(const QString &purpose);

    bool allowThirdPartyCookies(
        const QString &spaceId, const QString &origin, const QString &purpose);
    bool revokeThirdPartyCookies(const QString &spaceId, const QString &origin);
    bool thirdPartyCookiesAllowed(const QString &spaceId, const QString &origin) const;
    // Origin and purpose, in origin order, for Site information to list.
    QVariantList thirdPartyCookieAllowances(const QString &spaceId) const;
    // Read by the engine adapter enforcing the blocking, for a Space that is
    // not necessarily the one on show: a retained tab's profile is asked about
    // its own Space.
    QStringList allowedThirdPartyCookieOrigins(const QString &spaceId) const;

    // A certificate failure the reader let through. Engines remember an
    // accepted certificate for as long as their profile lives and offer no way
    // to take it back, so Omaweb keeps its own record of which origins are in
    // that state: the address trigger has to keep saying the check was waived
    // for as long as it is, in every tab of the Space it was waived in.
    void allowCertificate(const QString &spaceId, const QString &origin);
    bool certificateAllowed(const QString &spaceId, const QString &origin) const;
    QStringList allowedCertificateOrigins(const QString &spaceId) const;

    // One origin's session state, dropped together. A certificate exception is
    // not included: the engine is still holding it, so forgetting Omaweb's
    // record would only stop the reader being told.
    void forgetThirdParty(const QString &spaceId, const QString &origin);
    void forgetSpace(const QString &spaceId);

private:
    QHash<QString, QHash<QString, QString>> m_cookieAllowances;
    QHash<QString, QSet<QString>> m_certificateExceptions;
};

} // namespace omaweb
