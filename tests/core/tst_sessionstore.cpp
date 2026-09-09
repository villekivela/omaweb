#include "PrivateSessionStore.h"
#include "SessionStore.h"
#include "SpaceListModel.h"
#include "SqliteSessionStore.h"
#include "TabListModel.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using omaweb::PrivateSessionStore;
using omaweb::SessionStore;
using omaweb::SpaceState;
using omaweb::SqliteSessionStore;
using omaweb::TabState;

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

    QCOMPARE(store->saveClosedTabs(spaceId(), tabs), records);
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
    store.saveClosedTabs(
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

QTEST_MAIN(SessionStoreTest)
#include "tst_sessionstore.moc"
