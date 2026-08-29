#include "ContentMatcher.h"

#include "tanto_blocker.h"

#include <QJsonDocument>

namespace tanto {

class ContentMatcher::Private final {
public:
    explicit Private(TantoBlocker *value)
        : blocker(value)
    {
    }

    ~Private()
    {
        tanto_blocker_destroy(blocker);
    }

    TantoBlocker *blocker = nullptr;
};

ContentMatcher::ContentMatcher(std::unique_ptr<Private> data)
    : d(std::move(data))
{
}

ContentMatcher::~ContentMatcher() = default;

MatcherCompilation ContentMatcher::compile(const QString &rules)
{
    const auto encodedRules = rules.toUtf8();
    char *encodedReport = nullptr;
    auto *blocker = tanto_blocker_compile(encodedRules.constData(), &encodedReport);
    QJsonObject report;
    if (encodedReport) {
        report = QJsonDocument::fromJson(QByteArray(encodedReport)).object();
        tanto_blocker_string_free(encodedReport);
    }
    if (!blocker) {
        return {{}, report};
    }
    return {
        std::shared_ptr<const ContentMatcher>(
            new ContentMatcher(std::make_unique<Private>(blocker))),
        report,
    };
}

bool ContentMatcher::shouldBlock(const QUrl &requestUrl, const QUrl &sourceUrl,
    const QString &resourceType) const
{
    const auto request = requestUrl.toString(QUrl::FullyEncoded).toUtf8();
    const auto source = sourceUrl.toString(QUrl::FullyEncoded).toUtf8();
    const auto type = resourceType.toUtf8();
    return tanto_blocker_matches(d->blocker, request.constData(), source.constData(),
        type.constData());
}

QString ContentMatcher::cosmeticStyleSheet(const QUrl &url) const
{
    const auto encodedUrl = url.toString(QUrl::FullyEncoded).toUtf8();
    auto *css = tanto_blocker_cosmetic_css(d->blocker, encodedUrl.constData());
    if (!css) {
        return {};
    }
    const auto result = QString::fromUtf8(css);
    tanto_blocker_string_free(css);
    return result;
}

} // namespace tanto
