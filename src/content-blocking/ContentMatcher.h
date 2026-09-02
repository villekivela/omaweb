#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>

namespace omaweb {

// What the lists have to say about one request, in Qt's types. The three
// answers are independent and each is dropped where it would mean nothing;
// `omaweb_blocker.h` carries the reasoning.
struct RequestDecision {
    bool blocked = false;
    // The substitute to serve instead, by name, or empty when the request is
    // simply refused. Never set without blocked.
    QString substitute;
    // The request address with the tracking parameters removed, or empty when
    // no rule changed it. Never set together with blocked.
    QUrl rewrittenUrl;
};

// One substitute body out of the vendored library, ready to serve.
struct Substitute {
    QByteArray mimeType;
    QByteArray body;

    bool isValid() const { return !mimeType.isEmpty(); }
};

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

    // The substitute the vendored library carries under this name, or an
    // invalid one for a name it carries none for. The library is a constant
    // built into the binary, so this answers without a compiled rule set.
    static Substitute substitute(const QString &name);

    RequestDecision check(const QUrl &requestUrl, const QUrl &sourceUrl,
        const QString &resourceType) const;
    bool shouldBlockPopup(const QUrl &requestUrl, const QUrl &openerUrl) const;
    QString cosmeticStyleSheet(const QUrl &url) const;
    QString scriptletSource(const QUrl &url) const;
    bool cosmeticSurveyWanted(const QUrl &url) const;
    QString genericCosmeticStyleSheet(const QUrl &url, const QStringList &classes,
        const QStringList &ids) const;

private:
    class Private;
    explicit ContentMatcher(std::unique_ptr<Private> data);
    std::unique_ptr<Private> d;
};

} // namespace omaweb
