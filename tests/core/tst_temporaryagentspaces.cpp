#include "AgentControl.h"
#include "BrowserController.h"
#include "BrowserStateExchange.h"
#include "ControlSocket.h"
#include "SessionFixture.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
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
    }
    QVERIFY(QDir(spaceDirectory(fixture, temporaryId)).exists());

    const auto restarted = fixture.createController();
    QVERIFY(restarted->ready());
    QCOMPARE(restarted->spaces()->rowCount(), 2);
    QVERIFY(!restarted->agentSpace(temporaryId));
    QVERIFY(restarted->agentSpace(permanentId));
    QVERIFY(restarted->spaceTabs(temporaryId).isEmpty());
    QVERIFY(!QDir(spaceDirectory(fixture, temporaryId)).exists());
    QVERIFY(restarted->sessionStore()->temporaryAgentSpaceIds().isEmpty());
}

void TemporaryAgentSpacesTest::deletesACrashedSpaceThatWasOnShow()
{
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
    QCOMPARE(restarted->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(restarted->activeTabId(), QStringLiteral("personal-tab"));
    QCOMPARE(restarted->spaces()->rowCount(), 1);
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
}

QTEST_GUILESS_MAIN(TemporaryAgentSpacesTest)
#include "tst_temporaryagentspaces.moc"
