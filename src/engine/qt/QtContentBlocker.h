#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QWebEngineUrlRequestInfo>

#include <map>
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

    // One Space's profile and the Space it belongs to. A Private window has no
    // Space of its own and passes the empty name its shared session already
    // keys on, the same way QtCookiePolicy is told.
    Q_INVOKABLE bool attachToProfile(QObject *profile, const QString &spaceId);
    RequestDecision checkRequest(const QUrl &requestUrl, const QUrl &sourceUrl,
        QWebEngineUrlRequestInfo::ResourceType resourceType, const QString &spaceId) const;
    QString cosmeticStyleSheet(const QUrl &url) const;
    bool cosmeticSurveyWanted(const QUrl &url) const;
    QString genericCosmeticStyleSheet(
        const QUrl &url, const QStringList &classes, const QStringList &ids) const;

private:
    ContentBlocker *m_contentBlocker;
    // An interceptor for each Space, because the interceptor is the only thing
    // that sees a request and Chromium tells it nothing about where the
    // request came from. Interception attaches to a profile and a profile
    // belongs to a Space (ADR 0037), so the Space is what the interceptor
    // carries and one shared instance cannot.
    std::map<QString, std::unique_ptr<QWebEngineUrlRequestInterceptor>> m_interceptors;
    std::unique_ptr<QWebEngineUrlSchemeHandler> m_substitutes;
};

} // namespace omaweb
