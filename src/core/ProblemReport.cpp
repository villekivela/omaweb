#include "ProblemReport.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTimeZone>
#include <QUrlQuery>

#include <csignal>

namespace omaweb {

namespace {

    constexpr qsizetype plainValueLength = 16;
    constexpr int runTimeoutMs = 3000;
    // The frames of a crash a report keeps, from the top.
    constexpr qsizetype crashFrames = 8;

    QString signalName(int number)
    {
        switch (number) {
        case SIGSEGV:
            return QStringLiteral("SIGSEGV");
        case SIGABRT:
            return QStringLiteral("SIGABRT");
        case SIGBUS:
            return QStringLiteral("SIGBUS");
        case SIGILL:
            return QStringLiteral("SIGILL");
        case SIGFPE:
            return QStringLiteral("SIGFPE");
        case SIGTRAP:
            return QStringLiteral("SIGTRAP");
        default:
            return QStringLiteral("signal %1").arg(number);
        }
    }

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

    // A setting's value as a report may carry it. A list or an object, which
    // is where sites, fonts and per-site values live, is counted. An address
    // is redacted. Other text stays when it is a plain word, such as a choice,
    // and is said to be set when it may be a name, a folder or an account.
    QString shownSetting(const QJsonValue &value)
    {
        if (value.isObject() || value.isArray()) {
            const auto count = value.isObject() ? value.toObject().size() : value.toArray().size();
            return count == 1 ? QStringLiteral("set, 1 entry")
                              : QStringLiteral("set, %1 entries").arg(count);
        }
        if (!value.isString()) {
            return value.toVariant().toString();
        }
        const auto text = value.toString();
        if (text.isEmpty()) {
            return QStringLiteral("none");
        }
        if (text.contains(QStringLiteral("://"))) {
            return redactedAddress(QUrl(text));
        }
        return plain(text) ? text : QStringLiteral("set");
    }

} // namespace

// Without user information or a fragment, and with the path's segments, the
// query's names and the values a report needs.
QString redactedAddress(const QUrl &url)
{
    // The host stays whatever it is: it names the site or the application a
    // hand-off went to, which is what a report is about.
    auto redacted = url.adjusted(QUrl::RemoveUserInfo | QUrl::RemovePath | QUrl::RemoveQuery
                           | QUrl::RemoveFragment)
                        .toString();
    QStringList segments;
    for (const auto &segment : url.path(QUrl::FullyEncoded).split(u'/')) {
        const auto decoded = QUrl::fromPercentEncoding(segment.toLatin1());
        segments.append(decoded.isEmpty() || plain(decoded) ? segment : QStringLiteral("…"));
    }
    redacted += segments.join(u'/');
    if (!url.hasQuery()) {
        return redacted;
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
    return redacted + u'?' + pairs.join(u'&');
}

QString redactedText(const QString &text)
{
    // An address runs to white space or a quote. A mark that ends a sentence
    // is the sentence's.
    static const QRegularExpression address(
        QStringLiteral("\\b[a-z][a-z0-9+.-]*://[^\\s\"'<>]*[^\\s\"'<>.,;:!?)]"),
        QRegularExpression::CaseInsensitiveOption);
    QString redacted;
    qsizetype from = 0;
    for (const auto &match : address.globalMatch(text)) {
        redacted += text.mid(from, match.capturedStart() - from);
        redacted += redactedAddress(QUrl(match.captured()));
        from = match.capturedEnd();
    }
    return redacted + text.mid(from);
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

QString runForProblemReport(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setStandardInputFile(QProcess::nullDevice());
    process.start();
    if (!process.waitForFinished(runTimeoutMs) || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        process.kill();
        process.waitForFinished();
        return {};
    }
    return QString::fromLocal8Bit(process.readAllStandardOutput());
}

QStringList systemVersionLines(const ProblemReportRun &run)
{
    const auto missing = QStringLiteral("not found");
    const auto omarchy = run(QStringLiteral("omarchy-version"), {}).trimmed();
    // `Hyprland 0.56.2 built from branch …`: the second word.
    const auto hyprland = run(QStringLiteral("Hyprland"), {QStringLiteral("--version")})
                              .section(u'\n', 0, 0)
                              .section(u' ', 1, 1);
    return {
        QStringLiteral("Qt %1").arg(QString::fromLatin1(qVersion())),
        QStringLiteral("Omarchy %1").arg(omarchy.isEmpty() ? missing : omarchy),
        QStringLiteral("Hyprland %1").arg(hyprland.isEmpty() ? missing : hyprland),
        QStringLiteral("Kernel %1").arg(QSysInfo::kernelVersion()),
        QStringLiteral("Architecture %1").arg(QSysInfo::currentCpuArchitecture()),
    };
}

QStringList changedSettingLines(const QString &configRoot, const ProblemReportDefault &isDefault)
{
    QFile file(QDir(configRoot).filePath(QStringLiteral("settings.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        return {QStringLiteral("All at their defaults")};
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {QStringLiteral("settings.json does not parse: %1").arg(error.errorString())};
    }
    QStringList lines;
    const auto settings = document.object();
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        if (it.key() == u"version" || it.key() == u"known-extensions"
            || (isDefault && isDefault(it.key(), *it))) {
            continue;
        }
        lines.append(it.key() + QStringLiteral(": ") + shownSetting(*it));
    }
    if (lines.isEmpty()) {
        lines.append(QStringLiteral("All at their defaults"));
    }
    return lines;
}

QStringList lastCrashLines(const ProblemReportRun &run)
{
    const auto list = QJsonDocument::fromJson(run(QStringLiteral("coredumpctl"),
        {QStringLiteral("--json=short"), QStringLiteral("--no-pager"), QStringLiteral("list")})
            .toUtf8());
    QJsonObject last;
    for (const auto &entry : list.array()) {
        const auto program = QFileInfo(entry.toObject().value(QStringLiteral("exe")).toString());
        // The browser, and the engine's processes, which run from its folder.
        if (program.fileName().startsWith(QStringLiteral("omaweb"))
            || program.absolutePath().contains(QStringLiteral("omaweb"))) {
            last = entry.toObject();
        }
    }
    if (last.isEmpty()) {
        return {QStringLiteral("None that systemd-coredump keeps")};
    }
    const auto pid = QString::number(last.value(QStringLiteral("pid")).toInteger());
    const auto when = QDateTime::fromMSecsSinceEpoch(
        last.value(QStringLiteral("time")).toInteger() / 1000, QTimeZone::UTC);
    QStringList lines {QStringLiteral("%1, %2, at %3")
            .arg(QFileInfo(last.value(QStringLiteral("exe")).toString()).fileName(),
                signalName(last.value(QStringLiteral("sig")).toInt()), when.toString(Qt::ISODate))};
    // The first thread's trace is the one that crashed. The rest of what
    // `info` says, the command line above all, may name a page.
    const auto info = run(
        QStringLiteral("coredumpctl"), {QStringLiteral("info"), QStringLiteral("--no-pager"), pid});
    auto inTrace = false;
    for (const auto &line : info.split(u'\n')) {
        const auto trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("Stack trace of thread"))) {
            if (inTrace) {
                break;
            }
            inTrace = true;
            continue;
        }
        if (!inTrace) {
            continue;
        }
        if (trimmed.isEmpty() || lines.size() > crashFrames) {
            break;
        }
        // `#0  0x00007f1 function (library + 0x1)`, without the address,
        // which differs from run to run.
        static const QRegularExpression frame(QStringLiteral("^(#\\d+)\\s+0x[0-9a-f]+\\s+(.*)$"));
        const auto match = frame.match(trimmed);
        // A frame without symbols names nothing a reader of the report could
        // look up.
        if (match.hasMatch() && !match.captured(2).startsWith(QStringLiteral("n/a"))) {
            lines.append(match.captured(1) + u' ' + match.captured(2));
        }
    }
    return lines;
}

QString configRootFromEnvironment()
{
    const auto override = qEnvironmentVariable("OMAWEB_CONFIG_ROOT");
    if (!override.isEmpty()) {
        return override;
    }
    const auto xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    const auto base = xdg.isEmpty() ? QDir::home().filePath(QStringLiteral(".config")) : xdg;
    return QDir(base).filePath(QStringLiteral("omaweb"));
}

QList<ProblemReportSection> problemReportFromDisk(
    const QString &omawebVersion, const QString &configRoot, const ProblemReportRun &run)
{
    return {
        {.title = QStringLiteral("Versions"),
            .lines = QStringList {QStringLiteral("Omaweb %1, the client").arg(omawebVersion)}
                + systemVersionLines(run)},
        {.title = QStringLiteral("Settings"), .lines = changedSettingLines(configRoot)},
        {.title = QStringLiteral("Last crash"), .lines = lastCrashLines(run)},
        {.title = QStringLiteral("Not running"),
            .lines = {QStringLiteral("No browser answered, so the engine's versions, what sites "
                                     "see, the tab, Sync, extensions, Spaces and tabs and any "
                                     "Problem recording are missing.")}},
    };
}

} // namespace omaweb
