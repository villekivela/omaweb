#include "AgentControl.h"
#include "BrowserController.h"
#include "ControlSocket.h"
#include "PrivateSessionFixture.h"
#include "SessionFixture.h"

#include <QDeadlineTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

using omaweb::AgentControl;
using omaweb::BrowserController;
using omaweb::ControlSocket;
using omaweb::test::PrivateSessionFixture;
using omaweb::test::SessionFixture;
using omaweb::test::SessionSpec;
using omaweb::test::SpaceSpec;
using omaweb::test::TabSpec;

namespace {

QJsonObject ask(
    AgentControl &control, const QString &name, const QString &verb, QJsonObject fields = {})
{
    fields.insert(QStringLiteral("name"), name);
    fields.insert(QStringLiteral("verb"), verb);
    return control.answer(fields);
}

QStringList ids(const QJsonArray &items)
{
    QStringList result;
    for (const auto &item : items) {
        result.append(item.toObject().value(QStringLiteral("id")).toString());
    }
    return result;
}

bool succeeded(const QJsonObject &answer) { return answer.value(QStringLiteral("ok")).toBool(); }

QString failure(const QJsonObject &answer)
{
    return answer.value(QStringLiteral("code")).toString();
}

// Two Spaces the reader made: one on show and one away, each with a page, and
// a Pinned tab on show.
SessionSpec readersSession()
{
    return SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = QStringLiteral("personal"),
                .name = QStringLiteral("Personal"),
                .tabs = {TabSpec {
                             .id = QStringLiteral("personal-pin"),
                             .url = QUrl(QStringLiteral("https://mail.example/")),
                             .pinned = true,
                         },
                    TabSpec {
                        .id = QStringLiteral("personal-tab"),
                        .url = QUrl(QStringLiteral("https://personal.example/")),
                    }},
                .activeTabId = QStringLiteral("personal-tab"),
            },
            SpaceSpec {
                .id = QStringLiteral("work"),
                .name = QStringLiteral("Work"),
                .tabs = {TabSpec {
                    .id = QStringLiteral("work-tab"),
                    .url = QUrl(QStringLiteral("https://work.example/")),
                }},
            },
        },
        .activeSpaceId = QStringLiteral("personal"),
    };
}

} // namespace

class AgentControlTest final : public QObject {
    Q_OBJECT

private slots:
    void gatesOnlyAgentSpacesBehindAllowAgents();
    void keepsAllowAgentsOffUntilTheReaderTurnsItOn();
    void runsBrowserCommandsWithAllowAgentsOff();
    void opensATabInTheSpaceOnShowWithoutSelectingIt();
    void keepsACurrentTabForEachConnection();
    void refusesAddressesThatActInsideAPage();
    void leavesPinnedTabsAndTheReadersTabsAlone();
    void closesAndLoadsTabsOfASpaceNotOnShow();
    void createsAnAgentSpaceOnlyWithAllowAgents();
    void keepsTheAgentSpaceLabelAcrossARestart();
    void deletesOnlyAgentSpaces();
    void takesAnAgentSpaceOver();
    void detachesEveryConnectionWhenAllowAgentsGoesOff();
    void neverListsOrReachesAPrivateWindow();
    void answersOverASocketOnlyItsUserCanOpen();
};

void AgentControlTest::gatesOnlyAgentSpacesBehindAllowAgents()
{
    for (const auto &verb : {QStringLiteral("spaces"), QStringLiteral("tabs"),
             QStringLiteral("open"), QStringLiteral("close")}) {
        QVERIFY2(!AgentControl::gated(verb), qPrintable(verb));
    }
    QVERIFY(AgentControl::gated(QStringLiteral("space new")));
    QVERIFY(AgentControl::gated(QStringLiteral("space delete")));
}

// Off is the default and the reader's choice lasts, kept with the rest of
// their configuration rather than in a Space.
void AgentControlTest::keepsAllowAgentsOffUntilTheReaderTurnsItOn()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    {
        AgentControl control(browser.get(), config.path());
        QVERIFY(!control.allowAgents());
        QSignalSpy changed(&control, &AgentControl::allowAgentsChanged);
        control.setAllowAgents(true);
        QCOMPARE(changed.count(), 1);
    }
    AgentControl restarted(browser.get(), config.path());
    QVERIFY(restarted.allowAgents());
}

void AgentControlTest::runsBrowserCommandsWithAllowAgentsOff()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());

    const auto spaces = ask(control, QStringLiteral("script"), QStringLiteral("spaces"));
    QVERIFY(succeeded(spaces));
    const auto listed = spaces.value(QStringLiteral("spaces")).toArray();
    QCOMPARE(ids(listed), QStringList({QStringLiteral("personal"), QStringLiteral("work")}));
    QVERIFY(listed.at(0).toObject().value(QStringLiteral("onShow")).toBool());
    QVERIFY(!listed.at(0).toObject().value(QStringLiteral("agent")).toBool());

    const auto tabs = ask(control, QStringLiteral("script"), QStringLiteral("tabs"),
        {{QStringLiteral("space"), QStringLiteral("Work")}});
    QVERIFY(succeeded(tabs));
    QCOMPARE(ids(tabs.value(QStringLiteral("tabs")).toArray()),
        QStringList {QStringLiteral("work-tab")});

    const auto opened = ask(control, QStringLiteral("script"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://example.com/")}});
    QVERIFY2(succeeded(opened), qPrintable(opened.value(QStringLiteral("error")).toString()));
    const auto tabId
        = opened.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
    QVERIFY(browser->findTab(tabId));

    QVERIFY(succeeded(ask(control, QStringLiteral("script"), QStringLiteral("close"))));
    QVERIFY(!browser->findTab(tabId));
}

void AgentControlTest::opensATabInTheSpaceOnShowWithoutSelectingIt()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    QSignalSpy activeTab(browser.get(), &BrowserController::activeTabChanged);

    const auto opened = ask(control, QStringLiteral("agent"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://example.com/")}});
    QVERIFY(succeeded(opened));
    const auto tab = opened.value(QStringLiteral("tab")).toObject();
    QCOMPARE(tab.value(QStringLiteral("space")).toString(), QStringLiteral("personal"));
    QCOMPARE(browser->activeTabId(), QStringLiteral("personal-tab"));
    QCOMPARE(activeTab.count(), 0);
    QCOMPARE(browser->tabs()->rowCount(), 3);
}

// The current tab is the connection's own. The reader's selection never moves
// it and another connection never sees it.
void AgentControlTest::keepsACurrentTabForEachConnection()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());

    const auto first = ask(control, QStringLiteral("first"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://first.example/")}});
    const auto second = ask(control, QStringLiteral("second"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://second.example/")}});
    const auto firstTab
        = first.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
    const auto secondTab
        = second.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
    QVERIFY(firstTab != secondTab);

    const auto currentOf = [&control](const QString &name) {
        const auto tabs = ask(control, name, QStringLiteral("tabs"));
        for (const auto &tab : tabs.value(QStringLiteral("tabs")).toArray()) {
            if (tab.toObject().value(QStringLiteral("current")).toBool()) {
                return tab.toObject().value(QStringLiteral("id")).toString();
            }
        }
        return QString {};
    };
    QCOMPARE(currentOf(QStringLiteral("first")), firstTab);
    QCOMPARE(currentOf(QStringLiteral("second")), secondTab);

    browser->activateTab(secondTab);
    QCOMPARE(currentOf(QStringLiteral("first")), firstTab);

    // Naming a tab moves that one and makes it current.
    const auto moved = ask(control, QStringLiteral("first"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://moved.example/")},
            {QStringLiteral("tab"), firstTab}});
    QVERIFY(succeeded(moved));
    QCOMPARE(browser->findTab(firstTab)->url, QUrl(QStringLiteral("https://moved.example/")));

    QVERIFY(succeeded(ask(control, QStringLiteral("first"), QStringLiteral("close"))));
    QVERIFY(!browser->findTab(firstTab));
    QVERIFY(browser->findTab(secondTab));
    QCOMPARE(currentOf(QStringLiteral("second")), secondTab);
    QCOMPARE(failure(ask(control, QStringLiteral("first"), QStringLiteral("close"))),
        QStringLiteral("no-current-tab"));
}

void AgentControlTest::refusesAddressesThatActInsideAPage()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());

    for (const auto &address :
        {QStringLiteral("javascript:alert(1)"), QStringLiteral("data:text/html,<p>hi</p>")}) {
        const auto answer = ask(control, QStringLiteral("agent"), QStringLiteral("open"),
            {{QStringLiteral("url"), address}});
        QCOMPARE(failure(answer), QStringLiteral("refused"));
    }
    QCOMPARE(browser->tabs()->rowCount(), 2);
}

// Browser commands reach every Space, but a pin is the reader's address and a
// tab no Agent opened is the reader's to close.
void AgentControlTest::leavesPinnedTabsAndTheReadersTabsAlone()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());

    QCOMPARE(failure(ask(control, QStringLiteral("agent"), QStringLiteral("open"),
                 {{QStringLiteral("url"), QStringLiteral("https://example.com/")},
                     {QStringLiteral("tab"), QStringLiteral("personal-pin")}})),
        QStringLiteral("refused"));
    QCOMPARE(browser->findTab(QStringLiteral("personal-pin"))->url,
        QUrl(QStringLiteral("https://mail.example/")));

    for (const auto &tabId : {QStringLiteral("personal-pin"), QStringLiteral("personal-tab"),
             QStringLiteral("work-tab")}) {
        QCOMPARE(failure(ask(control, QStringLiteral("agent"), QStringLiteral("close"),
                     {{QStringLiteral("tab"), tabId}})),
            QStringLiteral("refused"));
        QVERIFY(browser->findTab(tabId));
    }

    // Another connection's tab is still one an Agent opened.
    const auto opened = ask(control, QStringLiteral("first"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://example.com/")}});
    const auto tabId
        = opened.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
    QVERIFY(succeeded(ask(control, QStringLiteral("second"), QStringLiteral("close"),
        {{QStringLiteral("tab"), tabId}})));
}

void AgentControlTest::closesAndLoadsTabsOfASpaceNotOnShow()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    QSignalSpy discarded(browser.get(), &BrowserController::awayTabDiscarded);

    const auto opened = ask(control, QStringLiteral("agent"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://away.example/")},
            {QStringLiteral("space"), QStringLiteral("work")}});
    QVERIFY(succeeded(opened));
    const auto tabId
        = opened.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(ids(ask(control, QStringLiteral("agent"), QStringLiteral("tabs"))
                     .value(QStringLiteral("tabs"))
                     .toArray()),
        QStringList({QStringLiteral("work-tab"), tabId}));

    // A later open moves the current tab rather than opening another.
    QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://away.example/next")}})));
    QCOMPARE(browser->findTab(tabId)->url, QUrl(QStringLiteral("https://away.example/next")));
    QCOMPARE(discarded.count(), 1);
    const auto another = ask(control, QStringLiteral("agent"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://away.example/new")},
            {QStringLiteral("new"), true}});
    const auto anotherId
        = another.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
    QVERIFY(anotherId != tabId);
    QCOMPARE(browser->findTab(anotherId)->spaceId, QStringLiteral("work"));

    QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("close"),
        {{QStringLiteral("tab"), tabId}})));
    QVERIFY(!browser->findTab(tabId));
    QCOMPARE(discarded.count(), 2);
    QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("close"))));

    QVERIFY(browser->switchSpace(QStringLiteral("work")));
    QCOMPARE(browser->activeTabId(), QStringLiteral("work-tab"));
    QCOMPARE(browser->tabs()->rowCount(), 1);
}

void AgentControlTest::createsAnAgentSpaceOnlyWithAllowAgents()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());

    const auto refused = ask(control, QStringLiteral("agent"), QStringLiteral("space new"),
        {{QStringLiteral("space"), QStringLiteral("Checks")}});
    QCOMPARE(failure(refused), QStringLiteral("allow-agents"));
    QVERIFY(refused.value(QStringLiteral("error")).toString().contains(u"Allow agents"));
    QCOMPARE(browser->spaces()->rowCount(), 2);

    control.setAllowAgents(true);
    const auto created = ask(control, QStringLiteral("agent"), QStringLiteral("space new"),
        {{QStringLiteral("space"), QStringLiteral("Checks")}});
    QVERIFY(succeeded(created));
    const auto space = created.value(QStringLiteral("space")).toObject();
    const auto spaceId = space.value(QStringLiteral("id")).toString();
    QVERIFY(space.value(QStringLiteral("agent")).toBool());
    QVERIFY(browser->agentSpace(spaceId));
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("personal"));

    // The new Space is where this connection's next tab goes.
    const auto opened = ask(control, QStringLiteral("agent"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://example.com/")}});
    QCOMPARE(
        opened.value(QStringLiteral("tab")).toObject().value(QStringLiteral("space")).toString(),
        spaceId);

    // A name is optional.
    QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("space new"))));
}

void AgentControlTest::keepsTheAgentSpaceLabelAcrossARestart()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    QString spaceId;
    {
        const auto browser = fixture.createController();
        spaceId = browser->createAgentSpace(QStringLiteral("Checks"));
        QVERIFY(!spaceId.isEmpty());
    }
    const auto restarted = fixture.createController();
    QVERIFY(restarted->agentSpace(spaceId));
    QVERIFY(!restarted->agentSpace(QStringLiteral("personal")));
    // The label is not a field of the Space record, so nothing that copies a
    // record, Sync included, carries it.
    for (const auto &space : restarted->sessionStore()->loadSpaces()) {
        QVERIFY(space.id != spaceId || space.name == QStringLiteral("Checks"));
    }
    QCOMPARE(restarted->sessionStore()->agentSpaceIds(), QStringList {spaceId});
}

void AgentControlTest::deletesOnlyAgentSpaces()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);

    QCOMPARE(failure(ask(control, QStringLiteral("agent"), QStringLiteral("space delete"),
                 {{QStringLiteral("space"), QStringLiteral("Work")}})),
        QStringLiteral("refused"));
    QCOMPARE(browser->spaces()->rowCount(), 2);

    const auto created = ask(control, QStringLiteral("agent"), QStringLiteral("space new"),
        {{QStringLiteral("space"), QStringLiteral("Checks")}});
    const auto spaceId
        = created.value(QStringLiteral("space")).toObject().value(QStringLiteral("id")).toString();
    QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("space delete"),
        {{QStringLiteral("space"), spaceId}})));
    QCOMPARE(browser->spaces()->rowCount(), 2);
    QVERIFY(!browser->agentSpace(spaceId));
    QVERIFY(browser->sessionStore()->agentSpaceIds().isEmpty());
}

void AgentControlTest::takesAnAgentSpaceOver()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);

    const auto created = ask(control, QStringLiteral("agent"), QStringLiteral("space new"),
        {{QStringLiteral("space"), QStringLiteral("Checks")}});
    const auto spaceId
        = created.value(QStringLiteral("space")).toObject().value(QStringLiteral("id")).toString();
    const auto opened = ask(control, QStringLiteral("agent"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://example.com/")}});
    QVERIFY(succeeded(opened));

    QSignalSpy changed(browser.get(), &BrowserController::agentSpacesChanged);
    QVERIFY(browser->takeOverSpace(spaceId));
    QCOMPARE(changed.count(), 1);
    QVERIFY(!browser->agentSpace(spaceId));
    QVERIFY(!browser->takeOverSpace(spaceId));
    QVERIFY(!browser->takeOverSpace(QStringLiteral("personal")));

    // The Space and its tabs stay; it is the reader's now.
    QCOMPARE(browser->spaces()->rowCount(), 3);
    QCOMPARE(browser->spaceTabs(spaceId).size(), 1);
    QCOMPARE(failure(ask(control, QStringLiteral("agent"), QStringLiteral("space delete"),
                 {{QStringLiteral("space"), spaceId}})),
        QStringLiteral("refused"));
    const auto spaces = ask(control, QStringLiteral("agent"), QStringLiteral("spaces"));
    for (const auto &space : spaces.value(QStringLiteral("spaces")).toArray()) {
        QVERIFY(!space.toObject().value(QStringLiteral("agent")).toBool());
    }
}

void AgentControlTest::detachesEveryConnectionWhenAllowAgentsGoesOff()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);

    for (const auto &name : {QStringLiteral("first"), QStringLiteral("second")}) {
        QVERIFY(succeeded(ask(control, name, QStringLiteral("open"),
            {{QStringLiteral("url"), QStringLiteral("https://example.com/")}})));
    }
    control.setAllowAgents(false);
    for (const auto &name : {QStringLiteral("first"), QStringLiteral("second")}) {
        QCOMPARE(
            failure(ask(control, name, QStringLiteral("close"))), QStringLiteral("no-current-tab"));
    }
    // The socket stays up for browser commands.
    QVERIFY(succeeded(ask(control, QStringLiteral("first"), QStringLiteral("spaces"))));
}

void AgentControlTest::neverListsOrReachesAPrivateWindow()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    PrivateSessionFixture privateSession;
    const auto privateWindow = privateSession.createController();
    privateWindow->openInput(QStringLiteral("https://private.example/"), false);
    const auto privateTabId = privateWindow->activeTabId();

    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    for (const auto &space : ask(control, QStringLiteral("agent"), QStringLiteral("spaces"))
             .value(QStringLiteral("spaces"))
             .toArray()) {
        const auto tabs = ask(control, QStringLiteral("agent"), QStringLiteral("tabs"),
            {{QStringLiteral("space"), space.toObject().value(QStringLiteral("id"))}});
        QVERIFY(!ids(tabs.value(QStringLiteral("tabs")).toArray()).contains(privateTabId));
    }
    QCOMPARE(failure(ask(control, QStringLiteral("agent"), QStringLiteral("open"),
                 {{QStringLiteral("url"), QStringLiteral("https://example.com/")},
                     {QStringLiteral("tab"), privateTabId}})),
        QStringLiteral("not-found"));

    // Handed a Private window, the control answers nothing about it at all.
    AgentControl privateControl(privateWindow.get(), config.path());
    for (const auto &verb : {QStringLiteral("spaces"), QStringLiteral("tabs"),
             QStringLiteral("open"), QStringLiteral("close"), QStringLiteral("space new")}) {
        const auto answer = ask(privateControl, QStringLiteral("agent"), verb,
            {{QStringLiteral("url"), QStringLiteral("https://example.com/")}});
        QCOMPARE(failure(answer), QStringLiteral("private"));
    }
    QCOMPARE(privateWindow->tabs()->rowCount(), 1);
}

void AgentControlTest::answersOverASocketOnlyItsUserCanOpen()
{
    QTemporaryDir config;
    QTemporaryDir runtime;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    ControlSocket socket(&control);
    const auto path = runtime.filePath(QStringLiteral("omaweb/control.sock"));
    QVERIFY(socket.listen(path));

    struct stat status {};
    QCOMPARE(::stat(QFile::encodeName(path).constData(), &status), 0);
    QCOMPARE(status.st_mode & 0777, 0600u);
    QCOMPARE(QFileInfo(runtime.filePath(QStringLiteral("omaweb"))).permissions()
            & (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::ExeOther),
        QFileDevice::Permissions {});

    QLocalSocket client;
    client.connectToServer(path);
    QVERIFY(client.waitForConnected(1000));
    const auto request = [&client](const QByteArray &line) {
        client.write(line + '\n');
        client.flush();
        // The server answers on this thread, so waiting has to turn the loop.
        QDeadlineTimer deadline(2000);
        while (!client.canReadLine() && !deadline.hasExpired()) {
            QTest::qWait(5);
        }
        return QJsonDocument::fromJson(client.readLine()).object();
    };
    const auto spaces = request(R"({"verb":"spaces","name":"test"})");
    QVERIFY(succeeded(spaces));
    QCOMPARE(spaces.value(QStringLiteral("spaces")).toArray().size(), 2);
    QCOMPARE(failure(request("not json")), QStringLiteral("bad-request"));
    QCOMPARE(failure(request(R"({"verb":"fly","name":"test"})")), QStringLiteral("bad-request"));

    // A second browser finds the socket answering and leaves it be.
    ControlSocket second(&control);
    QVERIFY(!second.listen(path));
    QVERIFY(succeeded(request(R"({"verb":"spaces","name":"test"})")));
}

QTEST_GUILESS_MAIN(AgentControlTest)
#include "tst_agentcontrol.moc"
