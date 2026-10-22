#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace omaweb {

// An address as a Problem report and a Problem recording carry it (ADR 0068).
QString redactedAddress(const QUrl &url);

// One part of a Problem report: a title and its facts, one to a line.
struct ProblemReportSection {
    QString title;
    QStringList lines;
    // A Problem recording, which a link too short for it keeps the newest
    // lines of.
    bool trimOldest = false;
};

// How long the link to the bug form may be. GitHub takes a little more, and
// a link much longer is refused by browsers and proxies on the way.
inline constexpr qsizetype problemReportLinkLimit = 8000;

QString problemReportText(const QList<ProblemReportSection> &sections);

struct ProblemReportForm {
    QUrl address;
    // Whether something was left out, so the whole report has to reach the
    // form another way.
    bool cut = false;
};

ProblemReportForm problemReportForm(const QList<ProblemReportSection> &sections);

} // namespace omaweb
