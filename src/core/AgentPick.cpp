#include "AgentPick.h"

#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace omaweb {
namespace {

    // A title is one line of a row, and the answer is cut at its tabs.
    QString oneLine(const QString &text)
    {
        static const QRegularExpression separators(QStringLiteral("[\\p{Cc}\\s]+"));
        auto line = text;
        line.replace(separators, QStringLiteral(" "));
        return line.trimmed();
    }

    struct Candidate {
        QString id;
        QString title;
        // The subtext at each step of telling it from the others: the host and
        // the Space, then the address, then the id.
        QStringList subtexts;
        qsizetype step = 0;

        QString answer() const { return title + u'\t' + subtexts.at(step); }
    };

    // Nerd Font "web", beside every row.
    QString glyph() { return QString::fromUcs4(U"\U000f059f"); }

} // namespace

QString TabPicks::idFor(const QString &selection) const { return idsByAnswer.value(selection); }

TabPicks tabPicks(const QJsonArray &tabs)
{
    QList<Candidate> candidates;
    for (const auto &value : tabs) {
        const auto tab = value.toObject();
        const auto id = tab.value(QStringLiteral("id")).toString();
        const auto url = oneLine(tab.value(QStringLiteral("url")).toString());
        const auto title = oneLine(tab.value(QStringLiteral("title")).toString());
        const auto host = QUrl(url).host();
        auto where = host.isEmpty() ? url : host;
        const auto space = oneLine(tab.value(QStringLiteral("spaceName")).toString());
        if (!space.isEmpty()) {
            where += QStringLiteral(" · ") + space;
        }
        candidates.append({.id = id,
            .title = title.isEmpty() ? url : title,
            .subtexts = {where, where + QStringLiteral(" · ") + url,
                where + QStringLiteral(" · ") + url + QStringLiteral(" · ") + id}});
    }

    // Every group of rows that answer alike takes one step towards telling
    // them apart, until none does. The id is the last step and is its own.
    for (bool shared = true; shared;) {
        shared = false;
        QHash<QString, qsizetype> counts;
        for (const auto &candidate : candidates) {
            ++counts[candidate.answer()];
        }
        for (auto &candidate : candidates) {
            if (counts.value(candidate.answer()) > 1
                && candidate.step + 1 < candidate.subtexts.size()) {
                ++candidate.step;
                shared = true;
            }
        }
    }

    TabPicks picks;
    for (const auto &candidate : candidates) {
        picks.rows.append(
            glyph() + u'\t' + candidate.title + u'\t' + candidate.subtexts.at(candidate.step));
        picks.idsByAnswer.insert(candidate.answer(), candidate.id);
    }
    return picks;
}

} // namespace omaweb
