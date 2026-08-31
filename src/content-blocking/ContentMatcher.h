#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>

namespace tanto {

struct MatcherCompilation {
    std::shared_ptr<const class ContentMatcher> matcher;
    QJsonObject report;
};

class ContentMatcher final {
public:
    static MatcherCompilation compile(const QString &rules);

    ~ContentMatcher();
    ContentMatcher(const ContentMatcher &) = delete;
    ContentMatcher &operator=(const ContentMatcher &) = delete;

    bool shouldBlock(const QUrl &requestUrl, const QUrl &sourceUrl,
        const QString &resourceType) const;
    QString cosmeticStyleSheet(const QUrl &url) const;
    bool cosmeticSurveyWanted(const QUrl &url) const;
    QString genericCosmeticStyleSheet(const QUrl &url, const QStringList &classes,
        const QStringList &ids) const;

private:
    class Private;
    explicit ContentMatcher(std::unique_ptr<Private> data);
    std::unique_ptr<Private> d;
};

} // namespace tanto
