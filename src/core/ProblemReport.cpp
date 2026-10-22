#include "ProblemReport.h"

#include <QRegularExpression>
#include <QUrlQuery>

namespace omaweb {

namespace {

    constexpr qsizetype plainValueLength = 16;

    // A number, or a word made of letters and the marks a locale or a flow
    // name is written with: `46`, `en-GB`, `GlifWebSignIn`. Digits among
    // letters are what a token looks like, so they are not plain.
    bool plain(const QString &value)
    {
        static const QRegularExpression number(QStringLiteral("^[0-9]+$"));
        static const QRegularExpression word(QStringLiteral("^[A-Za-z]+([-_.][A-Za-z]+)*$"));
        return value.size() < plainValueLength
            && (number.match(value).hasMatch() || word.match(value).hasMatch());
    }

} // namespace

// What the activity log keeps of an address, without user information or a
// fragment, and the query too: its names, and the values a report needs.
QString redactedAddress(const QUrl &url)
{
    const auto bare
        = url.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment).toString();
    if (!url.hasQuery()) {
        return bare;
    }
    QStringList pairs;
    for (const auto &[name, value] : QUrlQuery(url).queryItems(QUrl::FullyDecoded)) {
        const auto shown = QString::fromLatin1(QUrl::toPercentEncoding(name));
        if (value.isEmpty()) {
            pairs.append(shown);
        } else {
            pairs.append(shown + u'=' + (plain(value) ? value : QStringLiteral("…")));
        }
    }
    return bare + u'?' + pairs.join(u'&');
}

QString problemReportText(const QList<ProblemReportSection> &sections)
{
    QStringList parts;
    for (const auto &section : sections) {
        auto part = section.title + u'\n';
        for (const auto &line : section.lines) {
            part += QStringLiteral("- ") + line + u'\n';
        }
        parts.append(part);
    }
    return parts.join(u'\n');
}

// Filled in the order the sections come, whole, until one does not fit. That
// one is left out with all after it, except a recording, which keeps as many
// of its newest lines as fit.
ProblemReportForm problemReportForm(const QList<ProblemReportSection> &sections)
{
    const auto address = [](const QString &report) {
        // Encoded whole, so a `&` or a `+` in the report stays in it.
        return QUrl::fromEncoded(
            "https://github.com/villekivela/omaweb/issues/new?template=bug.yml&report="
                + QUrl::toPercentEncoding(report),
            QUrl::StrictMode);
    };
    const auto fits = [&address](const QString &report) {
        return address(report).toEncoded().size() <= problemReportLinkLimit;
    };
    const auto whole = problemReportText(sections);
    if (fits(whole)) {
        return {.address = address(whole), .cut = false};
    }
    // The line is a fact about the report rather than a word of the chrome:
    // the report is written in English for the tracker.
    const auto pasteBelow = QStringLiteral(
        "\nCut to fit the link. The whole report is on the clipboard: paste it below.\n");
    QList<ProblemReportSection> kept;
    for (const auto &section : sections) {
        auto candidate = kept;
        candidate.append(section);
        if (fits(problemReportText(candidate) + pasteBelow)) {
            kept = candidate;
            continue;
        }
        if (section.trimOldest) {
            auto trimmed = section;
            trimmed.lines.clear();
            for (auto line = section.lines.crbegin(); line != section.lines.crend(); ++line) {
                auto longer = trimmed;
                longer.lines.prepend(*line);
                auto attempt = kept;
                attempt.append(longer);
                if (!fits(problemReportText(attempt) + pasteBelow)) {
                    break;
                }
                trimmed = longer;
            }
            if (!trimmed.lines.isEmpty()) {
                kept.append(trimmed);
            }
        }
        break;
    }
    return {.address = address(problemReportText(kept) + pasteBelow), .cut = true};
}

} // namespace omaweb
