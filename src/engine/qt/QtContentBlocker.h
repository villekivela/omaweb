#pragma once

#include <QObject>
#include <QUrl>
#include <QWebEngineUrlRequestInfo>

#include <memory>

class QWebEngineUrlRequestInterceptor;

namespace tanto {

class ContentBlocker;

class QtContentBlocker final : public QObject {
    Q_OBJECT

public:
    explicit QtContentBlocker(ContentBlocker *contentBlocker, QObject *parent = nullptr);
    ~QtContentBlocker() override;

    Q_INVOKABLE bool attachToProfile(QObject *profile);
    bool shouldBlock(const QUrl &requestUrl, const QUrl &sourceUrl,
        QWebEngineUrlRequestInfo::ResourceType resourceType) const;
    QString cosmeticStyleSheet(const QUrl &url) const;
    bool cosmeticSurveyWanted(const QUrl &url) const;
    QString genericCosmeticStyleSheet(const QUrl &url, const QStringList &classes,
        const QStringList &ids) const;

private:
    ContentBlocker *m_contentBlocker;
    std::unique_ptr<QWebEngineUrlRequestInterceptor> m_interceptor;
};

} // namespace tanto
