#pragma once

#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

namespace omaweb {

// An address as a Problem report and a Problem recording carry it (ADR 0068).
QString redactedAddress(const QUrl &url);
// Text with every address in it redacted, as a page's console line or a
// warning quotes one.
QString redactedText(const QString &text);

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
// The settings the reader changed: those `settings.json` in `configRoot` holds,
// less any `isDefault` says is at its default, which a file edited by hand can
// hold. A folder or an address is said to be set rather than named.
using ProblemReportDefault = std::function<bool(const QString &key, const QJsonValue &value)>;
QStringList changedSettingLines(
    const QString &configRoot, const ProblemReportDefault &isDefault = {});
// When an Omaweb process last crashed, which one, and the top of the crashing
// thread's backtrace, as systemd-coredump keeps them.
QStringList lastCrashLines(const ProblemReportRun &run);

// The directory the reader's settings are in: `OMAWEB_CONFIG_ROOT`, or
// `omaweb` in the XDG configuration directory.
QString configRootFromEnvironment();

// The report `omaweb report` prints with no browser running: what can be read
// from disk, and a line saying what only a running browser knows.
QList<ProblemReportSection> problemReportFromDisk(
    const QString &omawebVersion, const QString &configRoot, const ProblemReportRun &run);

} // namespace omaweb
