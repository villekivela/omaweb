#include "AgentControl.h"
#include "BrowserController.h"
#include "BrowserStateExchange.h"
#include "ControlSocket.h"
#include "SessionFixture.h"
#include "SqliteSessionStore.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using omaweb::AgentControl;
using omaweb::BrowserController;
using omaweb::BrowserStateExchangeAdapter;
using omaweb::ControlSocket;
using omaweb::test::SessionFixture;
using omaweb::test::SessionSpec;
using omaweb::test::SpaceSpec;
using omaweb::test::TabSpec;

namespace {

SessionSpec readersSession()
{
    return SessionSpec {
        .spaces = {SpaceSpec {
            .id = QStringLiteral("personal"),
            .name = QStringLiteral("Personal"),
            .tabs = {TabSpec {
                .id = QStringLiteral("personal-tab"),
                .url = QUrl(QStringLiteral("https://personal.example/")),
            }},
        }},
        .activeSpaceId = QStringLiteral("personal"),
    };
}

QString spaceDirectory(const SessionFixture &fixture, const QString &spaceId)
{
    return QDir(fixture.dataRoot()).filePath(QStringLiteral("spaces/%1").arg(spaceId));
}

// What a page would leave in the Space: a tab in its store and a cookie in
// its Engine profile.
void browseIn(BrowserController &browser, const QString &spaceId)
{
    QVERIFY(!browser.openTabInSpace(spaceId, QUrl(QStringLiteral("https://signup.example/")))
            .isEmpty());
    const auto profile = browser.prepareProfileForSpace(spaceId);
    QVERIFY(!profile.isEmpty());
    QFile cookies(QDir(profile).filePath(QStringLiteral("Cookies")));
    QVERIFY(cookies.open(QIODevice::WriteOnly));
    cookies.write("session=1");
}

// A client of the socket, answered on this thread.
class Client final {
public:
    explicit Client(const QString &path) { m_socket.connectToServer(path); }

    bool connected() { return m_socket.waitForConnected(1000); }

    QJsonObject ask(const QByteArray &line)
    {
        m_socket.write(line + '\n');
        m_socket.flush();
        QDeadlineTimer deadline(2000);
        while (!m_socket.canReadLine() && !deadline.hasExpired()) {
            QTest::qWait(5);
        }
        return QJsonDocument::fromJson(m_socket.readLine()).object();
    }

    void close() { m_socket.disconnectFromServer(); }

private:
    QLocalSocket m_socket;
};

QString createdSpace(const QJsonObject &answer)
{
    return answer.value(QStringLiteral("space")).toObject().value(QStringLiteral("id")).toString();
}

} // namespace

class TemporaryAgentSpacesTest final : public QObject {
    Q_OBJECT

private slots:
    void deletesTheSpaceWhenItsConnectionCloses();
    void keepsASpaceTheReaderTookOver();
    void deletesWhatACrashLeftBehind();
    void deletesACrashedSpaceThatWasOnShow();
    void leavesARunningBrowsersSpacesAlone();
    void sweepsWhatTheEngineWritesAfterward();
    void refusesAConnectionWhoseSpaceIsGone();
    void forgetsWhatWasDownloadedInTheSpace();
    void addsTheColumnsAnOlderStoreLacks();
    void deletesEveryTemporarySpaceAsTheBrowserExits();
    void leavesTheReaderASpaceWhenTheLastOneGoes();
    void refusesATemporarySpaceWithoutAConnection();
    void keepsTemporarySpacesOutOfSync();
};

void TemporaryAgentSpacesTest::deletesTheSpaceWhenItsConnectionCloses()
{
    QTemporaryDir config;
    QTemporaryDir runtime(QDir::tempPath() + QStringLiteral("/omaweb-XXXXXX"));
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    ControlSocket socket(&control);
    const auto path = runtime.filePath(QStringLiteral("c.sock"));
    QVERIFY(socket.listen(path));

    Client creator(path);
    QVERIFY(creator.connected());
    const auto created
        = creator.ask(R"({"verb":"space new","name":"agent","space":"Signup","temporary":true})");
    const auto spaceId = createdSpace(created);
    QVERIFY2(!spaceId.isEmpty(), QJsonDocument(created).toJson().constData());
    QVERIFY(created.value(QStringLiteral("space"))
            .toObject()
            .value(QStringLiteral("temporary"))
            .toBool());
    QVERIFY(browser->temporarySpace(spaceId));
    browseIn(*browser, spaceId);
    QVERIFY(QDir(spaceDirectory(fixture, spaceId)).exists());

    // Another connection closing, even one of the same name, takes nothing.
    {
        Client other(path);
        QVERIFY(other.connected());
        QVERIFY(
            other.ask(R"({"verb":"spaces","name":"agent"})").value(QStringLiteral("ok")).toBool());
        other.close();
    }
    QTest::qWait(50);
    QVERIFY(browser->agentSpace(spaceId));

    creator.close();
    QTRY_VERIFY(!browser->agentSpace(spaceId));
    QCOMPARE(browser->spaces()->rowCount(), 1);
    QVERIFY(!QDir(spaceDirectory(fixture, spaceId)).exists());
    QVERIFY(browser->sessionStore()->temporaryAgentSpaceIds().isEmpty());
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("personal"));
}

void TemporaryAgentSpacesTest::keepsASpaceTheReaderTookOver()
{
    QTemporaryDir config;
    QTemporaryDir runtime(QDir::tempPath() + QStringLiteral("/omaweb-XXXXXX"));
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    QString spaceId;
    {
        const auto browser = fixture.createController();
        AgentControl control(browser.get(), config.path());
        control.setAllowAgents(true);
        ControlSocket socket(&control);
        const auto path = runtime.filePath(QStringLiteral("c.sock"));
        QVERIFY(socket.listen(path));

        Client creator(path);
        QVERIFY(creator.connected());
        spaceId
            = createdSpace(creator.ask(R"({"verb":"space new","name":"agent","temporary":true})"));
        QVERIFY(!spaceId.isEmpty());
        browseIn(*browser, spaceId);

        QVERIFY(browser->takeOverSpace(spaceId));
        QVERIFY(!browser->temporarySpace(spaceId));
        creator.close();
        QTest::qWait(50);
        QCOMPARE(browser->spaces()->rowCount(), 2);
        browser->deleteTemporarySpaces();
        QCOMPARE(browser->spaces()->rowCount(), 2);
    }
    const auto restarted = fixture.createController();
    QCOMPARE(restarted->spaces()->rowCount(), 2);
    QVERIFY(!restarted->agentSpace(spaceId));
    QVERIFY(!restarted->temporarySpace(spaceId));
    QVERIFY(QDir(spaceDirectory(fixture, spaceId)).exists());
}

// A browser that crashed never ran its exit, so the next one finds the
// Space still labelled temporary and deletes it before restoring anything.
void TemporaryAgentSpacesTest::deletesWhatACrashLeftBehind()
{
    QTemporaryDir config;
    QTemporaryDir runtime(QDir::tempPath() + QStringLiteral("/omaweb-XXXXXX"));
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    QString temporaryId;
    QString permanentId;
    {
        const auto browser = fixture.createController();
        temporaryId
            = browser->createAgentSpace(QStringLiteral("Signup"), QStringLiteral("agent"), true);
        permanentId = browser->createAgentSpace(QStringLiteral("Checks"), QStringLiteral("agent"));
        QVERIFY(!temporaryId.isEmpty());
        browseIn(*browser, temporaryId);
        browser->downloads()->started(temporaryId + QStringLiteral(":1"),
            QUrl(QStringLiteral("https://signup.example/receipt.pdf")), {},
            QStringLiteral("/d/receipt.pdf"), QStringLiteral("completed"), 1, 1);
    }
    QVERIFY(QDir(spaceDirectory(fixture, temporaryId)).exists());

    const auto restarted = fixture.createController();
    QVERIFY(restarted->ready());
    // Known as temporary before anything decides whether to delete it.
    QVERIFY(restarted->temporarySpace(temporaryId));
    AgentControl control(restarted.get(), config.path());
    ControlSocket socket(&control);
    QVERIFY(
        omaweb::openAgentSocket(socket, *restarted, runtime.filePath(QStringLiteral("c.sock"))));
    QCOMPARE(restarted->spaces()->rowCount(), 2);
    QVERIFY(!restarted->agentSpace(temporaryId));
    QVERIFY(restarted->agentSpace(permanentId));
    QVERIFY(restarted->spaceTabs(temporaryId).isEmpty());
    QVERIFY(!QDir(spaceDirectory(fixture, temporaryId)).exists());
    QVERIFY(restarted->sessionStore()->temporaryAgentSpaceIds().isEmpty());
    QVERIFY(restarted->sessionStore()->downloadHistory().isEmpty());
    QCOMPARE(restarted->downloads()->rowCount(), 0);
}

void TemporaryAgentSpacesTest::deletesACrashedSpaceThatWasOnShow()
{
    QTemporaryDir config;
    QTemporaryDir runtime(QDir::tempPath() + QStringLiteral("/omaweb-XXXXXX"));
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    QString temporaryId;
    {
        const auto browser = fixture.createController();
        temporaryId
            = browser->createAgentSpace(QStringLiteral("Signup"), QStringLiteral("agent"), true);
        QVERIFY(browser->switchSpace(temporaryId));
    }
    const auto restarted = fixture.createController();
    AgentControl control(restarted.get(), config.path());
    ControlSocket socket(&control);
    QVERIFY(
        omaweb::openAgentSocket(socket, *restarted, runtime.filePath(QStringLiteral("c.sock"))));
    QCOMPARE(restarted->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(restarted->activeTabId(), QStringLiteral("personal-tab"));
    QCOMPARE(restarted->spaces()->rowCount(), 1);
}

// A second process on the same data, which macOS starts for every launch and
// Linux starts when the handover fails, must not take the running browser's
// temporary Spaces for a crash's.
void TemporaryAgentSpacesTest::leavesARunningBrowsersSpacesAlone()
{
    QTemporaryDir config;
    QTemporaryDir runtime(QDir::tempPath() + QStringLiteral("/omaweb-XXXXXX"));
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto path = runtime.filePath(QStringLiteral("c.sock"));
    auto running = fixture.createController();
    auto runningControl = std::make_unique<AgentControl>(running.get(), config.path());
    auto runningSocket = std::make_unique<ControlSocket>(runningControl.get());
    QVERIFY(omaweb::openAgentSocket(*runningSocket, *running, path));
    const auto spaceId
        = running->createAgentSpace(QStringLiteral("Signup"), QStringLiteral("agent"), true);
    browseIn(*running, spaceId);

    {
        const auto second = fixture.createController();
        AgentControl control(second.get(), config.path());
        ControlSocket socket(&control);
        QVERIFY(!omaweb::openAgentSocket(socket, *second, path));
        QVERIFY(second->agentSpace(spaceId));
    }
    QVERIFY(running->agentSpace(spaceId));
    QVERIFY(QDir(spaceDirectory(fixture, spaceId)).exists());
    QCOMPARE(running->sessionStore()->temporaryAgentSpaceIds(), QStringList {spaceId});

    // The running browser crashes; the next one gets the socket and cleans up.
    runningSocket.reset();
    runningControl.reset();
    running.reset();
    const auto next = fixture.createController();
    AgentControl control(next.get(), config.path());
    ControlSocket socket(&control);
    QVERIFY(omaweb::openAgentSocket(socket, *next, path));
    QVERIFY(!next->agentSpace(spaceId));
    QVERIFY(!QDir(spaceDirectory(fixture, spaceId)).exists());
}

// The engine writes into a profile while it lets it go, after the directory
// has been removed. What grows back is swept at the next start.
void TemporaryAgentSpacesTest::sweepsWhatTheEngineWritesAfterward()
{
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    QString spaceId;
    {
        const auto browser = fixture.createController();
        spaceId = browser->createAgentSpace(QStringLiteral("Signup"), QStringLiteral("a"), true);
        browseIn(*browser, spaceId);
        const auto profile = browser->profilePathForSpace(spaceId);
        QVERIFY(browser->deleteTemporarySpace(spaceId));
        QVERIFY(!QDir(spaceDirectory(fixture, spaceId)).exists());
        QVERIFY(QDir().mkpath(profile));
        QFile late(QDir(profile).filePath(QStringLiteral("Cookies-journal")));
        QVERIFY(late.open(QIODevice::WriteOnly));
        late.write("written at shutdown");
    }
    QVERIFY(QDir(spaceDirectory(fixture, spaceId)).exists());
    const auto restarted = fixture.createController();
    QVERIFY(restarted->ready());
    QVERIFY(!QDir(spaceDirectory(fixture, spaceId)).exists());
}

// A connection whose Space went with its holder is refused, rather than sent
// to the Space on show, until it names another.
void TemporaryAgentSpacesTest::refusesAConnectionWhoseSpaceIsGone()
{
    QTemporaryDir config;
    QTemporaryDir runtime(QDir::tempPath() + QStringLiteral("/omaweb-XXXXXX"));
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    ControlSocket socket(&control);
    const auto path = runtime.filePath(QStringLiteral("c.sock"));
    QVERIFY(socket.listen(path));

    QString spaceId;
    {
        Client holder(path);
        QVERIFY(holder.connected());
        spaceId
            = createdSpace(holder.ask(R"({"verb":"space new","name":"checker","temporary":true})"));
        QVERIFY(!spaceId.isEmpty());
        holder.close();
    }
    QTRY_VERIFY(!browser->agentSpace(spaceId));

    const auto ask = [&control](QJsonObject fields) {
        fields.insert(QStringLiteral("name"), QStringLiteral("checker"));
        return control.answer(fields);
    };
    const auto refused = ask({{QStringLiteral("verb"), QStringLiteral("open")},
        {QStringLiteral("url"), QStringLiteral("https://signup.example/")}});
    QCOMPARE(refused.value(QStringLiteral("code")).toString(), QStringLiteral("not-found"));
    QVERIFY(refused.value(QStringLiteral("error")).toString().contains(u"--space"));
    QCOMPARE(ask({{QStringLiteral("verb"), QStringLiteral("tabs")}}).value(QStringLiteral("code")),
        QStringLiteral("not-found"));
    QCOMPARE(browser->tabs()->rowCount(), 1);

    QVERIFY(ask({{QStringLiteral("verb"), QStringLiteral("open")},
                    {QStringLiteral("url"), QStringLiteral("https://signup.example/")},
                    {QStringLiteral("space"), QStringLiteral("Personal")}})
            .value(QStringLiteral("ok"))
            .toBool());
    QVERIFY(ask({{QStringLiteral("verb"), QStringLiteral("tabs")}})
            .value(QStringLiteral("ok"))
            .toBool());
}

void TemporaryAgentSpacesTest::deletesEveryTemporarySpaceAsTheBrowserExits()
{
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    const auto first = browser->createAgentSpace(QStringLiteral("One"), QStringLiteral("a"), true);
    const auto second = browser->createAgentSpace(QStringLiteral("Two"), QStringLiteral("b"), true);
    const auto permanent = browser->createAgentSpace(QStringLiteral("Kept"), QStringLiteral("a"));
    browseIn(*browser, first);

    browser->deleteTemporarySpaces();
    QVERIFY(!browser->agentSpace(first));
    QVERIFY(!browser->agentSpace(second));
    QVERIFY(browser->agentSpace(permanent));
    QCOMPARE(browser->spaces()->rowCount(), 2);
    QVERIFY(!QDir(spaceDirectory(fixture, first)).exists());
}

// A window always has a Space, so the reader who deleted all of theirs and
// was left looking at a temporary one gets a Space of their own back.
void TemporaryAgentSpacesTest::leavesTheReaderASpaceWhenTheLastOneGoes()
{
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    const auto spaceId
        = browser->createAgentSpace(QStringLiteral("Signup"), QStringLiteral("a"), true);
    QVERIFY(browser->deleteSpace(QStringLiteral("personal"), QStringLiteral("Personal")));
    QCOMPARE(browser->activeSpaceId(), spaceId);

    QVERIFY(browser->deleteTemporarySpace(spaceId));
    QCOMPARE(browser->spaces()->rowCount(), 1);
    QVERIFY(browser->activeSpaceId() != spaceId);
    QVERIFY(!browser->agentSpace(browser->activeSpaceId()));
    QVERIFY(!browser->deleteTemporarySpace(browser->activeSpaceId()));
}

void TemporaryAgentSpacesTest::refusesATemporarySpaceWithoutAConnection()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    const auto answer = control.answer({{QStringLiteral("verb"), QStringLiteral("space new")},
        {QStringLiteral("name"), QStringLiteral("agent")}, {QStringLiteral("temporary"), true}});
    QCOMPARE(answer.value(QStringLiteral("code")).toString(), QStringLiteral("bad-request"));
    QCOMPARE(browser->spaces()->rowCount(), 1);
}

void TemporaryAgentSpacesTest::keepsTemporarySpacesOutOfSync()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    const auto temporary
        = browser->createAgentSpace(QStringLiteral("Signup"), QStringLiteral("a"), true);
    const auto permanent = browser->createAgentSpace(QStringLiteral("Checks"), QStringLiteral("a"));
    browseIn(*browser, temporary);

    const BrowserStateExchangeAdapter exchange(
        browser.get(), nullptr, nullptr, fixture.dataRoot(), config.path());
    const auto image = exchange.capture({});
    QStringList captured;
    for (const auto &space : image.spaces) {
        captured.append(space.id);
    }
    QVERIFY(captured.contains(permanent));
    QVERIFY(!captured.contains(temporary));
    QVERIFY(!image.tabsBySpace.contains(temporary));

    // Looking at a temporary Space is reported as looking at one Sync keeps.
    QVERIFY(browser->switchSpace(temporary));
    const auto onShow = exchange.capture({});
    QVERIFY(onShow.activeSpaceId != temporary);
    QVERIFY(captured.contains(onShow.activeSpaceId));
    QVERIFY(!onShow.activeTabId.isEmpty());
    QCOMPARE(onShow.activeTabId, onShow.activeTabIds.value(onShow.activeSpaceId));
}

void TemporaryAgentSpacesTest::forgetsWhatWasDownloadedInTheSpace()
{
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    const auto spaceId
        = browser->createAgentSpace(QStringLiteral("Signup"), QStringLiteral("a"), true);
    auto *downloads = browser->downloads();
    downloads->started(spaceId + QStringLiteral(":1"),
        QUrl(QStringLiteral("https://signup.example/receipt.pdf")), {},
        QStringLiteral("/d/receipt.pdf"), QStringLiteral("completed"), 1, 1);
    downloads->started(QStringLiteral("personal:1"),
        QUrl(QStringLiteral("https://reader.example/a.pdf")), {}, QStringLiteral("/d/a.pdf"),
        QStringLiteral("completed"), 1, 1);
    QCOMPARE(downloads->rowCount(), 2);

    QVERIFY(browser->deleteTemporarySpace(spaceId));
    QCOMPARE(downloads->rowCount(), 1);
    const auto history = browser->sessionStore()->downloadHistory();
    QCOMPARE(history.size(), 1);
    QCOMPARE(history.constFirst().toMap().value(QStringLiteral("spaceId")).toString(),
        QStringLiteral("personal"));
}

// A store made before temporary Spaces and download Spaces had their columns.
void TemporaryAgentSpacesTest::addsTheColumnsAnOlderStoreLacks()
{
    QTemporaryDir dataRoot;
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("old"));
        database.setDatabaseName(QDir(dataRoot.path()).filePath(QStringLiteral("state.sqlite")));
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE spaces (id TEXT PRIMARY KEY, name TEXT NOT NULL, color TEXT NOT NULL, "
            "active INTEGER NOT NULL DEFAULT 0, position INTEGER NOT NULL DEFAULT 0)")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE agent_spaces (space_id TEXT PRIMARY KEY REFERENCES spaces(id) "
            "ON DELETE CASCADE, creator TEXT NOT NULL DEFAULT '')")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE downloads (id TEXT PRIMARY KEY, url TEXT NOT NULL, path TEXT NOT NULL, "
            "state TEXT NOT NULL, received_bytes INTEGER NOT NULL DEFAULT 0, "
            "total_bytes INTEGER NOT NULL DEFAULT -1, error TEXT NOT NULL DEFAULT '', "
            "created_at INTEGER NOT NULL)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO downloads VALUES('old', 'https://a.example/', '/d/a', 'completed', 1, 1, "
            "'', 1)")));
        database.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("old"));

    omaweb::SqliteSessionStore store(dataRoot.path());
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));
    QVERIFY(
        store.saveSpace({QStringLiteral("s"), QStringLiteral("S"), QStringLiteral("#000"), true}));
    QVERIFY(store.saveAgentSpace(QStringLiteral("s"), QStringLiteral("agent"), true));
    QCOMPARE(store.temporaryAgentSpaceIds(), QStringList {QStringLiteral("s")});
    QVERIFY(store.recordDownload(QStringLiteral("new"), QStringLiteral("s"),
        QUrl(QStringLiteral("https://b.example/")), QStringLiteral("/d/b"),
        QStringLiteral("completed"), 1, 1));
    QVERIFY(store.forgetSpaceDownloads(QStringLiteral("s")));
    QCOMPARE(store.downloadHistory().size(), 1);
}

QTEST_GUILESS_MAIN(TemporaryAgentSpacesTest)
#include "tst_temporaryagentspaces.moc"
