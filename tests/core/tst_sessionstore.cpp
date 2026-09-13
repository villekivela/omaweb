#include "HistoryQuery.h"
#include "PerformanceProbe.h"
#include "PrivateSessionStore.h"
#include "SessionStore.h"
#include "SpaceListModel.h"
#include "SqliteSessionStore.h"
#include "TabListModel.h"
#include "ThreadedSessionStore.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

using omaweb::PrivateSessionStore;
using omaweb::SessionStore;
using omaweb::SpaceState;
using omaweb::SqliteSessionStore;
using omaweb::TabState;
using omaweb::ThreadedSessionStore;

namespace {

QString spaceId() { return QStringLiteral("space-1"); }

SpaceState makeSpace()
{
    SpaceState space;
    space.id = QStringLiteral("space-1");
    space.name = QStringLiteral("Personal");
    space.color = QStringLiteral("#7c6cff");
    space.active = true;
    return space;
}

TabState makeTab(const QString &id, const QString &url)
{
    TabState tab;
    tab.id = id;
    tab.spaceId = QStringLiteral("space-1");
    tab.url = url;
    tab.title = url;
    tab.active = true;
    return tab;
}

// Every entry the store put in the root it was given, if it put any there.
bool dataRootIsEmpty(const QString &root)
{
    QDirIterator entries(
        root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    return !entries.hasNext();
}

} // namespace

// One suite, two adapters. What differs between them is one fact — whether the
// adapter records — so the expectations are that fact, applied.
class SessionStoreTest : public QObject {
    Q_OBJECT

private slots:
    void adaptersOpen();
    void adaptersOpen_data();
    void adaptersRecordASpaceOnlyIfTheyRecord();
    void adaptersRecordASpaceOnlyIfTheyRecord_data();
    void adaptersRecordTabsOnlyIfTheyRecord();
    void adaptersRecordTabsOnlyIfTheyRecord_data();
    void adaptersRecordHistoryOnlyIfTheyRecord();
    void adaptersRecordHistoryOnlyIfTheyRecord_data();
    void adaptersRecordPreferencesOnlyIfTheyRecord();
    void adaptersRecordPreferencesOnlyIfTheyRecord_data();
    void adaptersRecordDownloadsOnlyIfTheyRecord();
    void adaptersRecordDownloadsOnlyIfTheyRecord_data();
    void adaptersAnswerASitePermissionTheyWereGiven();
    void adaptersAnswerASitePermissionTheyWereGiven_data();
    void aRecordingStoreKeepsItsSessionAcrossAReopen();
    void aRecordingStoreRetriesCreatingASpaceDirectory();
    void aPrivateStoreLeavesTheRootItWasGivenEmpty();
    void aPrivateStoreSharesItsDecisionsWithTheSession();
    void aThreadedStoreReadsBackTheWritesItWasGivenBefore();
    void aThreadedStoreLandsAPendingWriteWhenItCloses();
    void aThreadedStoreTakesTheSessionsRunningWritesInsideTheirBudget();

private:
    static std::unique_ptr<SessionStore> makeStore(const QString &kind, const QString &root);
};

std::unique_ptr<SessionStore> SessionStoreTest::makeStore(const QString &kind, const QString &root)
{
    if (kind == QLatin1String("sqlite")) {
        return std::make_unique<SqliteSessionStore>(root);
    }
    return std::make_unique<PrivateSessionStore>(QSharedPointer<QHash<QString, int>>::create());
}

static void adapterRows()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<bool>("records");
    QTest::newRow("SQLite") << QStringLiteral("sqlite") << true;
    QTest::newRow("private") << QStringLiteral("private") << false;
}

void SessionStoreTest::adaptersOpen_data() { adapterRows(); }

void SessionStoreTest::adaptersOpen()
{
    QFETCH(QString, kind);
    QTemporaryDir root;
    auto store = makeStore(kind, root.path());
    QString error;
    QVERIFY2(store->open(&error), qPrintable(error));
}

void SessionStoreTest::adaptersRecordASpaceOnlyIfTheyRecord_data() { adapterRows(); }

void SessionStoreTest::adaptersRecordASpaceOnlyIfTheyRecord()
{
    QFETCH(QString, kind);
    QFETCH(bool, records);
    QTemporaryDir root;
    auto store = makeStore(kind, root.path());
    QVERIFY(store->open());

    QCOMPARE(store->saveSpace(makeSpace()), records);
    QCOMPARE(store->loadSpaces().size(), records ? 1 : 0);
    QCOMPARE(store->spaceHasSavedContent(spaceId()), false);
}

void SessionStoreTest::adaptersRecordTabsOnlyIfTheyRecord_data() { adapterRows(); }

void SessionStoreTest::adaptersRecordTabsOnlyIfTheyRecord()
{
    QFETCH(QString, kind);
    QFETCH(bool, records);
    QTemporaryDir root;
    auto store = makeStore(kind, root.path());
    QVERIFY(store->open());
    store->saveSpace(makeSpace());

    const QVector<TabState> tabs {
        makeTab(QStringLiteral("tab-1"), QStringLiteral("https://a.example"))};
    QCOMPARE(store->saveTabs(spaceId(), tabs, QStringLiteral("tab-1")), records);
    QCOMPARE(store->loadTabs(spaceId()).size(), records ? 1 : 0);

    QCOMPARE(store->recordClosedTabs(spaceId(), tabs), records);
    QCOMPARE(store->loadClosedTabs(spaceId()).size(), records ? 1 : 0);
}

void SessionStoreTest::adaptersRecordHistoryOnlyIfTheyRecord_data() { adapterRows(); }

void SessionStoreTest::adaptersRecordHistoryOnlyIfTheyRecord()
{
    QFETCH(QString, kind);
    QFETCH(bool, records);
    QTemporaryDir root;
    auto store = makeStore(kind, root.path());
    QVERIFY(store->open());
    store->saveSpace(makeSpace());

    QCOMPARE(store->recordVisit(
                 spaceId(), QUrl(QStringLiteral("https://a.example")), QStringLiteral("A")),
        records);
    QCOMPARE(store->history(spaceId(), {}, 10).size(), records ? 1 : 0);
}

void SessionStoreTest::adaptersRecordPreferencesOnlyIfTheyRecord_data() { adapterRows(); }

void SessionStoreTest::adaptersRecordPreferencesOnlyIfTheyRecord()
{
    QFETCH(QString, kind);
    QFETCH(bool, records);
    QTemporaryDir root;
    auto store = makeStore(kind, root.path());
    QVERIFY(store->open());

    QCOMPARE(
        store->savePreference(QStringLiteral("sidebar-width"), QStringLiteral("280")), records);
    QCOMPARE(store->preference(QStringLiteral("sidebar-width"), QStringLiteral("fallback")),
        records ? QStringLiteral("280") : QStringLiteral("fallback"));
}

void SessionStoreTest::adaptersRecordDownloadsOnlyIfTheyRecord_data() { adapterRows(); }

void SessionStoreTest::adaptersRecordDownloadsOnlyIfTheyRecord()
{
    QFETCH(QString, kind);
    QFETCH(bool, records);
    QTemporaryDir root;
    auto store = makeStore(kind, root.path());
    QVERIFY(store->open());

    QCOMPARE(store->recordDownload(QStringLiteral("download-1"),
                 QUrl(QStringLiteral("https://a.example/notes.pdf")),
                 QStringLiteral("/tmp/notes.pdf"), QStringLiteral("running"), 0, 100),
        records);
    QCOMPARE(store->downloadHistory().size(), records ? 1 : 0);
    QCOMPARE(store->forgetDownload(QStringLiteral("download-1")), records);
}

void SessionStoreTest::adaptersAnswerASitePermissionTheyWereGiven_data() { adapterRows(); }

// The one thing both adapters keep. A Private window's session holds the
// permissions its windows agreed to for as long as the session lasts, so this
// is the same assertion for both, unlike everything above.
void SessionStoreTest::adaptersAnswerASitePermissionTheyWereGiven()
{
    QFETCH(QString, kind);
    QTemporaryDir root;
    auto store = makeStore(kind, root.path());
    QVERIFY(store->open());
    store->saveSpace(makeSpace());

    QVERIFY(store->savePermissionDecision(
        spaceId(), QStringLiteral("https://a.example"), QStringLiteral("camera"), 2));
    QCOMPARE(store->permissionDecision(
                 spaceId(), QStringLiteral("https://a.example"), QStringLiteral("camera")),
        2);
    QCOMPARE(store->permissionDecision(
                 spaceId(), QStringLiteral("https://b.example"), QStringLiteral("camera")),
        0);
}

void SessionStoreTest::aRecordingStoreKeepsItsSessionAcrossAReopen()
{
    QTemporaryDir root;
    {
        SqliteSessionStore store(root.path());
        QVERIFY(store.open());
        QVERIFY(store.saveSpace(makeSpace()));
        QVERIFY(store.saveTabs(spaceId(),
            {makeTab(QStringLiteral("tab-1"), QStringLiteral("https://a.example"))},
            QStringLiteral("tab-1")));
        QVERIFY(store.savePreference(QStringLiteral("sidebar-width"), QStringLiteral("280")));
    }

    SqliteSessionStore reopened(root.path());
    QVERIFY(reopened.open());
    QCOMPARE(reopened.loadSpaces().size(), 1);
    QCOMPARE(reopened.loadTabs(spaceId()).size(), 1);
    QCOMPARE(reopened.preference(QStringLiteral("sidebar-width")), QStringLiteral("280"));
}

void SessionStoreTest::aRecordingStoreRetriesCreatingASpaceDirectory()
{
    QTemporaryDir root;
    SqliteSessionStore store(root.path());
    QVERIFY(store.open());

    const auto spacesPath = root.filePath(QStringLiteral("spaces"));
    QFile blocking(spacesPath);
    QVERIFY(blocking.open(QIODevice::WriteOnly));
    blocking.close();

    const auto spacePath = root.filePath(QStringLiteral("spaces/space-1"));
    QTest::ignoreMessage(QtWarningMsg,
        qPrintable(QStringLiteral("Could not create Space directory: %1").arg(spacePath)));
    QVERIFY(
        !store.saveTab(makeTab(QStringLiteral("tab-1"), QStringLiteral("https://a.example")), 0));

    QVERIFY(blocking.remove());
    QVERIFY(
        store.saveTab(makeTab(QStringLiteral("tab-1"), QStringLiteral("https://a.example")), 0));
    QCOMPARE(store.loadTabs(spaceId()).size(), 1);
}

// The claim the guards used to make one call site at a time.
void SessionStoreTest::aPrivateStoreLeavesTheRootItWasGivenEmpty()
{
    QTemporaryDir root;
    PrivateSessionStore store(QSharedPointer<QHash<QString, int>>::create());
    QVERIFY(store.open());

    store.saveSpace(makeSpace());
    store.saveTabs(spaceId(),
        {makeTab(QStringLiteral("tab-1"), QStringLiteral("https://a.example"))},
        QStringLiteral("tab-1"));
    store.recordClosedTabs(
        spaceId(), {makeTab(QStringLiteral("tab-2"), QStringLiteral("https://b.example"))});
    store.recordVisit(spaceId(), QUrl(QStringLiteral("https://a.example")), QStringLiteral("A"));
    store.savePreference(QStringLiteral("sidebar-width"), QStringLiteral("280"));
    store.recordDownload(QStringLiteral("download-1"),
        QUrl(QStringLiteral("https://a.example/notes.pdf")), QStringLiteral("/tmp/notes.pdf"),
        QStringLiteral("running"), 0, 100);
    store.savePermissionDecision(
        spaceId(), QStringLiteral("https://a.example"), QStringLiteral("camera"), 2);

    QVERIFY(dataRootIsEmpty(root.path()));
}

// One session, several windows: the hash is what they share, so a decision
// made through one store is the same decision through the next.
void SessionStoreTest::aPrivateStoreSharesItsDecisionsWithTheSession()
{
    auto session = QSharedPointer<QHash<QString, int>>::create();
    PrivateSessionStore first(session);
    PrivateSessionStore second(session);

    QVERIFY(first.savePermissionDecision(
        {}, QStringLiteral("https://a.example"), QStringLiteral("camera"), 2));
    QCOMPARE(second.permissionDecision(
                 {}, QStringLiteral("https://a.example"), QStringLiteral("camera")),
        2);

    session->clear();
    QCOMPARE(second.permissionDecision(
                 {}, QStringLiteral("https://a.example"), QStringLiteral("camera")),
        0);
}

namespace {

// A Space at the bounds the running writes are priced against: history at its
// retained bound, a working day's tabs, and the closed-tab stack full. The
// same Space the session-restore probe brings back.
constexpr int openTabs = 100;
constexpr int closedTabs = 25;

QVector<TabState> makeTabs(const QString &prefix, int count)
{
    QVector<TabState> tabs;
    for (int index = 0; index < count; ++index) {
        auto tab = makeTab(QStringLiteral("%1-%2").arg(prefix).arg(index),
            QStringLiteral("https://%1-%2.example/article").arg(prefix).arg(index));
        tab.active = false;
        tabs.append(tab);
    }
    return tabs;
}

void fillSpaceToItsBounds(SessionStore &store)
{
    store.saveSpace(makeSpace());
    for (int index = 0; index < omaweb::history::retainedRows; ++index) {
        store.recordVisit(spaceId(), QUrl(QStringLiteral("https://visited-%1.example/").arg(index)),
            QStringLiteral("Visit %1").arg(index));
    }
}

// The interface thread's share of each running write: the longest of a run of
// them, because it is one slow write between two frames that the reader sees,
// not the typical one. A run of visits is longer than a cleanup batch so that
// the visit which restores the history bound is among them.
struct RunningWriteCost {
    double visitMicroseconds = 0;
    double tabsMicroseconds = 0;
    double closedTabsMicroseconds = 0;
};

RunningWriteCost priceRunningWrites(SessionStore &store)
{
    const auto tabs = makeTabs(QStringLiteral("tab"), openTabs);
    const auto closed = makeTabs(QStringLiteral("closed"), closedTabs);
    QElapsedTimer timer;
    RunningWriteCost cost;
    for (int index = 0; index < omaweb::history::cleanupBatch + 1; ++index) {
        timer.start();
        store.recordVisit(spaceId(), QUrl(QStringLiteral("https://again-%1.example/").arg(index)),
            QStringLiteral("Again %1").arg(index));
        cost.visitMicroseconds = std::max(cost.visitMicroseconds, timer.nsecsElapsed() / 1e3);
    }
    for (int run = 0; run < 20; ++run) {
        timer.start();
        store.recordTabs(spaceId(), tabs, tabs.first().id);
        cost.tabsMicroseconds = std::max(cost.tabsMicroseconds, timer.nsecsElapsed() / 1e3);
        timer.start();
        store.recordClosedTabs(spaceId(), closed);
        cost.closedTabsMicroseconds
            = std::max(cost.closedTabsMicroseconds, timer.nsecsElapsed() / 1e3);
    }
    return cost;
}

} // namespace

// A read after a queued write answers with that write landed: the store keeps
// the order it was given, so nothing on the interface has to wait for a write
// to be sure of what it reads next.
void SessionStoreTest::aThreadedStoreReadsBackTheWritesItWasGivenBefore()
{
    QTemporaryDir root;
    ThreadedSessionStore store(std::make_unique<SqliteSessionStore>(root.path()));
    QVERIFY(store.open());
    QVERIFY(store.saveSpace(makeSpace()));
    for (int index = 0; index < 50; ++index) {
        QVERIFY(store.saveTabs(
            spaceId(), makeTabs(QStringLiteral("tab"), index + 1), QStringLiteral("tab-0")));
        QVERIFY(store.recordClosedTabs(spaceId(), makeTabs(QStringLiteral("closed"), index)));
        QVERIFY(store.recordVisit(spaceId(),
            QUrl(QStringLiteral("https://visited-%1.example/").arg(index)),
            QStringLiteral("Visit %1").arg(index)));
    }
    QCOMPARE(store.loadTabs(spaceId()).size(), 50);
    QCOMPARE(store.loadClosedTabs(spaceId()).size(), 49);
    const auto history = store.history(spaceId(), {}, 100);
    QCOMPARE(history.size(), 50);
    QCOMPARE(history.first().toMap().value(QStringLiteral("url")).toUrl(),
        QUrl(QStringLiteral("https://visited-49.example/")));
}

// A quit while a write is still queued lands it before the store is gone,
// which is what the interface's own flush at quit relies on.
void SessionStoreTest::aThreadedStoreLandsAPendingWriteWhenItCloses()
{
    QTemporaryDir root;
    {
        ThreadedSessionStore store(std::make_unique<SqliteSessionStore>(root.path()));
        QVERIFY(store.open());
        QVERIFY(store.saveSpace(makeSpace()));
        QVERIFY(
            store.saveTabs(spaceId(), makeTabs(QStringLiteral("tab"), 3), QStringLiteral("tab-0")));
        QVERIFY(store.recordClosedTabs(spaceId(), makeTabs(QStringLiteral("closed"), 2)));
        QVERIFY(store.recordVisit(
            spaceId(), QUrl(QStringLiteral("https://last.example/")), QStringLiteral("Last")));
    }
    SqliteSessionStore reopened(root.path());
    QVERIFY(reopened.open());
    QCOMPARE(reopened.loadTabs(spaceId()).size(), 3);
    QCOMPARE(reopened.loadClosedTabs(spaceId()).size(), 2);
    QCOMPARE(reopened.history(spaceId(), {}, 10).size(), 1);
}

// What the interface thread pays for a visit, the coalesced tab write and the
// closed-tab write against a Space at its bounds. The control is the same
// writes taken on the calling thread, which is what they cost before the store
// thread existed; the threshold is the one the writes were moved for.
void SessionStoreTest::aThreadedStoreTakesTheSessionsRunningWritesInsideTheirBudget()
{
    // The issue's own bound rather than a margin over the first measurement:
    // the writes were moved because one of them crossed it. Microseconds,
    // because a queued write is over in tens of them.
    constexpr double thresholdMicroseconds = 2000.0;
    QTemporaryDir controlRoot;
    SqliteSessionStore control(controlRoot.path());
    QVERIFY(control.open());
    fillSpaceToItsBounds(control);
    const auto before = priceRunningWrites(control);
    omaweb::probe::report(QStringLiteral("visit-record-on-the-calling-thread"),
        before.visitMicroseconds, QStringLiteral("us"), thresholdMicroseconds);
    omaweb::probe::report(QStringLiteral("tab-write-on-the-calling-thread"),
        before.tabsMicroseconds, QStringLiteral("us"), thresholdMicroseconds);
    omaweb::probe::report(QStringLiteral("closed-tab-write-on-the-calling-thread"),
        before.closedTabsMicroseconds, QStringLiteral("us"), thresholdMicroseconds);

    QTemporaryDir root;
    ThreadedSessionStore store(std::make_unique<SqliteSessionStore>(root.path()));
    QVERIFY(store.open());
    fillSpaceToItsBounds(store);
    const auto after = priceRunningWrites(store);
    QVERIFY2(after.visitMicroseconds <= thresholdMicroseconds,
        qPrintable(omaweb::probe::report(QStringLiteral("visit-record"), after.visitMicroseconds,
            QStringLiteral("us"), thresholdMicroseconds)));
    QVERIFY2(after.tabsMicroseconds <= thresholdMicroseconds,
        qPrintable(omaweb::probe::report(QStringLiteral("tab-write"), after.tabsMicroseconds,
            QStringLiteral("us"), thresholdMicroseconds)));
    QVERIFY2(after.closedTabsMicroseconds <= thresholdMicroseconds,
        qPrintable(omaweb::probe::report(QStringLiteral("closed-tab-write"),
            after.closedTabsMicroseconds, QStringLiteral("us"), thresholdMicroseconds)));
}

QTEST_MAIN(SessionStoreTest)
#include "tst_sessionstore.moc"
