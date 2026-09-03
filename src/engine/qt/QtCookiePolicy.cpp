#include "QtCookiePolicy.h"

#include "BrowserController.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QWebEngineCookieStore>
#include <QtWebEngineQuick/QQuickWebEngineProfile>

namespace omaweb {
namespace {

// The same separator the core keys its own session tables with, so a key made
// here and a key made there are the same string.
constexpr QChar keySeparator = QChar(0x1f);
// How many refused third parties one page is remembered by. Enough to name the
// embedded flows a page actually has, and bounded so the table cannot become a
// record of everywhere the reader has been.
constexpr qsizetype refusedOriginsPerPage = 16;

QString allowanceKey(const QString &spaceId, const QString &origin)
{
    return spaceId + keySeparator + origin;
}

} // namespace

QtCookiePolicy::QtCookiePolicy(QObject *parent)
    : QObject(parent)
{
}

QtCookiePolicy::~QtCookiePolicy()
{
    // Chromium holds the callback, and the callback holds this. Taking it back
    // before the object goes is what keeps a request in flight from reaching a
    // policy that no longer exists.
    for (auto it = m_attachments.cbegin(); it != m_attachments.cend(); ++it) {
        it.key()->setCookieFilter(nullptr);
    }
}

QString QtCookiePolicy::cookieOrigin(const QUrl &url)
{
    const auto scheme = url.scheme().toLower();
    if ((scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
        || url.host().isEmpty()) {
        return {};
    }
    QUrl origin;
    origin.setScheme(scheme);
    origin.setHost(url.host().toLower());
    const auto port = url.port(-1);
    const auto defaultPort = scheme == QStringLiteral("https") ? 443 : 80;
    if (port != -1 && port != defaultPort) {
        origin.setPort(port);
    }
    return origin.toString(QUrl::FullyEncoded);
}

bool QtCookiePolicy::attachToProfile(QObject *profile, QObject *controller,
    const QString &spaceId)
{
    auto *engineProfile = qobject_cast<QQuickWebEngineProfile *>(profile);
    auto *browser = qobject_cast<BrowserController *>(controller);
    if (!engineProfile || !browser) {
        return false;
    }
    auto *store = engineProfile->cookieStore();
    if (!store) {
        return false;
    }

    m_attachments.insert(store, Attachment{browser, spaceId});
    connect(store, &QObject::destroyed, this, [this, store] {
        m_attachments.remove(store);
        refreshAllowances();
    });
    connect(browser, &BrowserController::thirdPartyCookieAllowancesChanged, this,
        &QtCookiePolicy::refreshAllowances, Qt::UniqueConnection);
    refreshAllowances();

    store->setCookieFilter([this, spaceId](const QWebEngineCookieStore::FilterRequest &request) {
        // A site's own state is its own business. Only a third party is asked
        // about, and the answer for one it has not been given is no.
        if (!request.thirdParty) {
            return true;
        }
        if (allows(spaceId, request.origin)) {
            return true;
        }
        m_refused.fetchAndAddRelaxed(1);
        rememberRefusal(request.firstPartyUrl, request.origin);
        return false;
    });
    return true;
}

int QtCookiePolicy::refusedCount() const
{
    return m_refused.loadRelaxed();
}

QStringList QtCookiePolicy::refusedOrigins(const QUrl &firstParty) const
{
    const auto page = cookieOrigin(firstParty);
    if (page.isEmpty()) {
        return {};
    }
    const QMutexLocker locker(&m_guard);
    return m_refusedOrigins.value(page);
}

void QtCookiePolicy::rememberRefusal(const QUrl &firstParty, const QUrl &origin)
{
    const auto page = cookieOrigin(firstParty);
    const auto named = cookieOrigin(origin);
    if (page.isEmpty() || named.isEmpty() || page == named) {
        return;
    }
    const QMutexLocker locker(&m_guard);
    auto &refused = m_refusedOrigins[page];
    if (refused.contains(named)) {
        return;
    }
    if (refused.size() >= refusedOriginsPerPage) {
        refused.removeFirst();
    }
    refused.append(named);
}

bool QtCookiePolicy::allows(const QString &spaceId, const QUrl &origin)
{
    const auto named = cookieOrigin(origin);
    if (named.isEmpty()) {
        return false;
    }
    const QMutexLocker locker(&m_guard);
    return m_allowed.contains(allowanceKey(spaceId, named));
}

void QtCookiePolicy::refreshAllowances()
{
    QSet<QString> allowed;
    for (auto it = m_attachments.cbegin(); it != m_attachments.cend(); ++it) {
        const auto &attachment = it.value();
        if (!attachment.controller) {
            continue;
        }
        for (const auto &origin :
            attachment.controller->allowedThirdPartyCookieOrigins(attachment.spaceId)) {
            allowed.insert(allowanceKey(attachment.spaceId, origin));
        }
    }
    const QMutexLocker locker(&m_guard);
    m_allowed = std::move(allowed);
}

} // namespace omaweb
