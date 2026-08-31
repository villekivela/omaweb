#include "QtContentBlocker.h"

#include "ContentBlocker.h"

#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>

namespace tanto {
namespace {

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
        if (m_contentBlocker->shouldBlock(
                info.requestUrl(), info.firstPartyUrl(), info.resourceType())) {
            info.block(true);
        }
    }

private:
    QtContentBlocker *m_contentBlocker;
};

} // namespace

QtContentBlocker::QtContentBlocker(ContentBlocker *contentBlocker, QObject *parent)
    : QObject(parent)
    , m_contentBlocker(contentBlocker)
    , m_interceptor(std::make_unique<RequestInterceptor>(this))
{
}

bool QtContentBlocker::shouldBlock(const QUrl &requestUrl, const QUrl &sourceUrl,
    QWebEngineUrlRequestInfo::ResourceType resourceType) const
{
    return m_contentBlocker->shouldBlock(
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

bool QtContentBlocker::attachToProfile(QObject *profileObject)
{
    auto *profile = qobject_cast<QWebEngineProfile *>(profileObject);
    if (!profile) {
        return false;
    }
    profile->setUrlRequestInterceptor(m_interceptor.get());
    return true;
}

} // namespace tanto
