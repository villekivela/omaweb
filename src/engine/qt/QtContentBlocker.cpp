#include "QtContentBlocker.h"

#include "ContentBlocker.h"
#include "ContentMatcher.h"

#include <QBuffer>
#include <QQuickWebEngineProfile>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>

namespace omaweb {
namespace {

// A page that waits on `analytics.js` and is handed nothing waits forever, so
// a filter list can name a substitute to serve in its place. Chromium refuses
// to redirect a request to a `data:` URL, which is the form the library hands
// a body over in, so the substitutes get a scheme of their own.
constexpr auto substituteScheme = "omaweb-resource";

// The address one substitute is served at. A canonical resource name is a
// bare filename — `noop.js`, `1x1.gif` — so the whole address is the scheme
// and the name, and a replaced request stays legible in a network log as the
// resource that replaced it.
QUrl substituteUrl(const QString &name)
{
    return QUrl(QLatin1String(substituteScheme) + QLatin1Char(':') + name);
}

QString resourceTypeName(QWebEngineUrlRequestInfo::ResourceType type)
{
    using Info = QWebEngineUrlRequestInfo;
    switch (type) {
    case Info::ResourceTypeMainFrame: return QStringLiteral("document");
    case Info::ResourceTypeSubFrame: return QStringLiteral("subdocument");
    case Info::ResourceTypeStylesheet: return QStringLiteral("stylesheet");
    case Info::ResourceTypeScript:
    case Info::ResourceTypeWorker:
    case Info::ResourceTypeSharedWorker:
    case Info::ResourceTypeServiceWorker: return QStringLiteral("script");
    case Info::ResourceTypeImage:
    case Info::ResourceTypeFavicon: return QStringLiteral("image");
    case Info::ResourceTypeFontResource: return QStringLiteral("font");
    case Info::ResourceTypeObject:
    case Info::ResourceTypePluginResource: return QStringLiteral("object");
    case Info::ResourceTypeMedia: return QStringLiteral("media");
    case Info::ResourceTypeXhr:
    case Info::ResourceTypeJson: return QStringLiteral("xmlhttprequest");
    case Info::ResourceTypePing: return QStringLiteral("ping");
    case Info::ResourceTypeCspReport: return QStringLiteral("csp_report");
    case Info::ResourceTypeWebSocket: return QStringLiteral("websocket");
    default: return QStringLiteral("other");
    }
}

class RequestInterceptor final : public QWebEngineUrlRequestInterceptor {
public:
    explicit RequestInterceptor(QtContentBlocker *contentBlocker)
        : m_contentBlocker(contentBlocker)
    {
    }

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        const auto decision = m_contentBlocker->checkRequest(
            info.requestUrl(), info.firstPartyUrl(), info.resourceType());
        // Chromium drops a redirect on a request carrying a payload, and says
        // so only in a warning. Both answers below are redirects, so a request
        // that cannot take one falls back to what it can take.
        const auto redirectable = info.requestMethod() == "GET";
        if (decision.blocked) {
            if (decision.substitute.isEmpty() || !redirectable) {
                info.block(true);
            } else {
                info.redirect(substituteUrl(decision.substitute));
            }
            return;
        }
        // Not a refusal: the request goes out, with the tracking parameters a
        // rule named stripped off its address.
        if (!decision.rewrittenUrl.isEmpty() && redirectable) {
            info.redirect(decision.rewrittenUrl);
        }
    }

private:
    QtContentBlocker *m_contentBlocker;
};

// Serves one substitute body out of the vendored library, under its own name
// and with its own MIME type. The library is a constant built into the binary,
// so this handler outlives any particular rule set and never consults one.
class SubstituteSchemeHandler final : public QWebEngineUrlSchemeHandler {
public:
    void requestStarted(QWebEngineUrlRequestJob *job) override
    {
        const auto substitute = ContentMatcher::substitute(job->requestUrl().path());
        if (!substitute.isValid()) {
            job->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
        }
        auto *body = new QBuffer(job);
        body->setData(substitute.body);
        body->open(QIODevice::ReadOnly);
        job->reply(substitute.mimeType, body);
    }
};

} // namespace

// Chromium learns its schemes once, before it starts. A substitute is served
// to a page that may well be https and may well carry a strict policy of its
// own, and the request that asked for it may have been a fetch, so the scheme
// has to be as capable as the request it stands in for.
void QtContentBlocker::registerSubstituteScheme()
{
    if (QWebEngineUrlScheme::schemeByName(substituteScheme).name() == substituteScheme) {
        return;
    }
    QWebEngineUrlScheme scheme(substituteScheme);
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Path);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme
        | QWebEngineUrlScheme::ContentSecurityPolicyIgnored
        | QWebEngineUrlScheme::CorsEnabled
        | QWebEngineUrlScheme::FetchApiAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
}

QtContentBlocker::QtContentBlocker(ContentBlocker *contentBlocker, QObject *parent)
    : QObject(parent)
    , m_contentBlocker(contentBlocker)
    , m_interceptor(std::make_unique<RequestInterceptor>(this))
    , m_substitutes(std::make_unique<SubstituteSchemeHandler>())
{
}

RequestDecision QtContentBlocker::checkRequest(const QUrl &requestUrl, const QUrl &sourceUrl,
    QWebEngineUrlRequestInfo::ResourceType resourceType) const
{
    return m_contentBlocker->checkRequest(
        requestUrl, sourceUrl, resourceTypeName(resourceType));
}

QString QtContentBlocker::cosmeticStyleSheet(const QUrl &url) const
{
    return m_contentBlocker->cosmeticStyleSheet(url);
}

bool QtContentBlocker::cosmeticSurveyWanted(const QUrl &url) const
{
    return m_contentBlocker->cosmeticSurveyWanted(url);
}

QString QtContentBlocker::genericCosmeticStyleSheet(const QUrl &url, const QStringList &classes,
    const QStringList &ids) const
{
    return m_contentBlocker->genericCosmeticStyleSheet(url, classes, ids);
}

QtContentBlocker::~QtContentBlocker() = default;

// QML's WebEngineProfile is QQuickWebEngineProfile, which is not a
// QWebEngineProfile and does not derive from one: the two are separate classes
// carrying the same two calls. Casting to one of them alone attaches to
// nothing and says so only through a return value QML ignores, which is
// content blocking that reports its rules and applies none of them.
bool QtContentBlocker::attachToProfile(QObject *profileObject)
{
    const auto attach = [this](auto *profile) {
        profile->setUrlRequestInterceptor(m_interceptor.get());
        profile->installUrlSchemeHandler(substituteScheme, m_substitutes.get());
        return true;
    };
    if (auto *profile = qobject_cast<QWebEngineProfile *>(profileObject)) {
        return attach(profile);
    }
    if (auto *profile = qobject_cast<QQuickWebEngineProfile *>(profileObject)) {
        return attach(profile);
    }
    return false;
}

} // namespace omaweb
