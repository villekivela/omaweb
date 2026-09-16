#include "QtCookiePolicy.h"

#include "BrowserController.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QWebEngineCookieStore>
#include <QtWebEngineQuick/QQuickWebEngineProfile>

#include <algorithm>

namespace omaweb {
namespace {

    // How many refused third parties one page is remembered by. Enough to name the
    // embedded flows a page actually has, and bounded so the table cannot become a
    // record of everywhere the reader has been.
    constexpr qsizetype refusedOriginsPerPage = 16;

    // The address as the engine spells it when it names a first party, so a
    // view's announcement and the filter's request meet on one string. The
    // fragment is off: a jump inside the document is the same document.
    QString setOutKey(const QUrl &url)
    {
        return url.toString(QUrl::FullyEncoded | QUrl::RemoveFragment);
    }

    // Whether a cookie for one host belongs to the site of another: the same
    // host, or one beneath the other. The engine's own answer reads the public
    // suffix list, which Qt keeps to itself, so two hosts that share only a
    // registrable domain are not matched here and a document arriving at
    // www.example.com still has a cookie api.example.com sets refused.
    bool sameSiteHost(const QString &host, const QString &site)
    {
        return host == site || host.endsWith(u'.' + site) || site.endsWith(u'.' + host);
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

bool QtCookiePolicy::attachToProfile(QObject *profile, QObject *controller, const QString &spaceId)
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

    m_attachments.insert(store, Attachment {browser, spaceId});
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
        if (arrivedAtOwnSite(request.firstPartyUrl, request.origin)
            || allows(spaceId, request.origin)) {
            return true;
        }
        m_refused.fetchAndAddRelaxed(1);
        rememberRefusal(request.firstPartyUrl, request.origin);
        return false;
    });
    return true;
}

bool QtCookiePolicy::deleteAllCookies(QObject *profile)
{
    auto *engineProfile = qobject_cast<QQuickWebEngineProfile *>(profile);
    if (!engineProfile || !engineProfile->cookieStore()) {
        return false;
    }
    engineProfile->cookieStore()->deleteAllCookies();
    return true;
}

int QtCookiePolicy::refusedCount() const { return m_refused.loadRelaxed(); }

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

void QtCookiePolicy::showDocument(QObject *view, const QUrl &setOutFrom, const QUrl &arrivedAt)
{
    if (!view) {
        return;
    }
    if (!m_documents.contains(view)) {
        connect(view, &QObject::destroyed, this, [this, view] { forgetDocument(view); });
    }
    const auto key = setOutKey(setOutFrom);
    const auto host = arrivedAt.host().toLower();
    // Arriving where it set out from is the ordinary load, which the engine
    // judges correctly and which needs no entry to say so.
    const auto redirected
        = !key.isEmpty() && !host.isEmpty() && !sameSiteHost(host, setOutFrom.host().toLower());
    const QMutexLocker locker(&m_guard);
    forgetArrival(m_documents.value(view));
    m_documents.insert(view, redirected ? Document {key, host} : Document {});
    if (redirected) {
        m_arrivals.insert(key, host);
    }
}

void QtCookiePolicy::forgetDocument(QObject *view)
{
    const QMutexLocker locker(&m_guard);
    forgetArrival(m_documents.take(view));
}

void QtCookiePolicy::forgetArrival(const Document &document)
{
    if (document.first.isEmpty()) {
        return;
    }
    // One entry, not every one alike: another view may have arrived the same
    // way and still be showing what it arrived at.
    const auto entry = m_arrivals.constFind(document.first, document.second);
    if (entry != m_arrivals.cend()) {
        m_arrivals.erase(entry);
    }
}

bool QtCookiePolicy::arrivedAtOwnSite(const QUrl &setOutFrom, const QUrl &origin) const
{
    const auto host = origin.host().toLower();
    if (host.isEmpty()) {
        return false;
    }
    const QMutexLocker locker(&m_guard);
    const auto sites = m_arrivals.values(setOutKey(setOutFrom));
    return std::any_of(sites.cbegin(), sites.cend(),
        [&host](const auto &site) { return sameSiteHost(host, site); });
}

bool QtCookiePolicy::allows(const QString &spaceId, const QUrl &origin)
{
    const auto named = cookieOrigin(origin);
    if (named.isEmpty()) {
        return false;
    }
    const QMutexLocker locker(&m_guard);
    return m_allowed.value(spaceId).contains(named);
}

void QtCookiePolicy::refreshAllowances()
{
    QHash<QString, QSet<QString>> allowed;
    for (auto it = m_attachments.cbegin(); it != m_attachments.cend(); ++it) {
        const auto &attachment = it.value();
        if (!attachment.controller) {
            continue;
        }
        const auto origins
            = attachment.controller->allowedThirdPartyCookieOrigins(attachment.spaceId);
        allowed[attachment.spaceId] = QSet<QString>(origins.cbegin(), origins.cend());
    }
    const QMutexLocker locker(&m_guard);
    m_allowed = std::move(allowed);
}

} // namespace omaweb
