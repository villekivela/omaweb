#include "ProblemRecording.h"

#include <QDir>
#include <QDirIterator>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <chrono>

using namespace std::chrono_literals;
using omaweb::ProblemRecording;

class ProblemRecordingTest final : public QObject {
    Q_OBJECT

private slots:
    void recordsOnlyWhileRunning();
    void capturesWhatHappenedInTheTabs();
    void namesTabsByWhenTheyFirstAppear();
    void endsAfterTenMinutes();
    void endingOnItsOwnKeepsWhatItRecorded();
    void aReportTakesTheRecordingAndEndsIt();
    void keepsOmawebsOwnWarningsOnlyWhileRunning();
    void leavesNothingOnDisk();
};

void ProblemRecordingTest::recordsOnlyWhileRunning()
{
    ProblemRecording recording;
    QSignalSpy changed(&recording, &ProblemRecording::recordingChanged);

    recording.record(QStringLiteral("navigated"), QStringLiteral("a"),
        QUrl(QStringLiteral("https://example.com/")), {});
    QVERIFY(!recording.recording());
    recording.toggle();
    QVERIFY(recording.recording());
    recording.record(QStringLiteral("navigated"), QStringLiteral("a"),
        QUrl(QStringLiteral("https://example.org/")), {});
    recording.toggle();
    QVERIFY(!recording.recording());
    QCOMPARE(changed.count(), 2);

    const auto lines = recording.take();
    QCOMPARE(lines.size(), 1);
    QVERIFY(lines.first().contains(QStringLiteral("https://example.org/")));
}

// What #711 and #746 needed: where the tab went, where it was sent on, the
// hand-off to Slack, what a permission prompt was answered and what the page
// logged as an error. Addresses are redacted as the report's are.
void ProblemRecordingTest::capturesWhatHappenedInTheTabs()
{
    ProblemRecording recording;
    recording.start();
    const auto tab = QStringLiteral("tab-1");
    recording.record(QStringLiteral("navigated"), tab,
        QUrl(QStringLiteral("https://app.slack.com/signin?team=T0123456789ABC#x")), {});
    recording.record(QStringLiteral("redirected"), tab,
        QUrl(QStringLiteral("https://slack.com/ssb/redirect?rrk=46")), {});
    recording.record(QStringLiteral("handed off"), tab,
        QUrl(QStringLiteral("slack://magic-login/abcdef123456")), QStringLiteral("allowed"));
    recording.record(QStringLiteral("permission answered"), tab,
        QUrl(QStringLiteral("https://app.slack.com/")), QStringLiteral("notifications: allow"));
    recording.record(QStringLiteral("console error"), tab, {},
        QStringLiteral("Uncaught TypeError: x is undefined"));

    const auto lines = recording.take();

    QCOMPARE(lines.size(), 5);
    const QRegularExpression timed(QStringLiteral("^\\+\\d\\d:\\d\\d\\.\\d\\d\\d tab 1 "));
    for (const auto &line : lines) {
        QVERIFY2(timed.match(line).hasMatch(), qPrintable(line));
    }
    QVERIFY(lines.at(0).endsWith(QStringLiteral("navigated https://app.slack.com/signin?team=…")));
    QVERIFY(
        lines.at(1).endsWith(QStringLiteral("redirected https://slack.com/ssb/redirect?rrk=46")));
    QVERIFY(lines.at(2).endsWith(
        QStringLiteral("handed off slack://magic-login/abcdef123456: allowed")));
    QVERIFY(lines.at(3).endsWith(
        QStringLiteral("permission answered https://app.slack.com/: notifications: allow")));
    QVERIFY(
        lines.at(4).endsWith(QStringLiteral("console error: Uncaught TypeError: x is undefined")));
}

void ProblemRecordingTest::namesTabsByWhenTheyFirstAppear()
{
    ProblemRecording recording;
    recording.start();
    for (const auto *tab : {"{f00}", "{ba7}", "{f00}"}) {
        recording.record(QStringLiteral("load failed"), QString::fromLatin1(tab), {}, {});
    }

    const auto lines = recording.take();

    QVERIFY(lines.at(0).contains(QStringLiteral(" tab 1 ")));
    QVERIFY(lines.at(1).contains(QStringLiteral(" tab 2 ")));
    QVERIFY(lines.at(2).contains(QStringLiteral(" tab 1 ")));
    for (const auto &line : lines) {
        QVERIFY(!line.contains(QStringLiteral("f00")));
    }
}

void ProblemRecordingTest::endsAfterTenMinutes()
{
    ProblemRecording recording;
    const auto before = QDateTime::currentDateTimeUtc();
    recording.start();

    QCOMPARE(recording.limit(), 10min);
    const auto ends = recording.endsAt();
    QVERIFY(ends >= before.addSecs(600));
    QVERIFY(ends <= QDateTime::currentDateTimeUtc().addSecs(600));
}

void ProblemRecordingTest::endingOnItsOwnKeepsWhatItRecorded()
{
    ProblemRecording recording(nullptr, 50ms);
    recording.start();
    recording.record(QStringLiteral("navigated"), QStringLiteral("a"),
        QUrl(QStringLiteral("https://example.com/")), {});

    QTRY_VERIFY(!recording.recording());
    recording.record(QStringLiteral("navigated"), QStringLiteral("a"),
        QUrl(QStringLiteral("https://example.org/")), {});

    const auto lines = recording.take();
    QCOMPARE(lines.size(), 1);
    QVERIFY(lines.first().contains(QStringLiteral("example.com")));
}

void ProblemRecordingTest::aReportTakesTheRecordingAndEndsIt()
{
    ProblemRecording recording;
    recording.start();
    recording.record(QStringLiteral("navigated"), QStringLiteral("a"),
        QUrl(QStringLiteral("https://example.com/")), {});

    QCOMPARE(recording.take().size(), 1);
    QVERIFY(!recording.recording());
    QVERIFY(recording.take().isEmpty());
}

void ProblemRecordingTest::keepsOmawebsOwnWarningsOnlyWhileRunning()
{
    ProblemRecording recording;
    qWarning("before the recording");
    recording.start();
    qWarning("the engine could not start a renderer");
    qDebug("a debug line");
    recording.stop();
    qWarning("after the recording");

    QCOMPARE(recording.take().size(), 1);
    recording.start();
    qWarning("the engine could not start a renderer");
    const auto lines = recording.take();
    QCOMPARE(lines.size(), 1);
    QVERIFY(lines.first().endsWith(
        QStringLiteral("Omaweb warning: the engine could not start a renderer")));
}

void ProblemRecordingTest::leavesNothingOnDisk()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    for (const auto *variable : {"HOME", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME",
             "XDG_STATE_HOME", "TMPDIR"}) {
        qputenv(variable, QFile::encodeName(home.path()));
    }
    {
        ProblemRecording recording;
        recording.start();
        for (auto index = 0; index < 200; ++index) {
            recording.record(QStringLiteral("navigated"), QStringLiteral("a"),
                QUrl(QStringLiteral("https://example.com/%1").arg(index)), {});
        }
        qWarning("a warning while recording");
        recording.stop();
        QCOMPARE(recording.take().size(), 201);
    }

    QDirIterator written(home.path(), QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
        QDirIterator::Subdirectories);
    QVERIFY2(!written.hasNext(), qPrintable(written.hasNext() ? written.next() : QString()));
}

QTEST_GUILESS_MAIN(ProblemRecordingTest)
#include "tst_problemrecording.moc"
