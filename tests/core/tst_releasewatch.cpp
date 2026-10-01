#include "BrowserController.h"
#include "ReleaseWatch.h"
#include "SpaceStorage.h"

#include <QTemporaryDir>
#include <QTest>

using omaweb::BrowserController;
using omaweb::ReleaseWatch;
using omaweb::SpaceStorage;

class ReleaseWatchTest final : public QObject {
    Q_OBJECT

private slots:
    void opensTheNotesOnceAfterAnUpgrade();
    void opensNothingOnAFirstInstall();
    void opensNothingAfterADowngrade();
    void keepsTheNotesForALaunchThatOpenedNoWindowForThem();
    void aBuildThatDoesNotKnowItsVersionRecordsNoLaunch();

private:
    // One run of the browser on the profile under `root`: the watch follows
    // the browser as main.cpp has it do, and a window asks for the notes once,
    // or not at all when `windowAsks` is false.
    static QUrl launch(const QTemporaryDir &root, const QString &version,
        ReleaseWatch::Ask ask = ReleaseWatch::Ask::GitHub, bool windowAsks = true);
};

QUrl ReleaseWatchTest::launch(
    const QTemporaryDir &root, const QString &version, ReleaseWatch::Ask ask, bool windowAsks)
{
    BrowserController browser(SpaceStorage(root.path(), QStringLiteral("test")));
    // The daily question goes to GitHub, which a test does not ask. Turning it
    // off leaves the launch to answer for itself.
    browser.setPreference(QStringLiteral("release-check"), QStringLiteral("false"));
    ReleaseWatch watch(version, ask);
    watch.follow(&browser);
    return windowAsks ? watch.takeUpgradeNotes() : QUrl();
}

void ReleaseWatchTest::opensTheNotesOnceAfterAnUpgrade()
{
    QTemporaryDir root;
    QVERIFY(launch(root, QStringLiteral("0.8.0")).isEmpty());

    {
        BrowserController browser(SpaceStorage(root.path(), QStringLiteral("test")));
        browser.setPreference(QStringLiteral("release-check"), QStringLiteral("false"));
        ReleaseWatch watch(QStringLiteral("0.9.0"));
        watch.follow(&browser);
        QCOMPARE(
            watch.takeUpgradeNotes(), QUrl(QStringLiteral("https://omaweb.app/releases/v0.9.0/")));
        // A second window in the same run is not a second upgrade.
        QVERIFY(watch.takeUpgradeNotes().isEmpty());
    }
    QVERIFY(launch(root, QStringLiteral("0.9.0")).isEmpty());
}

void ReleaseWatchTest::opensNothingOnAFirstInstall()
{
    QTemporaryDir root;
    QVERIFY(launch(root, QStringLiteral("0.9.0")).isEmpty());
    // Launched again, the same release is not an upgrade either.
    QVERIFY(launch(root, QStringLiteral("0.9.0")).isEmpty());
    // The first launch was remembered, so the next release is one.
    QCOMPARE(launch(root, QStringLiteral("0.10.0")),
        QUrl(QStringLiteral("https://omaweb.app/releases/v0.10.0/")));
    QVERIFY(launch(root, QStringLiteral("0.10.0")).isEmpty());
}

void ReleaseWatchTest::opensNothingAfterADowngrade()
{
    QTemporaryDir root;
    QVERIFY(launch(root, QStringLiteral("0.9.0")).isEmpty());
    QVERIFY(launch(root, QStringLiteral("0.8.1")).isEmpty());
}

// A run whose only window is a Private one never asks for the notes. The
// upgrade is still unread, and the next run's ordinary window opens them.
void ReleaseWatchTest::keepsTheNotesForALaunchThatOpenedNoWindowForThem()
{
    QTemporaryDir root;
    QVERIFY(launch(root, QStringLiteral("0.8.0")).isEmpty());
    launch(root, QStringLiteral("0.9.0"), ReleaseWatch::Ask::GitHub, false);
    QCOMPARE(launch(root, QStringLiteral("0.9.0")),
        QUrl(QStringLiteral("https://omaweb.app/releases/v0.9.0/")));
}

// A tagless tree builds with a fallback version that reads like a release. It
// is not one, and remembering it would make the next real launch look like an
// upgrade or a downgrade from it.
void ReleaseWatchTest::aBuildThatDoesNotKnowItsVersionRecordsNoLaunch()
{
    QTemporaryDir root;
    QVERIFY(launch(root, QStringLiteral("0.9.0")).isEmpty());
    QVERIFY(launch(root, QStringLiteral("0.10.0"), ReleaseWatch::Ask::Never).isEmpty());
    QVERIFY(launch(root, QStringLiteral("0.1.0"), ReleaseWatch::Ask::Never).isEmpty());
    QVERIFY(launch(root, QStringLiteral("0.9.0")).isEmpty());
}

QTEST_GUILESS_MAIN(ReleaseWatchTest)

#include "tst_releasewatch.moc"
