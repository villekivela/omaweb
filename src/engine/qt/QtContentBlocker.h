#pragma once

#include <QObject>
#include <QUrl>
#include <QWebEngineUrlRequestInfo>

#include <memory>

class QWebEngineUrlRequestInterceptor;
class QWebEngineUrlSchemeHandler;

namespace omaweb {

class ContentBlocker;
struct RequestDecision;

class QtContentBlocker final : public QObject {
    Q_OBJECT

public:
    // The scheme a substitute is served under. Chromium refuses to redirect a
    // request to a `data:` URL, and it has to learn about a scheme of its own
    // before it starts, so this runs before QtWebEngineQuick::initialize().
    static void registerSubstituteScheme();

    explicit QtContentBlocker(ContentBlocker *contentBlocker, QObject *parent = nullptr);
    ~QtContentBlocker() override;

    Q_INVOKABLE bool attachToProfile(QObject *profile);
    RequestDecision checkRequest(const QUrl &requestUrl, const QUrl &sourceUrl,
        QWebEngineUrlRequestInfo::ResourceType resourceType) const;
    QString cosmeticStyleSheet(const QUrl &url) const;
    bool cosmeticSurveyWanted(const QUrl &url) const;
    QString genericCosmeticStyleSheet(
        const QUrl &url, const QStringList &classes, const QStringList &ids) const;

private:
    ContentBlocker *m_contentBlocker;
    std::unique_ptr<QWebEngineUrlRequestInterceptor> m_interceptor;
    std::unique_ptr<QWebEngineUrlSchemeHandler> m_substitutes;
};

} // namespace omaweb
