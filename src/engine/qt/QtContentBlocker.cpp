#include "QtContentBlocker.h"

#include "ContentBlocker.h"
#include "ContentMatcher.h"
#include "GlobalPrivacyControl.h"

#include <QBuffer>
#include <QPointer>
#include <QQuickWebEngineProfile>
#include <QWebEngineProfile>
#include <QWebEngineScriptCollection>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>

#include <algorithm>
#include <utility>

namespace omaweb {
namespace {

    // A page that waits on `analytics.js` and is handed nothing waits forever, so
    // a filter list can name a substitute to serve in its place. Chromium refuses
    // to redirect a request to a `data:` URL, which is the form the library hands
    // a body over in, so the substitutes get a scheme of their own.
    constexpr auto substituteScheme = "omaweb-resource";

    // How long refused addresses wait for company before the page is told.
    // Long enough that a page load's refusals arrive in a few batches rather
    // than hundreds, short enough that a broken-image icon is not seen first.
    constexpr int refusedElementFlushIntervalMilliseconds = 100;

    // Whether a request of this type is one an element made and is drawn by:
    // an image, a frame, a plugin's object, a video or an audio track. A
    // refused script or stylesheet leaves nothing in the layout to take out.
    bool drawnByAnElement(QWebEngineUrlRequestInfo::ResourceType type)
    {
        using Info = QWebEngineUrlRequestInfo;
        switch (type) {
        case Info::ResourceTypeImage:
        case Info::ResourceTypeSubFrame:
        case Info::ResourceTypeObject:
        case Info::ResourceTypePluginResource:
        case Info::ResourceTypeMedia:
            return true;
        default:
            return false;
        }
    }

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
        case Info::ResourceTypeMainFrame:
            return QStringLiteral("document");
        case Info::ResourceTypeSubFrame:
            return QStringLiteral("subdocument");
        case Info::ResourceTypeStylesheet:
            return QStringLiteral("stylesheet");
        case Info::ResourceTypeScript:
        case Info::ResourceTypeWorker:
        case Info::ResourceTypeSharedWorker:
        case Info::ResourceTypeServiceWorker:
            return QStringLiteral("script");
        case Info::ResourceTypeImage:
        case Info::ResourceTypeFavicon:
            return QStringLiteral("image");
        case Info::ResourceTypeFontResource:
            return QStringLiteral("font");
        case Info::ResourceTypeObject:
        case Info::ResourceTypePluginResource:
            return QStringLiteral("object");
        case Info::ResourceTypeMedia:
            return QStringLiteral("media");
        case Info::ResourceTypeXhr:
        case Info::ResourceTypeJson:
            return QStringLiteral("xmlhttprequest");
        case Info::ResourceTypePing:
            return QStringLiteral("ping");
        case Info::ResourceTypeCspReport:
            return QStringLiteral("csp_report");
        case Info::ResourceTypeWebSocket:
            return QStringLiteral("websocket");
        default:
            return QStringLiteral("other");
        }
    }

    class RequestInterceptor final : public QWebEngineUrlRequestInterceptor {
    public:
        RequestInterceptor(QtContentBlocker *contentBlocker, QString spaceId)
            : m_contentBlocker(contentBlocker)
            , m_spaceId(std::move(spaceId))
        {
        }

        void interceptRequest(QWebEngineUrlRequestInfo &info) override
        {
            // Set before the refusal is decided rather than after: a request
            // refused below never leaves, and one redirected below comes
            // through here again as the request that does.
            if (m_contentBlocker->sendsGlobalPrivacyControl()) {
                info.setHttpHeader(
                    GlobalPrivacyControl::headerName(), GlobalPrivacyControl::headerValue());
            }
            // Read before the answer is given: a redirect below replaces the
            // request address, and the element names the one it asked for.
            const auto requestUrl = info.requestUrl();
            const auto decision = m_contentBlocker->checkRequest(
                requestUrl, info.firstPartyUrl(), info.resourceType(), m_spaceId);
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
                // A substitute collapses the element too: a 1x1 stand-in is
                // as much of a hole in the page as a broken image.
                if (drawnByAnElement(info.resourceType())) {
                    m_contentBlocker->noteRefusedElement(
                        m_spaceId, info.firstPartyUrl(), requestUrl);
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
        QString m_spaceId;
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
        | QWebEngineUrlScheme::ContentSecurityPolicyIgnored | QWebEngineUrlScheme::CorsEnabled
        | QWebEngineUrlScheme::FetchApiAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
}

QtContentBlocker::QtContentBlocker(ContentBlocker *contentBlocker,
    const GlobalPrivacyControl *globalPrivacyControl, QObject *parent)
    : QObject(parent)
    , m_contentBlocker(contentBlocker)
    , m_globalPrivacyControl(globalPrivacyControl)
    , m_substitutes(std::make_unique<SubstituteSchemeHandler>())
{
    // Document creation is the only point early enough: the property has to
    // read `true` to the page's first script, and a consent script is often
    // that script. Every frame, because a third party's frame is where the
    // question is most often asked.
    m_globalPrivacyControlScript.setName(QStringLiteral("Omaweb Global Privacy Control"));
    m_globalPrivacyControlScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    m_globalPrivacyControlScript.setWorldId(QWebEngineScript::MainWorld);
    m_globalPrivacyControlScript.setRunsOnSubFrames(true);
    m_globalPrivacyControlScript.setSourceCode(GlobalPrivacyControl::scriptSource());
    m_refusedElementFlush.setSingleShot(true);
    m_refusedElementFlush.setInterval(refusedElementFlushIntervalMilliseconds);
    connect(
        &m_refusedElementFlush, &QTimer::timeout, this, &QtContentBlocker::flushRefusedElements);
    if (m_globalPrivacyControl) {
        connect(m_globalPrivacyControl, &GlobalPrivacyControl::enabledChanged, this,
            &QtContentBlocker::applyGlobalPrivacyControl);
        applyGlobalPrivacyControl();
    }
}

bool QtContentBlocker::sendsGlobalPrivacyControl() const
{
    return m_sendGlobalPrivacyControl.load(std::memory_order_relaxed);
}

void QtContentBlocker::applyGlobalPrivacyControl()
{
    const auto enabled = m_globalPrivacyControl && m_globalPrivacyControl->enabled();
    m_sendGlobalPrivacyControl.store(enabled, std::memory_order_relaxed);
    std::erase_if(m_profiles, [](const QPointer<QObject> &profile) { return profile.isNull(); });
    for (const auto &profile : m_profiles) {
        installGlobalPrivacyControlScript(profile.data(), enabled);
    }
}

// The QML profile's script collection is a class Qt keeps private, so it is
// reached through the meta-object, the way QML itself reaches it.
void QtContentBlocker::installGlobalPrivacyControlScript(QObject *profile, bool wanted) const
{
    if (auto *widgetProfile = qobject_cast<QWebEngineProfile *>(profile)) {
        if (wanted) {
            widgetProfile->scripts()->insert(m_globalPrivacyControlScript);
        } else {
            widgetProfile->scripts()->remove(m_globalPrivacyControlScript);
        }
        return;
    }
    auto *collection = profile->property("userScripts").value<QObject *>();
    // A name the collection no longer answers to would otherwise fail in
    // silence, and the page would go on reading `undefined` while the header
    // still says the reader opted out.
    if (!collection
        || !QMetaObject::invokeMethod(collection, wanted ? "insert" : "remove",
            Q_ARG(QWebEngineScript, m_globalPrivacyControlScript))) {
        qWarning("Global Privacy Control could not reach the profile's script collection.");
    }
}

// Requests are matched on whichever thread the engine hands them to, and the
// pending batch belongs to this object's thread. The page address loses its
// fragment here, the way the Refusal tally's key does: a fragment jump is the
// same document, and the view compares the address it is showing the same way.
void QtContentBlocker::noteRefusedElement(
    const QString &spaceId, const QUrl &pageAddress, const QUrl &requestUrl) const
{
    QPointer<QtContentBlocker> guard(const_cast<QtContentBlocker *>(this));
    const auto page = pageAddress.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
    const auto address = requestUrl.toString(QUrl::FullyEncoded);
    QMetaObject::invokeMethod(
        const_cast<QtContentBlocker *>(this),
        [guard, spaceId, page, address] {
            if (!guard) {
                return;
            }
            guard->m_pendingRefusedElements[{spaceId, page}].append(address);
            if (!guard->m_refusedElementFlush.isActive()) {
                guard->m_refusedElementFlush.start();
            }
        },
        Qt::QueuedConnection);
}

void QtContentBlocker::flushRefusedElements()
{
    m_refusedElementFlush.stop();
    const auto pending = std::exchange(m_pendingRefusedElements, {});
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        emit requestsRefused(it.key().first, QUrl(it.key().second), it.value());
    }
}

RequestDecision QtContentBlocker::checkRequest(const QUrl &requestUrl, const QUrl &sourceUrl,
    QWebEngineUrlRequestInfo::ResourceType resourceType, const QString &spaceId) const
{
    return m_contentBlocker->checkRequest(
        requestUrl, sourceUrl, resourceTypeName(resourceType), spaceId);
}

QString QtContentBlocker::cosmeticStyleSheet(const QUrl &url) const
{
    return m_contentBlocker->cosmeticStyleSheet(url);
}

bool QtContentBlocker::cosmeticSurveyWanted(const QUrl &url) const
{
    return m_contentBlocker->cosmeticSurveyWanted(url);
}

QString QtContentBlocker::genericCosmeticStyleSheet(
    const QUrl &url, const QStringList &classes, const QStringList &ids) const
{
    return m_contentBlocker->genericCosmeticStyleSheet(url, classes, ids);
}

QtContentBlocker::~QtContentBlocker() = default;

// QML's WebEngineProfile is QQuickWebEngineProfile, which is not a
// QWebEngineProfile and does not derive from one: the two are separate classes
// carrying the same two calls. Casting to one of them alone attaches to
// nothing and says so only through a return value QML ignores, which is
// content blocking that reports its rules and applies none of them.
bool QtContentBlocker::attachToProfile(QObject *profileObject, const QString &spaceId)
{
    auto &interceptor = m_interceptors[spaceId];
    if (!interceptor) {
        interceptor = std::make_unique<RequestInterceptor>(this, spaceId);
    }
    const auto attach = [this, &interceptor, profileObject](auto *profile) {
        profile->setUrlRequestInterceptor(interceptor.get());
        profile->installUrlSchemeHandler(substituteScheme, m_substitutes.get());
        const auto attached = std::ranges::any_of(m_profiles,
            [profileObject](const QPointer<QObject> &known) { return known == profileObject; });
        if (!attached) {
            m_profiles.emplace_back(profileObject);
            installGlobalPrivacyControlScript(profileObject, sendsGlobalPrivacyControl());
            emit profileAttached(profileObject);
        }
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
