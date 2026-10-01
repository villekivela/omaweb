#include "BrowserController.h"
#include "ReleaseWatch.h"
#include "SpaceStorage.h"

#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using omaweb::BrowserController;
using omaweb::ReleaseWatch;
using omaweb::SpaceStorage;

namespace {

// One run of the browser on the profile under `root`. The Spaces can be
// arranged before `start`, which follows the browser as main.cpp has the
// watch do.
class Run final {
public:
    Run(const QTemporaryDir &root, const QString &version,
        ReleaseWatch::Ask ask = ReleaseWatch::Ask::GitHub)
        : browser(SpaceStorage(root.path(), QStringLiteral("test")))
        , watch(version, ask)
    {
        // The daily question goes to GitHub, which a test does not ask.
        // Turning it off leaves the launch to answer for itself.
        browser.setPreference(QStringLiteral("release-check"), QStringLiteral("false"));
    }

    void start() { watch.follow(&browser); }

    // Whether a Space holds a tab at this address.
    bool holds(const QString &spaceId, const QUrl &address) const
    {
        const auto tabs = browser.spaceTabs(spaceId);
        return std::ranges::any_of(tabs, [&](const auto &tab) { return tab.url == address; });
    }

    BrowserController browser;
    ReleaseWatch watch;
};

const QUrl notesOf090(QStringLiteral("https://omaweb.app/releases/v0.9.0/"));

} // namespace

class ReleaseWatchTest final : public QObject {
    Q_OBJECT

private slots:
    void opensTheNotesOnceAfterAnUpgrade();
    void opensTheNotesBehindThePageOnShow();
    void opensTheNotesInTheReadersOwnSpace_data();
    void opensTheNotesInTheReadersOwnSpace();
    void waitsForALaunchWithASpaceOfTheReaders();
    void opensNothingOnAFirstInstall();
    void opensNothingAfterADowngrade();
    void keepsTheNotesForALaunchThatOpenedNoWindowForThem();
    void aBuildThatDoesNotKnowItsVersionRecordsNoLaunch();

private:
    // The windows a run opens. Only an ordinary one asks for the notes.
    enum class Windows { Ordinary, PrivateOnly };

    // A run with no arrangement, answering what its first window opened.
    static QUrl launch(const QTemporaryDir &root, const QString &version,
        ReleaseWatch::Ask ask = ReleaseWatch::Ask::GitHub, Windows windows = Windows::Ordinary);
};

QUrl ReleaseWatchTest::launch(
    const QTemporaryDir &root, const QString &version, ReleaseWatch::Ask ask, Windows windows)
{
    Run run(root, version, ask);
    run.start();
    return windows == Windows::Ordinary ? run.watch.openUpgradeNotes() : QUrl();
}

void ReleaseWatchTest::opensTheNotesOnceAfterAnUpgrade()
{
    QTemporaryDir root;
    QVERIFY(launch(root, QStringLiteral("0.8.0")).isEmpty());

    {
        Run run(root, QStringLiteral("0.9.0"));
        run.start();
        QCOMPARE(run.watch.openUpgradeNotes(), notesOf090);
        // A second window in the same run is not a second upgrade.
        QVERIFY(run.watch.openUpgradeNotes().isEmpty());
    }
    QVERIFY(launch(root, QStringLiteral("0.9.0")).isEmpty());
}

// Beside the reader's tabs rather than in front of them: the page on show
// stays on show.
void ReleaseWatchTest::opensTheNotesBehindThePageOnShow()
{
    QTemporaryDir root;
    QVERIFY(launch(root, QStringLiteral("0.8.0")).isEmpty());

    Run run(root, QStringLiteral("0.9.0"));
    run.start();
    const auto spaceId = run.browser.activeSpaceId();
    const auto pageOnShow = run.browser.activeTabId();
    QCOMPARE(run.watch.openUpgradeNotes(), notesOf090);
    QVERIFY(run.holds(spaceId, notesOf090));
    QCOMPARE(run.browser.activeSpaceId(), spaceId);
    QCOMPARE(run.browser.activeTabId(), pageOnShow);
}

void ReleaseWatchTest::opensTheNotesInTheReadersOwnSpace_data()
{
    QTest::addColumn<bool>("temporary");
    QTest::newRow("an Agent Space") << false;
    QTest::newRow("a temporary Agent Space") << true;
}

// An Agent Space is the Agent's to fill, and a temporary one is deleted with
// what is in it. The notes go to the reader's own Space shown last.
void ReleaseWatchTest::opensTheNotesInTheReadersOwnSpace()
{
    QFETCH(bool, temporary);
    QTemporaryDir root;
    QVERIFY(launch(root, QStringLiteral("0.8.0")).isEmpty());

    Run run(root, QStringLiteral("0.9.0"));
    const auto workSpaceId = run.browser.createSpace(QStringLiteral("Work"));
    QVERIFY(run.browser.switchSpace(workSpaceId));
    const auto agentSpaceId = run.browser.createAgentSpace(
        QStringLiteral("Checks"), QStringLiteral("agent"), temporary);
    QVERIFY(run.browser.switchSpace(agentSpaceId));
    run.start();

    QCOMPARE(run.watch.openUpgradeNotes(), notesOf090);
    QVERIFY(run.holds(workSpaceId, notesOf090));
    QVERIFY(!run.holds(agentSpaceId, notesOf090));
    QCOMPARE(run.browser.activeSpaceId(), agentSpaceId);
}

// Every Space an Agent's leaves nowhere of the reader's to put the notes. They
// are not counted as opened, and a launch that has such a Space opens them.
void ReleaseWatchTest::waitsForALaunchWithASpaceOfTheReaders()
{
    QTemporaryDir root;
    QVERIFY(launch(root, QStringLiteral("0.8.0")).isEmpty());

    {
        Run run(root, QStringLiteral("0.9.0"));
        const auto personalSpaceId = run.browser.activeSpaceId();
        const auto agentSpaceId
            = run.browser.createAgentSpace(QStringLiteral("Checks"), QStringLiteral("agent"));
        QVERIFY(run.browser.switchSpace(agentSpaceId));
        QVERIFY(run.browser.deleteSpace(personalSpaceId, QStringLiteral("Personal")));
        run.start();
        QVERIFY(run.watch.openUpgradeNotes().isEmpty());
        QVERIFY(!run.holds(agentSpaceId, notesOf090));
    }

    Run run(root, QStringLiteral("0.9.0"));
    const auto homeSpaceId = run.browser.createSpace(QStringLiteral("Home"));
    run.start();
    QCOMPARE(run.watch.openUpgradeNotes(), notesOf090);
    QVERIFY(run.holds(homeSpaceId, notesOf090));
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
    // Back on the release the reader already had the notes for.
    QVERIFY(launch(root, QStringLiteral("0.9.0")).isEmpty());
}

// A run whose only window is a Private one never asks for the notes. The
// upgrade is still unread, and the next run's ordinary window opens them.
void ReleaseWatchTest::keepsTheNotesForALaunchThatOpenedNoWindowForThem()
{
    QTemporaryDir root;
    QVERIFY(launch(root, QStringLiteral("0.8.0")).isEmpty());
    launch(root, QStringLiteral("0.9.0"), ReleaseWatch::Ask::GitHub, Windows::PrivateOnly);
    QCOMPARE(launch(root, QStringLiteral("0.9.0")), notesOf090);
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
