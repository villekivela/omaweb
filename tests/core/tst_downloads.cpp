#include "BrowserController.h"
#include "Downloads.h"
#include "PrivateSessionFixture.h"
#include "PrivateSessionStore.h"
#include "SpaceStorage.h"
#include "SqliteSessionStore.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QSharedPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using omaweb::BrowserController;
using omaweb::DownloadPermissions;
using omaweb::Downloads;
using omaweb::PrivateSessionStore;
using omaweb::SpaceStorage;
using omaweb::SqliteSessionStore;
using omaweb::test::PrivateSessionFixture;

namespace {

// A window that answers what an origin has been allowed and nothing else. The
// list needs this much of a window and no more, so the tests that are about the
// list stand one of these up rather than a browser.
class StubHost final : public DownloadPermissions {
public:
    QString permissionOrigin(const QUrl &url) const override
    {
        return url.isValid() && !url.host().isEmpty()
            ? url.scheme() + QStringLiteral("://") + url.host()
            : QString {};
    }

    bool originInteracted(const QUrl &url) const override
    {
        return interacted.contains(permissionOrigin(url));
    }

    int automaticDownloadDecision(const QString &origin) const override
    {
        return decisions.value(origin, BrowserController::Ask);
    }

    bool rememberAutomaticDownloadDecision(const QString &origin, int decision) override
    {
        decisions.insert(origin, decision);
        remembered += 1;
        return true;
    }

    QSet<QString> interacted;
    QHash<QString, int> decisions;
    int remembered = 0;
};

QVariant roleOf(const Downloads &downloads, int row, Downloads::Role role)
{
    return downloads.data(downloads.index(row), role);
}

} // namespace

class DownloadsTest final : public QObject {
    Q_OBJECT

private slots:
    void holdsRunningAndRecordedDownloadsInOneList();
    void tellsTwoPrivateDownloadsApartWithoutARecord();
    void publishesTheDownloadActivityWithoutBeingRefreshed();
    void asksOneHeldQuestionAtATimeAcrossEngineProfiles();
    void routesCancelAndRetryByTheDownloadNamespace();
    void forgetsOneDownloadWithoutForgettingTheRest();
    void persistsOnlyNonPrivateDownloadHistory();
    void asksBeforeWritingDownAProgram();
    void takesAPermissionForAutomaticAndMultipleDownloads();
    void sendsAConflictingNameToTheSaveDialog();
};

void DownloadsTest::holdsRunningAndRecordedDownloadsInOneList()
{
    QTemporaryDir root;
    StubHost host;
    SqliteSessionStore store(root.path());
    QVERIFY(store.open());
    QVERIFY(store.recordDownload(QStringLiteral("kept"),
        QUrl(QStringLiteral("https://files.example/old.zip")), QStringLiteral("/d/old.zip"),
        QStringLiteral("completed"), 10, 10));

    Downloads downloads(&store, &host);
    QCOMPARE(downloads.rowCount(), 1);
    QCOMPARE(roleOf(downloads, 0, Downloads::RunningRole).toBool(), false);

    downloads.started(QStringLiteral("work:1"),
        QUrl(QStringLiteral("https://files.example/new.zip")),
        QUrl(QStringLiteral("https://files.example/page")), QStringLiteral("/d/new.zip"),
        QStringLiteral("in-progress"), 0, 100);

    // One list: the running download and the Download record, told apart by a
    // role rather than by which collection they came out of.
    QCOMPARE(downloads.rowCount(), 2);
    QCOMPARE(roleOf(downloads, 0, Downloads::RunningRole).toBool(), true);
    QCOMPARE(roleOf(downloads, 0, Downloads::FileNameRole).toString(), QStringLiteral("new.zip"));
    QCOMPARE(roleOf(downloads, 1, Downloads::RecordIdRole).toString(), QStringLiteral("kept"));

    downloads.updated(QStringLiteral("work:1"), QStringLiteral("completed"), 100, 100, {});
    QCOMPARE(downloads.rowCount(), 2);
    QCOMPARE(roleOf(downloads, 0, Downloads::RunningRole).toBool(), false);
    QCOMPARE(store.downloadHistory().size(), 2);
}

void DownloadsTest::tellsTwoPrivateDownloadsApartWithoutARecord()
{
    StubHost host;
    PrivateSessionStore store(QSharedPointer<QHash<QString, int>>::create());
    QVERIFY(store.open());
    Downloads downloads(&store, &host);

    downloads.started(QStringLiteral(":1"), QUrl(QStringLiteral("https://files.example/a.zip")),
        QUrl(QStringLiteral("https://files.example/page")), QStringLiteral("/d/a.zip"),
        QStringLiteral("in-progress"), 0, 100);
    downloads.started(QStringLiteral(":2"), QUrl(QStringLiteral("https://files.example/b.zip")),
        QUrl(QStringLiteral("https://files.example/page")), QStringLiteral("/d/b.zip"),
        QStringLiteral("in-progress"), 0, 100);

    QCOMPARE(downloads.rowCount(), 2);
    QVERIFY(roleOf(downloads, 0, Downloads::RecordIdRole).toString().isEmpty());
    QVERIFY(roleOf(downloads, 1, Downloads::RecordIdRole).toString().isEmpty());
    QVERIFY(roleOf(downloads, 0, Downloads::RowIdRole).toString()
        != roleOf(downloads, 1, Downloads::RowIdRole).toString());
    QCOMPARE(downloads.running(), 2);
    QCOMPARE(downloads.activeCount(QUrl(QStringLiteral("https://files.example/page"))), 2);

    // Nothing recorded it, so nothing is left of it once it stops running.
    downloads.updated(QStringLiteral(":1"), QStringLiteral("completed"), 100, 100, {});
    QCOMPARE(downloads.rowCount(), 1);
    QCOMPARE(roleOf(downloads, 0, Downloads::RuntimeIdRole).toString(), QStringLiteral(":2"));
}

void DownloadsTest::publishesTheDownloadActivityWithoutBeingRefreshed()
{
    QTemporaryDir root;
    StubHost host;
    SqliteSessionStore store(root.path());
    QVERIFY(store.open());
    Downloads downloads(&store, &host);
    QSignalSpy activity(&downloads, &Downloads::activityChanged);

    QCOMPARE(downloads.running(), 0);
    QCOMPARE(downloads.fraction(), -1.0);

    downloads.started(QStringLiteral("work:1"), QUrl(QStringLiteral("https://a.example/one.zip")),
        QUrl(QStringLiteral("https://a.example/page")), QStringLiteral("/d/one.zip"),
        QStringLiteral("in-progress"), 0, 1000);
    downloads.updated(QStringLiteral("work:1"), QStringLiteral("in-progress"), 250, 1000, {});
    QCOMPARE(downloads.running(), 1);
    QCOMPARE(downloads.fraction(), 0.25);
    QVERIFY(activity.count() >= 2);

    // One download of unknown size makes the whole of it unknown.
    downloads.started(QStringLiteral("work:2"), QUrl(QStringLiteral("https://a.example/two.zip")),
        QUrl(QStringLiteral("https://a.example/page")), QStringLiteral("/d/two.zip"),
        QStringLiteral("in-progress"), 0, 0);
    QCOMPARE(downloads.running(), 2);
    QCOMPARE(downloads.fraction(), -1.0);
    QCOMPARE(downloads.runningDownloads().size(), 2);

    downloads.updated(QStringLiteral("work:1"), QStringLiteral("completed"), 1000, 1000, {});
    downloads.updated(QStringLiteral("work:2"), QStringLiteral("completed"), 10, 10, {});
    QCOMPARE(downloads.running(), 0);
    QCOMPARE(downloads.finished(), 2);

    // The mark reports what finished since the window was last idle, so the
    // next download starting into an idle window starts that count again.
    downloads.started(QStringLiteral("work:3"), QUrl(QStringLiteral("https://a.example/three.zip")),
        QUrl(QStringLiteral("https://a.example/page")), QStringLiteral("/d/three.zip"),
        QStringLiteral("in-progress"), 0, 100);
    QCOMPARE(downloads.finished(), 0);
}

void DownloadsTest::asksOneHeldQuestionAtATimeAcrossEngineProfiles()
{
    QTemporaryDir root;
    StubHost host;
    SqliteSessionStore store(root.path());
    QVERIFY(store.open());
    Downloads downloads(&store, &host);
    QSignalSpy questions(&downloads, &Downloads::questionChanged);
    QSignalSpy released(&downloads, &Downloads::releaseRequested);
    QSignalSpy discarded(&downloads, &Downloads::discardRequested);

    QVERIFY(downloads.question().isEmpty());
    downloads.hold(QStringLiteral("work"), QStringLiteral("token-1"),
        BrowserController::ConfirmDownload, QStringLiteral("https://a.example"),
        QUrl(QStringLiteral("https://a.example/install.sh")), QStringLiteral("install.sh"),
        QStringLiteral("script"));
    downloads.hold(QStringLiteral("personal"), QStringLiteral("token-2"),
        BrowserController::ConfirmDownload, QStringLiteral("https://b.example"),
        QUrl(QStringLiteral("https://b.example/setup.sh")), QStringLiteral("setup.sh"),
        QStringLiteral("script"));

    // One at a time, however many engine profiles the queue spans: the second
    // question waits behind the first.
    QCOMPARE(questions.count(), 1);
    QCOMPARE(
        downloads.question().value(QStringLiteral("token")).toString(), QStringLiteral("token-1"));

    downloads.answer(true, QStringLiteral("/d/install.sh"), BrowserController::Ask);
    QCOMPARE(released.count(), 1);
    QCOMPARE(released.first().at(0).toString(), QStringLiteral("work"));
    QCOMPARE(released.first().at(2).toString(), QStringLiteral("/d/install.sh"));
    QCOMPARE(
        downloads.question().value(QStringLiteral("token")).toString(), QStringLiteral("token-2"));

    downloads.answer(false, {}, BrowserController::Block);
    QCOMPARE(discarded.count(), 1);
    QCOMPARE(discarded.first().at(0).toString(), QStringLiteral("personal"));
    QCOMPARE(discarded.first().at(2).toString(), QStringLiteral("setup.sh"));
    QVERIFY(downloads.question().isEmpty());
    QCOMPARE(host.remembered, 1);
    QCOMPARE(host.automaticDownloadDecision(QStringLiteral("https://b.example")),
        static_cast<int>(BrowserController::Block));

    // Nothing is on screen, so answering again answers nothing.
    downloads.answer(true, {}, BrowserController::Ask);
    QCOMPARE(released.count(), 1);
}

void DownloadsTest::routesCancelAndRetryByTheDownloadNamespace()
{
    QTemporaryDir root;
    StubHost host;
    SqliteSessionStore store(root.path());
    QVERIFY(store.open());
    Downloads downloads(&store, &host);
    QSignalSpy cancels(&downloads, &Downloads::cancelRequested);
    QSignalSpy retries(&downloads, &Downloads::retryRequested);

    downloads.started(QStringLiteral("personal:7"),
        QUrl(QStringLiteral("https://a.example/one.zip")),
        QUrl(QStringLiteral("https://a.example/page")), QStringLiteral("/d/one.zip"),
        QStringLiteral("in-progress"), 0, 100);
    downloads.cancel(0);
    QCOMPARE(cancels.count(), 1);
    QCOMPARE(cancels.first().at(0).toString(), QStringLiteral("personal"));
    QCOMPARE(cancels.first().at(1).toString(), QStringLiteral("personal:7"));

    downloads.updated(QStringLiteral("personal:7"), QStringLiteral("interrupted"), 30, 100,
        QStringLiteral("network"));
    downloads.retry(0);
    QCOMPARE(retries.count(), 1);
    QCOMPARE(retries.first().at(0).toString(), QStringLiteral("personal"));
    QCOMPARE(retries.first().at(2).toUrl(), QUrl(QStringLiteral("https://a.example/one.zip")));

    // A Download record with no engine behind it names no namespace, which is
    // how the window knows to ask the page for the address again instead.
    Downloads restored(&store, &host);
    QSignalSpy restoredRetries(&restored, &Downloads::retryRequested);
    restored.retry(0);
    QCOMPARE(restoredRetries.count(), 1);
    QVERIFY(restoredRetries.first().at(0).toString().isEmpty());
    QVERIFY(restoredRetries.first().at(1).toString().isEmpty());
}

void DownloadsTest::forgetsOneDownloadWithoutForgettingTheRest()
{
    QTemporaryDir root;
    StubHost host;
    SqliteSessionStore store(root.path());
    QVERIFY(store.open());
    QVERIFY(store.recordDownload(QStringLiteral("second"),
        QUrl(QStringLiteral("https://files.example/second.zip")),
        QStringLiteral("/Downloads/second.zip"), QStringLiteral("completed"), 20, 20));
    QVERIFY(store.recordDownload(QStringLiteral("first"),
        QUrl(QStringLiteral("https://files.example/first.zip")),
        QStringLiteral("/Downloads/first.zip"), QStringLiteral("completed"), 10, 10));

    Downloads downloads(&store, &host);
    QCOMPARE(downloads.rowCount(), 2);
    const auto forgotten = roleOf(downloads, 0, Downloads::RecordIdRole).toString();
    const auto kept = roleOf(downloads, 1, Downloads::RecordIdRole).toString();
    QVERIFY(forgotten != kept);
    QVERIFY(downloads.forget(0));
    QCOMPARE(downloads.rowCount(), 1);
    QCOMPARE(roleOf(downloads, 0, Downloads::RecordIdRole).toString(), kept);
    QVERIFY(!downloads.forget(1));
    QVERIFY(!downloads.forget(-1));
    QCOMPARE(store.downloadHistory().size(), 1);

    // A Private window's running download has no Download record, so there is
    // nothing to forget and the row stays where it is.
    PrivateSessionStore refusing(QSharedPointer<QHash<QString, int>>::create());
    QVERIFY(refusing.open());
    Downloads privateDownloads(&refusing, &host);
    privateDownloads.started(QStringLiteral(":1"),
        QUrl(QStringLiteral("https://files.example/a.zip")),
        QUrl(QStringLiteral("https://files.example/page")), QStringLiteral("/d/a.zip"),
        QStringLiteral("in-progress"), 0, 100);
    QVERIFY(!privateDownloads.forget(0));
    QCOMPARE(privateDownloads.rowCount(), 1);
}

void DownloadsTest::persistsOnlyNonPrivateDownloadHistory()
{
    QTemporaryDir root;
    {
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
        auto *downloads = controller.downloads();
        downloads->started(QStringLiteral("test:1"),
            QUrl(QStringLiteral("https://files.example/archive.zip")),
            QUrl(QStringLiteral("https://files.example/page")),
            QStringLiteral("/Downloads/archive.zip"), QStringLiteral("in-progress"), 12, 100);
        QVERIFY(!roleOf(*downloads, 0, Downloads::RecordIdRole).toString().isEmpty());
        controller.closeActiveTab();
        downloads->updated(QStringLiteral("test:1"), QStringLiteral("completed"), 100, 100, {});

        PrivateSessionFixture privateSession;
        auto privateController = privateSession.createController();
        auto *privateDownloads = privateController->downloads();
        privateDownloads->started(QStringLiteral(":1"),
            QUrl(QStringLiteral("https://files.example/private.zip")),
            QUrl(QStringLiteral("https://files.example/page")),
            QStringLiteral("/Downloads/private.zip"), QStringLiteral("in-progress"), 5, 5);
        QVERIFY(roleOf(*privateDownloads, 0, Downloads::RecordIdRole).toString().isEmpty());
    }

    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    auto *downloads = restored.downloads();
    QCOMPARE(downloads->rowCount(), 1);
    QVERIFY(!roleOf(*downloads, 0, Downloads::RecordIdRole).toString().isEmpty());
    QCOMPARE(roleOf(*downloads, 0, Downloads::StateRole).toString(), QStringLiteral("completed"));
    QCOMPARE(roleOf(*downloads, 0, Downloads::ReceivedBytesRole).toLongLong(), 100);
    QCOMPARE(roleOf(*downloads, 0, Downloads::RunningRole).toBool(), false);
}

// Deciding a disposition asks the window what an origin has been allowed, so
// these three carry a window. The list itself needs none: every test above
// stands up a stub host instead.
void DownloadsTest::asksBeforeWritingDownAProgram()
{
    QTemporaryDir root;
    QTemporaryDir directory;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const QUrl origin(QStringLiteral("https://files.example/page"));
    controller.recordOriginInteraction(origin);

    const auto document = controller.downloads()->disposition(
        origin, QStringLiteral("notes.pdf"), QStringLiteral("application/pdf"), directory.path());
    QCOMPARE(
        document.value(QStringLiteral("disposition")).toInt(), BrowserController::AcceptDownload);
    QVERIFY(document.value(QStringLiteral("risk")).toString().isEmpty());

    const auto script = controller.downloads()->disposition(
        origin, QStringLiteral("install.sh"), QStringLiteral("text/plain"), directory.path());
    QCOMPARE(
        script.value(QStringLiteral("disposition")).toInt(), BrowserController::ConfirmDownload);
    QCOMPARE(script.value(QStringLiteral("risk")).toString(), QStringLiteral("script"));
    QCOMPARE(script.value(QStringLiteral("fileName")).toString(), QStringLiteral("install.sh"));
    QCOMPARE(
        script.value(QStringLiteral("origin")).toString(), QStringLiteral("https://files.example"));
    QVERIFY(!script.value(QStringLiteral("automatic")).toBool());
}

void DownloadsTest::takesAPermissionForAutomaticAndMultipleDownloads()
{
    QTemporaryDir root;
    QTemporaryDir directory;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personalSpaceId = controller.activeSpaceId();
    const QUrl origin(QStringLiteral("https://files.example/page"));
    const auto disposition = [&] {
        return controller.downloads()->disposition(origin, QStringLiteral("notes.pdf"),
            QStringLiteral("application/pdf"), directory.path());
    };

    QCOMPARE(disposition().value(QStringLiteral("disposition")).toInt(),
        BrowserController::AskDownloadPermission);
    QVERIFY(disposition().value(QStringLiteral("automatic")).toBool());

    QCOMPARE(controller.permissionPolicy(QStringLiteral("automatic-downloads")),
        static_cast<int>(BrowserController::Rememberable));
    QVERIFY(controller.setPermissionDecision(
        origin, QStringLiteral("automatic-downloads"), BrowserController::Block));
    QCOMPARE(disposition().value(QStringLiteral("disposition")).toInt(),
        BrowserController::RefuseDownload);

    QVERIFY(controller.setPermissionDecision(
        origin, QStringLiteral("automatic-downloads"), BrowserController::AllowPersistently));
    QCOMPARE(disposition().value(QStringLiteral("disposition")).toInt(),
        BrowserController::AcceptDownload);

    const auto workSpaceId = controller.createSpace(QStringLiteral("Work"));
    QVERIFY(controller.switchSpace(workSpaceId));
    QCOMPARE(disposition().value(QStringLiteral("disposition")).toInt(),
        BrowserController::AskDownloadPermission);
    QVERIFY(controller.switchSpace(personalSpaceId));

    // A second download from an origin the reader did click is automatic while
    // the first is still running, and asked for again once that one settles.
    const QUrl clicked(QStringLiteral("https://other.example/page"));
    controller.recordOriginInteraction(clicked);
    const auto clickedDisposition = [&] {
        return controller.downloads()
            ->disposition(clicked, QStringLiteral("notes.pdf"), QStringLiteral("application/pdf"),
                directory.path())
            .value(QStringLiteral("disposition"))
            .toInt();
    };
    QCOMPARE(clickedDisposition(), BrowserController::AcceptDownload);
    controller.downloads()->started(QStringLiteral("test:1"),
        QUrl(QStringLiteral("https://other.example/notes.pdf")), clicked,
        QStringLiteral("/d/notes.pdf"), QStringLiteral("in-progress"), 0, 100);
    QCOMPARE(clickedDisposition(), BrowserController::AskDownloadPermission);
    QCOMPARE(
        controller.downloads()
            ->disposition(QUrl(QStringLiteral("https://third.example/page")),
                QStringLiteral("notes.pdf"), QStringLiteral("application/pdf"), directory.path())
            .value(QStringLiteral("automatic"))
            .toBool(),
        true);
    controller.downloads()->updated(
        QStringLiteral("test:1"), QStringLiteral("completed"), 100, 100, {});
    QCOMPARE(clickedDisposition(), BrowserController::AcceptDownload);
}

void DownloadsTest::sendsAConflictingNameToTheSaveDialog()
{
    QTemporaryDir root;
    QTemporaryDir directory;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    const QUrl origin(QStringLiteral("https://files.example/page"));
    controller.recordOriginInteraction(origin);
    auto *downloads = controller.downloads();

    QFile existing(QDir(directory.path()).filePath(QStringLiteral("notes.pdf")));
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.close();

    QCOMPARE(downloads
                 ->disposition(origin, QStringLiteral("notes.pdf"),
                     QStringLiteral("application/pdf"), directory.path())
                 .value(QStringLiteral("disposition"))
                 .toInt(),
        BrowserController::SaveDownloadAs);
    QCOMPARE(downloads
                 ->disposition(origin, QStringLiteral("other.pdf"),
                     QStringLiteral("application/pdf"), directory.path())
                 .value(QStringLiteral("disposition"))
                 .toInt(),
        BrowserController::AcceptDownload);

    QFile program(QDir(directory.path()).filePath(QStringLiteral("install.sh")));
    QVERIFY(program.open(QIODevice::WriteOnly));
    program.close();
    QCOMPARE(downloads
                 ->disposition(origin, QStringLiteral("install.sh"), QStringLiteral("text/plain"),
                     directory.path())
                 .value(QStringLiteral("disposition"))
                 .toInt(),
        BrowserController::ConfirmDownload);
    QCOMPARE(downloads
                 ->disposition(origin, QStringLiteral("install.sh"), QStringLiteral("text/plain"),
                     directory.path(), true)
                 .value(QStringLiteral("disposition"))
                 .toInt(),
        BrowserController::SaveDownloadAs);
    QCOMPARE(downloads
                 ->disposition(origin, QStringLiteral("free.sh"), QStringLiteral("text/plain"),
                     directory.path(), true)
                 .value(QStringLiteral("disposition"))
                 .toInt(),
        BrowserController::AcceptDownload);
    QCOMPARE(downloads
                 ->disposition(QUrl(QStringLiteral("https://untouched.example/x")),
                     QStringLiteral("free.pdf"), QStringLiteral("application/pdf"),
                     directory.path(), true)
                 .value(QStringLiteral("disposition"))
                 .toInt(),
        BrowserController::AcceptDownload);
}

QTEST_MAIN(DownloadsTest)

#include "tst_downloads.moc"
