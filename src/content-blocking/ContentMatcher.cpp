#include "ContentMatcher.h"

#include "tanto_blocker.h"

#include <QJsonArray>
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

namespace {

// The decision owns its strings until it is released, so these are copies
// rather than a handover.
QString decoded(const char *value)
{
    return value ? QString::fromUtf8(value) : QString();
}

} // namespace

RequestDecision ContentMatcher::check(const QUrl &requestUrl, const QUrl &sourceUrl,
    const QString &resourceType) const
{
    const auto request = requestUrl.toString(QUrl::FullyEncoded).toUtf8();
    const auto source = sourceUrl.toString(QUrl::FullyEncoded).toUtf8();
    const auto type = resourceType.toUtf8();
    TantoBlockerDecision answer{};
    tanto_blocker_check(d->blocker, request.constData(), source.constData(), type.constData(),
        &answer);
    RequestDecision decision;
    decision.blocked = answer.blocked;
    decision.substitute = decoded(answer.substitute);
    const auto rewritten = decoded(answer.rewritten_url);
    if (!rewritten.isEmpty()) {
        decision.rewrittenUrl = QUrl(rewritten);
    }
    tanto_blocker_decision_release(&answer);
    return decision;
}

// The library hands a body over as a `data:` URL carrying the resource's own
// MIME type, which is the one form both engines' resource storage already
// speaks. Serving it means taking the two apart again.
Substitute ContentMatcher::substitute(const QString &name)
{
    const auto encodedName = name.toUtf8();
    auto *encoded = tanto_blocker_substitute(encodedName.constData());
    if (!encoded) {
        return {};
    }
    const QByteArray dataUrl(encoded);
    tanto_blocker_string_free(encoded);
    constexpr QByteArrayView prefix = "data:";
    constexpr QByteArrayView separator = ";base64,";
    const auto mark = dataUrl.indexOf(separator);
    if (!dataUrl.startsWith(prefix) || mark < 0) {
        return {};
    }
    const auto mimeType = dataUrl.mid(prefix.size(), mark - prefix.size());
    if (mimeType.isEmpty()) {
        return {};
    }
    return {mimeType, QByteArray::fromBase64(dataUrl.mid(mark + separator.size()))};
}

// The lists' $popup rules, asked about with the window's address as the
// request and the page that asked for it as the source.
bool ContentMatcher::shouldBlockPopup(const QUrl &requestUrl, const QUrl &openerUrl) const
{
    const auto request = requestUrl.toString(QUrl::FullyEncoded).toUtf8();
    const auto opener = openerUrl.toString(QUrl::FullyEncoded).toUtf8();
    return tanto_blocker_matches_popup(d->blocker, request.constData(), opener.constData());
}

namespace {

QByteArray encodedNames(const QStringList &names)
{
    return QJsonDocument(QJsonArray::fromStringList(names)).toJson(QJsonDocument::Compact);
}

} // namespace

// The rules written against this page's hostname, plus the generic rules whose
// selectors no class or id survey could match. Everything else generic waits
// for genericCosmeticStyleSheet.
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

// The code the lists' `##+js(...)` rules ask for on this page: the source of
// each named function from the vendored library, plus the calls. Nothing a list
// wrote is in it — a rule contributes a name and its arguments.
QString ContentMatcher::scriptletSource(const QUrl &url) const
{
    const auto encodedUrl = url.toString(QUrl::FullyEncoded).toUtf8();
    auto *source = tanto_blocker_scriptlet_source(d->blocker, encodedUrl.constData());
    if (!source) {
        return {};
    }
    const auto result = QString::fromUtf8(source);
    tanto_blocker_string_free(source);
    return result;
}

bool ContentMatcher::cosmeticSurveyWanted(const QUrl &url) const
{
    const auto encodedUrl = url.toString(QUrl::FullyEncoded).toUtf8();
    return tanto_blocker_cosmetic_survey_wanted(d->blocker, encodedUrl.constData());
}

QString ContentMatcher::genericCosmeticStyleSheet(const QUrl &url, const QStringList &classes,
    const QStringList &ids) const
{
    const auto encodedUrl = url.toString(QUrl::FullyEncoded).toUtf8();
    const auto encodedClasses = encodedNames(classes);
    const auto encodedIds = encodedNames(ids);
    auto *css = tanto_blocker_generic_cosmetic_css(d->blocker, encodedUrl.constData(),
        encodedClasses.constData(), encodedIds.constData());
    if (!css) {
        return {};
    }
    const auto result = QString::fromUtf8(css);
    tanto_blocker_string_free(css);
    return result;
}

} // namespace tanto
