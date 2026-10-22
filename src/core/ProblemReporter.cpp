#include "ProblemReporter.h"

#include "BrowserController.h"
#include "PageScale.h"
#include "ProblemRecording.h"
#include "SpaceListModel.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace omaweb {

namespace {

    QString decisionName(int decision)
    {
        switch (decision) {
        case BrowserController::AllowOnce:
            return QStringLiteral("allowed once");
        case BrowserController::AllowPersistently:
            return QStringLiteral("allowed");
        case BrowserController::Block:
            return QStringLiteral("blocked");
        default:
            return QStringLiteral("ask");
        }
    }

    QStringList siteLines(const QVariantMap &sites)
    {
        static const QList<std::pair<QString, QString>> facts {
            {QStringLiteral("userAgent"), QStringLiteral("User agent")},
            {QStringLiteral("brands"), QStringLiteral("Brands")},
            {QStringLiteral("fullVersionList"), QStringLiteral("Full version list")},
            {QStringLiteral("platform"), QStringLiteral("Platform")},
            {QStringLiteral("language"), QStringLiteral("Language")},
        };
        QStringList lines;
        for (const auto &[key, name] : facts) {
            const auto value = sites.value(key).toString();
            lines.append(name + QStringLiteral(": ")
                + (value.isEmpty() ? QStringLiteral("not read") : value));
        }
        return lines;
    }

} // namespace

ProblemReporter::ProblemReporter(BrowserController *browser, QString configRoot, QObject *parent)
    : QObject(parent)
    , m_browser(browser)
    , m_configRoot(std::move(configRoot))
    , m_run(&runForProblemReport)
{
}

void ProblemReporter::setEngineVersions(QStringList lines) { m_engineVersions = std::move(lines); }

void ProblemReporter::setSites(const QVariantMap &sites) { m_sites = sites; }

void ProblemReporter::setSync(QObject *controller) { m_sync = controller; }

void ProblemReporter::setRecording(ProblemRecording *recording) { m_recording = recording; }

void ProblemReporter::setPageScale(const PageScale *scale) { m_pageScale = scale; }

void ProblemReporter::setRun(ProblemReportRun run) { m_run = std::move(run); }

QList<ProblemReportSection> ProblemReporter::sections(const QString &tabId, bool privateWindow)
{
    QList<ProblemReportSection> report {
        {.title = QStringLiteral("Versions"),
            .lines = m_engineVersions + systemVersionLines(m_run)},
        {.title = QStringLiteral("What sites see"), .lines = siteLines(m_sites)},
    };
    if (privateWindow || !tabId.isEmpty()) {
        report.append({.title = QStringLiteral("Tab"), .lines = tabLines(tabId, privateWindow)});
    }
    report.append({
        {.title = QStringLiteral("Settings"), .lines = changedSettingLines(m_configRoot)},
        {.title = QStringLiteral("Extensions"), .lines = extensionLines()},
        {.title = QStringLiteral("Spaces and tabs"), .lines = sessionLines()},
        {.title = QStringLiteral("Sync"), .lines = syncLines()},
        {.title = QStringLiteral("Last crash"), .lines = lastCrashLines(m_run)},
    });
    const auto recorded = m_recording ? m_recording->take() : QStringList {};
    if (!recorded.isEmpty()) {
        report.append(
            {.title = QStringLiteral("Problem recording"), .lines = recorded, .trimOldest = true});
    }
    return report;
}

QString ProblemReporter::text(const QString &tabId, bool privateWindow)
{
    return problemReportText(sections(tabId, privateWindow));
}

QVariantMap ProblemReporter::form(const QString &tabId, bool privateWindow)
{
    const auto report = sections(tabId, privateWindow);
    const auto form = problemReportForm(report);
    return {
        {QStringLiteral("address"), form.address},
        {QStringLiteral("cut"), form.cut},
        {QStringLiteral("text"), problemReportText(report)},
    };
}

QStringList ProblemReporter::tabLines(const QString &tabId, bool privateWindow) const
{
    if (privateWindow) {
        return {QStringLiteral("In a Private window")};
    }
    const auto tab = m_browser ? m_browser->findTab(tabId) : std::nullopt;
    if (!tab) {
        return {QStringLiteral("Closed before the report was written")};
    }
    QStringList lines {
        QStringLiteral("Address: %1").arg(redactedAddress(tab->url)),
        QStringLiteral("Space: %1")
            .arg(m_browser->agentSpace(tab->spaceId) ? QStringLiteral("Agent")
                                                     : QStringLiteral("ordinary")),
    };
    // Permissions are listed for the Space on show, which a tab the report
    // starts from is in.
    if (tab->spaceId == m_browser->activeSpaceId()) {
        for (const auto &entry : m_browser->sitePermissions(tab->url)) {
            const auto permission = entry.toMap();
            lines.append(permission.value(QStringLiteral("permission")).toString()
                + QStringLiteral(": ")
                + decisionName(permission.value(QStringLiteral("decision")).toInt()));
        }
    }
    lines.append(QStringLiteral("Zoom: %1%").arg(qRound(tab->zoom * 100)));
    lines.append(QStringLiteral("Page scale: %1%")
            .arg(qRound((m_pageScale ? m_pageScale->value() : 1.0) * 100)));
    return lines;
}

QStringList ProblemReporter::syncLines() const
{
    if (!m_sync) {
        return {QStringLiteral("Not loaded")};
    }
    if (!m_sync->property("enabled").toBool()) {
        return {QStringLiteral("Off")};
    }
    QStringList lines {QStringLiteral("On: %1").arg(m_sync->property("status").toString())};
    const auto error = m_sync->property("errorMessage").toString();
    if (!error.isEmpty()) {
        lines.append(QStringLiteral("Last error: %1").arg(error));
    }
    const auto synced = m_sync->property("lastSuccessfulSync").toDateTime();
    if (synced.isValid()) {
        lines.append(QStringLiteral("Last synced: %1").arg(synced.toUTC().toString(Qt::ISODate)));
    }
    return lines;
}

QStringList ProblemReporter::extensionLines() const
{
    QStringList lines;
    for (const auto &entry : m_browser ? m_browser->knownExtensions() : QVariantList {}) {
        const auto extension = entry.toMap();
        const auto enabled = extension.value(QStringLiteral("enabled")).toBool();
        if (!enabled && !extension.value(QStringLiteral("installed")).toBool()) {
            continue;
        }
        QFile manifest(QDir(extension.value(QStringLiteral("path")).toString())
                .filePath(QStringLiteral("manifest.json")));
        const auto version = manifest.open(QIODevice::ReadOnly)
            ? QJsonDocument::fromJson(manifest.readAll())
                  .object()
                  .value(QStringLiteral("version"))
                  .toString()
            : QString {};
        lines.append(QStringLiteral("%1 %2, %3")
                .arg(extension.value(QStringLiteral("name")).toString(),
                    version.isEmpty() ? QStringLiteral("not fetched") : version,
                    enabled ? QStringLiteral("on") : QStringLiteral("off")));
    }
    if (lines.isEmpty()) {
        lines.append(QStringLiteral("None"));
    }
    return lines;
}

QStringList ProblemReporter::sessionLines() const
{
    if (!m_browser) {
        return {};
    }
    const auto *spaces = m_browser->spaces();
    qsizetype tabs = 0;
    for (int row = 0; row < spaces->rowCount(); ++row) {
        tabs += m_browser->spaceTabs(spaces->index(row, 0).data(SpaceListModel::IdRole).toString())
                    .size();
    }
    return {QStringLiteral("%1 %2, %3 %4")
            .arg(spaces->rowCount())
            .arg(spaces->rowCount() == 1 ? QStringLiteral("Space") : QStringLiteral("Spaces"))
            .arg(tabs)
            .arg(tabs == 1 ? QStringLiteral("tab") : QStringLiteral("tabs"))};
}

} // namespace omaweb
