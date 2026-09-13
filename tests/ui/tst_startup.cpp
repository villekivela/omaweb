#include "HistoryQuery.h"
#include "PerformanceProbe.h"
#include "SqliteSessionStore.h"
#include "TabListModel.h"

#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <expected>

using omaweb::SpaceState;
using omaweb::SqliteSessionStore;
using omaweb::TabState;

namespace {

// Thresholds set at four times the first measurement, recorded with it in the
// development guide's performance section. The margin is for the CI runner,
// a slower machine drawing through the software rasteriser, which the probe
// still has to pass on.
constexpr double startupThresholdMilliseconds = 2200.0;
constexpr double sessionRestoreThresholdMilliseconds = 2500.0;

// How many launches a number is taken over. Startup is dominated by loading
// the QML tree, which the first launch also warms the disk cache for, so the
// median of three is the warm figure rather than the coldest or the luckiest.
constexpr int launches = 3;

// The Space the restore probe brings back: the closed-tab stack at its bound,
// history at its retained bound, and more open tabs than the sidebar can show
// at once. The open-tab count has no bound of its own, so this is the working
// day the probe stands for rather than a limit the store enforces.
constexpr int openTabs = 100;
constexpr int pinnedTabs = 5;
constexpr int closedTabs = 25;

struct StartupReport {
    double firstFrameMilliseconds = -1;
    double pageFrameMilliseconds = -1;
};

// Launches the UI lab against a data root and reads back what it reports:
// when it first drew its window, and when it first drew a page in it.
std::expected<StartupReport, QString> launchLab(const QString &dataRoot)
{
    QProcess lab;
    lab.setProgram(QStringLiteral(OMAWEB_UI_LAB_EXECUTABLE));
    lab.setArguments({QStringLiteral("--report-startup"), QStringLiteral("--data-root"), dataRoot});
    lab.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    lab.start();
    if (!lab.waitForFinished(60000)) {
        lab.kill();
        return std::unexpected(
            QStringLiteral("the UI lab did not finish: %1").arg(lab.errorString()));
    }
    if (lab.exitStatus() != QProcess::NormalExit || lab.exitCode() != 0) {
        return std::unexpected(QStringLiteral("the UI lab exited with %1").arg(lab.exitCode()));
    }
    StartupReport report;
    const auto output = QString::fromUtf8(lab.readAllStandardOutput());
    for (const auto &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const auto parts = line.split(QLatin1Char('='));
        if (parts.size() != 2) {
            continue;
        }
        if (parts.first() == QLatin1String("first_frame_milliseconds")) {
            report.firstFrameMilliseconds = parts.last().toDouble();
        } else if (parts.first() == QLatin1String("page_frame_milliseconds")) {
            report.pageFrameMilliseconds = parts.last().toDouble();
        }
    }
    if (report.firstFrameMilliseconds < 0) {
        return std::unexpected(
            QStringLiteral("the UI lab reported no first frame:\n%1").arg(output));
    }
    return report;
}

double median(QList<double> values)
{
    std::sort(values.begin(), values.end());
    return values.at(values.size() / 2);
}

} // namespace

class StartupProbes final : public QObject {
    Q_OBJECT

private slots:
    void aFreshWindowIsDrawnInsideItsBudget();
    void aSpaceAtItsBoundsIsRestoredInsideItsBudget();
};

// Startup to first window drawn: the browser's chrome, brought up on an empty
// data root the way a first launch is. The lab is the browser without an
// engine, so this is the cost of Omaweb's own startup rather than Chromium's.
void StartupProbes::aFreshWindowIsDrawnInsideItsBudget()
{
    QList<double> samples;
    for (int launch = 0; launch < launches; ++launch) {
        QTemporaryDir dataRoot;
        QVERIFY(dataRoot.isValid());
        const auto report = launchLab(dataRoot.path());
        QVERIFY2(report.has_value(), qPrintable(report.error_or(QString())));
        samples.append(report->firstFrameMilliseconds);
    }
    const auto measured = median(samples);
    QVERIFY2(measured <= startupThresholdMilliseconds,
        qPrintable(omaweb::probe::report(QStringLiteral("startup-to-first-frame"), measured,
            QStringLiteral("ms"), startupThresholdMilliseconds)));
}

// Session restore: launch to the visible Space's page drawn, for a Space with
// a working day's tabs, its closed-tab stack full, and history at its bound.
// Nothing about the Space is loaded lazily on the way to that first page, so
// this is what a reader with a full Space waits for every morning.
void StartupProbes::aSpaceAtItsBoundsIsRestoredInsideItsBudget()
{
    QTemporaryDir dataRoot;
    QVERIFY(dataRoot.isValid());
    const auto spaceId = QStringLiteral("space-1");
    {
        SqliteSessionStore store(dataRoot.path());
        QVERIFY(store.open());
        SpaceState space;
        space.id = spaceId;
        space.name = QStringLiteral("Work");
        space.color = QStringLiteral("#7c6cff");
        space.active = true;
        QVERIFY(store.saveSpace(space));
        QVERIFY(store.setActiveSpace(spaceId));

        QVector<TabState> tabs;
        for (int index = 0; index < openTabs; ++index) {
            TabState tab;
            tab.id = QStringLiteral("tab-%1").arg(index);
            tab.spaceId = spaceId;
            tab.url = QUrl(QStringLiteral("https://site-%1.example/article").arg(index));
            tab.title = QStringLiteral("Article %1").arg(index);
            tab.pinned = index < pinnedTabs;
            tab.active = index == pinnedTabs;
            tabs.append(tab);
        }
        QVERIFY(store.saveTabs(spaceId, tabs, QStringLiteral("tab-%1").arg(pinnedTabs)));

        QVector<TabState> closed;
        for (int index = 0; index < closedTabs; ++index) {
            TabState tab;
            tab.id = QStringLiteral("closed-%1").arg(index);
            tab.spaceId = spaceId;
            tab.url = QUrl(QStringLiteral("https://closed-%1.example/").arg(index));
            tab.title = QStringLiteral("Closed %1").arg(index);
            closed.append(tab);
        }
        QVERIFY(store.recordClosedTabs(spaceId, closed));

        for (int index = 0; index < omaweb::history::retainedRows; ++index) {
            QVERIFY(store.recordVisit(spaceId,
                QUrl(QStringLiteral("https://visited-%1.example/page").arg(index)),
                QStringLiteral("Visit %1").arg(index)));
        }
    }

    QList<double> samples;
    for (int launch = 0; launch < launches; ++launch) {
        const auto report = launchLab(dataRoot.path());
        QVERIFY2(report.has_value(), qPrintable(report.error_or(QString())));
        // A restore that drew no page restored nothing worth timing.
        QVERIFY2(report->pageFrameMilliseconds >= 0, "the restored Space drew no page");
        samples.append(report->pageFrameMilliseconds);
    }
    const auto measured = median(samples);
    QVERIFY2(measured <= sessionRestoreThresholdMilliseconds,
        qPrintable(omaweb::probe::report(QStringLiteral("session-restore-to-page-frame"), measured,
            QStringLiteral("ms"), sessionRestoreThresholdMilliseconds)));
}

QTEST_MAIN(StartupProbes)

#include "tst_startup.moc"
