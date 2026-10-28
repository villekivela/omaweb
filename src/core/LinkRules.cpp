#include "LinkRules.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtNetwork/private/qtldurl_p.h>

namespace omaweb::linkrules {

namespace {

    // A pattern taken apart: the host, whether it takes the hosts under it,
    // and the path prefix without its `*`, or none.
    struct Pattern {
        QString host;
        bool subdomains = false;
        std::optional<QString> path;
    };

    std::optional<Pattern> parse(const QString &pattern)
    {
        // Labels of letters, digits and hyphens, then a path that holds no
        // query, fragment, space or second `*`.
        static const QRegularExpression grammar(
            QStringLiteral(R"(^(\*\.)?([a-z0-9-]+(?:\.[a-z0-9-]+)*)(/[^*?#\s]*\*)?$)"),
            QRegularExpression::CaseInsensitiveOption);
        const auto match = grammar.match(pattern);
        if (!match.hasMatch()) {
            return std::nullopt;
        }
        Pattern parsed {.host = match.captured(2).toLower(),
            .subdomains = match.hasCaptured(1),
            .path = std::nullopt};
        if (match.hasCaptured(3)) {
            parsed.path = match.captured(3).chopped(1);
        }
        return parsed;
    }

} // namespace

QString privateWindow() { return QStringLiteral("private"); }

bool parses(const QString &pattern) { return parse(pattern).has_value(); }

bool matches(const QString &pattern, const QUrl &url)
{
    const auto parsed = parse(pattern);
    if (!parsed
        || (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http"))) {
        return false;
    }
    // As the DNS writes it, which is how a pattern names a host outside
    // ASCII: QUrl shows some of those decoded.
    const auto host = url.host(QUrl::EncodeUnicode).toLower();
    const bool hostMatches
        = host == parsed->host || (parsed->subdomains && host.endsWith(u'.' + parsed->host));
    if (!hostMatches) {
        return false;
    }
    if (!parsed->path) {
        return true;
    }
    // `/acme/*` takes `/acme` too, which is the page the others sit under.
    const auto &prefix = *parsed->path;
    const auto path = url.path();
    return path.startsWith(prefix) || (prefix.endsWith(u'/') && path == prefix.chopped(1));
}

std::optional<QString> sitePattern(const QUrl &url)
{
    if (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http")) {
        return std::nullopt;
    }
    const auto host = url.host(QUrl::EncodeUnicode).toLower();
    if (host.isEmpty() || host.endsWith(u'.') || !QHostAddress(host).isNull()) {
        return std::nullopt;
    }
    const auto labels = host.split(u'.');
    if (labels.size() < 2 || qIsEffectiveTLD(host)) {
        return std::nullopt;
    }
    // The longest suffix the list names wins, so the walk starts from the
    // left.
    for (qsizetype first = 1; first < labels.size(); ++first) {
        const auto suffix = labels.sliced(first).join(u'.');
        if (qIsEffectiveTLD(suffix)) {
            return QStringLiteral("*.") + labels.sliced(first - 1).join(u'.');
        }
    }
    // The list's default rule: a top-level name it does not know is a public
    // suffix of its own.
    return QStringLiteral("*.") + labels.sliced(labels.size() - 2).join(u'.');
}

QList<LinkRule> read(const QJsonValue &value)
{
    QList<LinkRule> rules;
    for (const auto &entry : value.toArray()) {
        const auto rule = entry.toObject();
        rules.append({.pattern = rule.value(QLatin1String("pattern")).toString(),
            .space = rule.value(QLatin1String("space")).toString()});
    }
    return rules;
}

QJsonArray write(const QList<LinkRule> &rules)
{
    QJsonArray array;
    for (const auto &rule : rules) {
        array.append(QJsonObject {
            {QStringLiteral("pattern"), rule.pattern}, {QStringLiteral("space"), rule.space}});
    }
    return array;
}

std::optional<LinkRule> route(const QList<LinkRule> &rules, const QUrl &url)
{
    for (const auto &rule : rules) {
        if (matches(rule.pattern, url)) {
            return rule;
        }
    }
    return std::nullopt;
}

} // namespace omaweb::linkrules
