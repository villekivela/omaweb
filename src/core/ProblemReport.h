#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

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

// Runs a program for what it prints, or nothing when it is not there, fails or
// takes more than a few seconds.
using ProblemReportRun
    = std::function<QString(const QString &program, const QStringList &arguments)>;
QString runForProblemReport(const QString &program, const QStringList &arguments);

// The versions a report reads from the system rather than from the browser:
// Qt, Omarchy, Hyprland, the kernel and the architecture.
QStringList systemVersionLines(const ProblemReportRun &run);
// The settings the reader changed, which are those `settings.json` in
// `configRoot` holds. A folder or an address is said to be set rather than
// named.
QStringList changedSettingLines(const QString &configRoot);
// When an Omaweb process last crashed, which one, and the top of the crashing
// thread's backtrace, as systemd-coredump keeps them.
QStringList lastCrashLines(const ProblemReportRun &run);

// The directory the reader's settings are in: `OMAWEB_CONFIG_ROOT`, or
// `omaweb` in the XDG configuration directory.
QString omawebConfigRoot();

// The report `omaweb report` prints with no browser running: what can be read
// from disk, and a line saying what only a running browser knows.
QList<ProblemReportSection> problemReportFromDisk(
    const QString &omawebVersion, const QString &configRoot, const ProblemReportRun &run);

} // namespace omaweb
