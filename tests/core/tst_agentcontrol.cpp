#include "AgentControl.h"
#include "BrowserController.h"
#include "ControlSocket.h"
#include "PrivateSessionFixture.h"
#include "SessionFixture.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QRegularExpression>
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

// A page verb through the path the socket takes, with its answer caught.
QJsonObject askPage(
    AgentControl &control, const QString &name, const QString &verb, QJsonObject fields = {})
{
    fields.insert(QStringLiteral("name"), name);
    fields.insert(QStringLiteral("verb"), verb);
    QJsonObject answered;
    control.handle(fields, [&answered](const QJsonObject &answer) { answered = answer; });
    return answered;
}

// A tab of an Agent Space the connection makes for it, which is the only
// kind of tab a page verb reaches until Space grants land. Needs Allow agents.
QString openAgentTab(AgentControl &control, const QString &name)
{
    const auto space = ask(control, name, QStringLiteral("space new"),
        {{QStringLiteral("space"), name + QStringLiteral(" work")}})
                           .value(QStringLiteral("space"))
                           .toObject()
                           .value(QStringLiteral("id"))
                           .toString();
    const auto opened = ask(control, name, QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://example.com/")},
            {QStringLiteral("space"), space}});
    return opened.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
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
    void loadsAddressesOnlyInAnAgentsTabs();
    void deletesOnlyTheAgentSpacesItsConnectionCreated();
    void followsAllowAgentsInTheReadersFile();
    void boundsTheConnectionStatesItKeeps();
    void refusesALineTooLongToBeARequest();
    void leavesAPathThatIsNotASocketAlone();
    void gatesThePageVerbsBehindAllowAgentsAndTheAgentsTabs();
    void handsAPageVerbToThePageAndRepliesWithItsAnswer();
    void checksABatchBeforeThePageSeesIt();
    void writesScreenshotsWhereOnlyTheReaderCanRead();
    void refusesWhatIsUnderWayWhenAllowAgentsGoesOff();
    void keepsATabAnAgentTabOnlyWhileAnAgentUsesIt();
    void answersASocketsRequestsInTheOrderAsked();
    void switchesSpaceAndSelectsATabWithAllowAgentsOff();
    void runsOnlyThePublicCommandsInTheWindow();
    void decidesEveryCommandOfTheRegistry();
    void uploadsOnlyInAnAgentSpace();
    void checksADialogStep();
    void givesEachConnectionItsOwnDownloadDirectory();
    void drivesTheWindowsAnAgentTabOpens();
};

void AgentControlTest::gatesOnlyAgentSpacesBehindAllowAgents()
{
    for (const auto &verb : {QStringLiteral("spaces"), QStringLiteral("tabs"),
             QStringLiteral("open"), QStringLiteral("close"), QStringLiteral("space"),
             QStringLiteral("focus"), QStringLiteral("commands"), QStringLiteral("run")}) {
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
        spaceId = browser->createAgentSpace(QStringLiteral("Checks"), QStringLiteral("agent"));
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
    QCOMPARE(restarted->agentSpaceCreator(spaceId), QStringLiteral("agent"));
    const QHash<QString, QString> labels {{spaceId, QStringLiteral("agent")}};
    QCOMPARE(restarted->sessionStore()->agentSpaces(), labels);
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
    QVERIFY(browser->sessionStore()->agentSpaces().isEmpty());
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
    for (const auto &verb :
        {QStringLiteral("spaces"), QStringLiteral("tabs"), QStringLiteral("open"),
            QStringLiteral("close"), QStringLiteral("space new"), QStringLiteral("space"),
            QStringLiteral("focus"), QStringLiteral("commands"), QStringLiteral("run")}) {
        const auto answer = ask(privateControl, QStringLiteral("agent"), verb,
            {{QStringLiteral("url"), QStringLiteral("https://example.com/")},
                {QStringLiteral("space"), QStringLiteral("Private")},
                {QStringLiteral("target"), privateTabId},
                {QStringLiteral("command"), QStringLiteral("close-tab")}});
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

// Until Space grants land an Agent reaches no tab of the reader's, the one
// on show included, even to give it a new address.
void AgentControlTest::loadsAddressesOnlyInAnAgentsTabs()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    QString agentTabId;
    {
        AgentControl control(browser.get(), config.path());
        for (const auto &tabId : {QStringLiteral("personal-tab"), QStringLiteral("work-tab")}) {
            const auto answer = ask(control, QStringLiteral("agent"), QStringLiteral("open"),
                {{QStringLiteral("url"), QStringLiteral("https://example.com/")},
                    {QStringLiteral("tab"), tabId}});
            QCOMPARE(failure(answer), QStringLiteral("refused"));
            QVERIFY(answer.value(QStringLiteral("error")).toString().contains(u"Open a new tab"));
        }
        QCOMPARE(browser->findTab(QStringLiteral("personal-tab"))->url,
            QUrl(QStringLiteral("https://personal.example/")));
        QCOMPARE(browser->findTab(QStringLiteral("work-tab"))->url,
            QUrl(QStringLiteral("https://work.example/")));

        control.setAllowAgents(true);
        QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("space new"))));
        const auto opened = ask(control, QStringLiteral("agent"), QStringLiteral("open"),
            {{QStringLiteral("url"), QStringLiteral("https://example.com/")}});
        agentTabId
            = opened.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
    }

    // A later run knows no tab it opened, and an Agent Space's tabs are the
    // Agents' only while Allow agents is on.
    AgentControl restarted(browser.get(), config.path());
    const QJsonObject elsewhere {{QStringLiteral("url"), QStringLiteral("https://example.org/")},
        {QStringLiteral("tab"), agentTabId}};
    QVERIFY(succeeded(ask(restarted, QStringLiteral("other"), QStringLiteral("open"), elsewhere)));
    restarted.setAllowAgents(false);
    QCOMPARE(failure(ask(restarted, QStringLiteral("other"), QStringLiteral("open"), elsewhere)),
        QStringLiteral("refused"));
}

void AgentControlTest::deletesOnlyTheAgentSpacesItsConnectionCreated()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);

    const auto created = ask(control, QStringLiteral("first"), QStringLiteral("space new"));
    const auto spaceId
        = created.value(QStringLiteral("space")).toObject().value(QStringLiteral("id")).toString();
    const QJsonObject target {{QStringLiteral("space"), spaceId}};
    const auto refused
        = ask(control, QStringLiteral("second"), QStringLiteral("space delete"), target);
    QCOMPARE(failure(refused), QStringLiteral("refused"));
    QVERIFY(browser->agentSpace(spaceId));
    QVERIFY(
        succeeded(ask(control, QStringLiteral("first"), QStringLiteral("space delete"), target)));
    QVERIFY(!browser->agentSpace(spaceId));
    QCOMPARE(browser->spaces()->rowCount(), 2);
}

// The reader's file is the setting, so a change there reaches the browser
// already running, and turning it off detaches every connection at once.
void AgentControlTest::followsAllowAgentsInTheReadersFile()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    QSignalSpy changed(&control, &AgentControl::allowAgentsChanged);

    // Written the way an editor or PrivacyFile writes, by replacing the file.
    const auto write = [&config](const QByteArray &contents) {
        QSaveFile file(config.filePath(QStringLiteral("privacy.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
        QVERIFY(file.commit());
    };
    write(R"({"allow-agents": true})");
    QTRY_VERIFY(control.allowAgents());
    QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("space new"))));
    QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://example.com/")}})));

    write(R"({"allow-agents": false, "global-privacy-control": true})");
    QTRY_VERIFY(!control.allowAgents());
    QCOMPARE(failure(ask(control, QStringLiteral("agent"), QStringLiteral("close"))),
        QStringLiteral("no-current-tab"));
    QCOMPARE(failure(ask(control, QStringLiteral("agent"), QStringLiteral("space new"))),
        QStringLiteral("allow-agents"));

    // Still followed after being replaced twice, and a file that is not the
    // reader's `true` keeps Agents out.
    write(R"({"allow-agents": true})");
    QTRY_VERIFY(control.allowAgents());
    write("not json");
    QTRY_VERIFY(!control.allowAgents());
    QCOMPARE(changed.count(), 4);
}

void AgentControlTest::boundsTheConnectionStatesItKeeps()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());

    const auto currentOf = [&control](const QString &name) {
        for (const auto &tab :
            ask(control, name, QStringLiteral("tabs")).value(QStringLiteral("tabs")).toArray()) {
            if (tab.toObject().value(QStringLiteral("current")).toBool()) {
                return true;
            }
        }
        return false;
    };
    QVERIFY(succeeded(ask(control, QStringLiteral("kept"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://kept.example/")}})));
    QVERIFY(succeeded(ask(control, QStringLiteral("dropped"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://dropped.example/")}})));
    for (qsizetype index = 0; index < AgentControl::maximumConnections - 2; ++index) {
        ask(control, QStringLiteral("name-%1").arg(index), QStringLiteral("spaces"));
        if (index == 0) {
            QVERIFY(currentOf(QStringLiteral("kept")));
        }
    }
    // One more name than it keeps: the state used longest ago makes room.
    ask(control, QStringLiteral("one-too-many"), QStringLiteral("spaces"));
    QVERIFY(currentOf(QStringLiteral("kept")));
    QVERIFY(!currentOf(QStringLiteral("dropped")));
}

void AgentControlTest::refusesALineTooLongToBeARequest()
{
    QTemporaryDir config;
    QTemporaryDir runtime(QDir::tempPath() + QStringLiteral("/omaweb-XXXXXX"));
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    ControlSocket socket(&control);
    const auto path = runtime.filePath(QStringLiteral("c.sock"));
    QVERIFY(socket.listen(path));

    QLocalSocket client;
    client.connectToServer(path);
    QVERIFY(client.waitForConnected(1000));
    // A request padded past the limit, followed by one that would be read as
    // a request of its own if the first were cut short and carried on from.
    QByteArray padded = R"({"verb":"spaces","name":")";
    padded += QByteArray(70 * 1024, 'a');
    padded += R"("})";
    client.write(padded + '\n' + R"({"verb":"spaces","name":"after"})" + '\n');
    client.flush();
    QTRY_VERIFY_WITH_TIMEOUT(
        client.state() == QLocalSocket::UnconnectedState || client.canReadLine(), 2000);
    const auto answer = QJsonDocument::fromJson(client.readLine()).object();
    QCOMPARE(failure(answer), QStringLiteral("bad-request"));
    QVERIFY(answer.value(QStringLiteral("error")).toString().contains(u"too long"));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QLocalSocket::UnconnectedState, 2000);
    QVERIFY(!client.canReadLine());
}

void AgentControlTest::leavesAPathThatIsNotASocketAlone()
{
    QTemporaryDir config;
    QTemporaryDir runtime(QDir::tempPath() + QStringLiteral("/omaweb-XXXXXX"));
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    const auto path = runtime.filePath(QStringLiteral("c.sock"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("the reader's");
    }
    ControlSocket socket(&control);
    QVERIFY(!socket.listen(path));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("the reader's"));
}

// A page verb needs Allow agents and a tab an Agent may drive, as loading an
// address in one does.
void AgentControlTest::gatesThePageVerbsBehindAllowAgentsAndTheAgentsTabs()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    QSignalSpy requested(&control, &AgentControl::pageRequested);
    connect(&control, &AgentControl::pageRequested, this, [] { });

    control.setAllowAgents(true);
    const auto agentTab = openAgentTab(control, QStringLiteral("agent"));
    control.setAllowAgents(false);
    for (const auto &verb : {QStringLiteral("look"), QStringLiteral("read"), QStringLiteral("do"),
             QStringLiteral("shot"), QStringLiteral("eval")}) {
        QCOMPARE(failure(askPage(
                     control, QStringLiteral("agent"), verb, {{QStringLiteral("tab"), agentTab}})),
            QStringLiteral("allow-agents"));
    }
    control.setAllowAgents(true);
    // A tab the Agent opened itself in the reader's Space is still the
    // reader's page, with their cookies and logins.
    const auto inReadersSpace = ask(control, QStringLiteral("agent"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://reader.example/")},
            {QStringLiteral("space"), QStringLiteral("personal")}})
                                    .value(QStringLiteral("tab"))
                                    .toObject()
                                    .value(QStringLiteral("id"))
                                    .toString();
    QVERIFY(!inReadersSpace.isEmpty());
    QVERIFY(!control.agentTabIds().contains(inReadersSpace));
    QCOMPARE(failure(askPage(control, QStringLiteral("agent"), QStringLiteral("look"))),
        QStringLiteral("refused"));
    QCOMPARE(failure(askPage(control, QStringLiteral("fresh"), QStringLiteral("look"))),
        QStringLiteral("no-current-tab"));
    for (const auto &tabId : {QStringLiteral("personal-tab"), QStringLiteral("work-tab"),
             QStringLiteral("personal-pin")}) {
        QCOMPARE(failure(askPage(control, QStringLiteral("agent"), QStringLiteral("look"),
                     {{QStringLiteral("tab"), tabId}})),
            QStringLiteral("refused"));
    }
    QCOMPARE(requested.count(), 0);
    // The page verb path answers a browser command too, and the one-shot path
    // answers no page verb.
    QVERIFY(succeeded(askPage(control, QStringLiteral("agent"), QStringLiteral("spaces"))));
    QCOMPARE(failure(ask(control, QStringLiteral("agent"), QStringLiteral("look"))),
        QStringLiteral("pending"));
    QVERIFY(!agentTab.isEmpty());
}

void AgentControlTest::handsAPageVerbToThePageAndRepliesWithItsAnswer()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);

    // Nothing holds a page to answer with.
    const auto agentTab = openAgentTab(control, QStringLiteral("agent"));
    const auto agentSpace = browser->findTab(agentTab)->spaceId;
    QCOMPARE(failure(askPage(control, QStringLiteral("agent"), QStringLiteral("look"))),
        QStringLiteral("unavailable"));

    QSignalSpy requested(&control, &AgentControl::pageRequested);
    QList<QJsonObject> replies;
    control.handle(
        {{QStringLiteral("verb"), QStringLiteral("look")},
            {QStringLiteral("name"), QStringLiteral("agent")}, {QStringLiteral("all"), true}},
        [&replies](const QJsonObject &answer) { replies.append(answer); });
    QCOMPARE(requested.count(), 1);
    QVERIFY(replies.isEmpty());
    const auto requestId = requested.at(0).at(0).toInt();
    const auto request = requested.at(0).at(1).toMap();
    QCOMPARE(request.value(QStringLiteral("verb")).toString(), QStringLiteral("look"));
    QCOMPARE(request.value(QStringLiteral("tabId")).toString(), agentTab);
    QCOMPARE(request.value(QStringLiteral("spaceId")).toString(), agentSpace);
    QCOMPARE(request.value(QStringLiteral("name")).toString(), QStringLiteral("agent"));
    QCOMPARE(request.value(QStringLiteral("arguments")).toMap().value(QStringLiteral("all")),
        QVariant(true));
    QVERIFY(control.agentTabIds().contains(agentTab));
    QCOMPARE(control.agentTab(agentTab).value(QStringLiteral("spaceId")).toString(), agentSpace);

    control.answerPage(requestId,
        {{QStringLiteral("ok"), true},
            {QStringLiteral("look"),
                QVariantMap {{QStringLiteral("title"), QStringLiteral("Example")}}}});
    QCOMPARE(replies.size(), 1);
    QVERIFY(succeeded(replies.constFirst()));
    QCOMPARE(replies.constFirst().value(QStringLiteral("tab")).toString(), agentTab);
    QCOMPARE(replies.constFirst()
                 .value(QStringLiteral("look"))
                 .toObject()
                 .value(QStringLiteral("title"))
                 .toString(),
        QStringLiteral("Example"));
    // An answer comes once, and one for a request nobody made is dropped.
    control.answerPage(requestId, {{QStringLiteral("ok"), true}});
    control.answerPage(requestId + 99, {{QStringLiteral("ok"), true}});
    QCOMPARE(replies.size(), 1);

    // An answer that says nothing of how it went is a failure, not a success.
    control.handle({{QStringLiteral("verb"), QStringLiteral("read")},
                       {QStringLiteral("name"), QStringLiteral("agent")}},
        [&replies](const QJsonObject &answer) { replies.append(answer); });
    control.answerPage(requested.at(1).at(0).toInt(), {{QStringLiteral("markdown"), QString()}});
    QCOMPARE(failure(replies.constLast()), QStringLiteral("failed"));
}

void AgentControlTest::checksABatchBeforeThePageSeesIt()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    QSignalSpy requested(&control, &AgentControl::pageRequested);
    openAgentTab(control, QStringLiteral("agent"));

    const auto batch = [&control](const QJsonArray &steps, QJsonObject fields = {}) {
        fields.insert(QStringLiteral("steps"), steps);
        return askPage(control, QStringLiteral("agent"), QStringLiteral("do"), fields);
    };
    const auto step = [](const QString &action, QJsonObject fields = {}) {
        fields.insert(QStringLiteral("action"), action);
        return fields;
    };
    const auto click
        = step(QStringLiteral("click"), {{QStringLiteral("target"), QStringLiteral("3")}});
    QCOMPARE(failure(batch({})), QStringLiteral("bad-request"));
    QCOMPARE(failure(batch({step(QStringLiteral("dance"))})), QStringLiteral("bad-request"));
    QCOMPARE(failure(batch({step(QStringLiteral("click"))})), QStringLiteral("bad-request"));
    QCOMPARE(failure(batch({step(
                 QStringLiteral("fill"), {{QStringLiteral("target"), QStringLiteral("3")}})})),
        QStringLiteral("bad-request"));
    QCOMPARE(failure(batch({step(QStringLiteral("wait"),
                 {{QStringLiteral("text"), QStringLiteral("a")},
                     {QStringLiteral("url"), QStringLiteral("b")}})})),
        QStringLiteral("bad-request"));
    QJsonArray many;
    for (auto index = 0; index < 51; ++index) {
        many.append(click);
    }
    QCOMPARE(failure(batch(many)), QStringLiteral("bad-request"));
    QCOMPARE(failure(batch({click}, {{QStringLiteral("settle"), 20000}})),
        QStringLiteral("bad-request"));
    QCOMPARE(
        failure(batch({click}, {{QStringLiteral("timeout"), 5}})), QStringLiteral("bad-request"));
    QCOMPARE(requested.count(), 0);

    connect(&control, &AgentControl::pageRequested, this, [] { });
    batch({click, step(QStringLiteral("back"))});
    QCOMPARE(requested.count(), 1);
    const auto arguments = requested.at(0).at(1).toMap().value(QStringLiteral("arguments")).toMap();
    QCOMPARE(arguments.value(QStringLiteral("steps")).toList().size(), 2);
    QCOMPARE(arguments.value(QStringLiteral("settle")).toInt(), 300);
    QCOMPARE(arguments.value(QStringLiteral("timeout")).toInt(), 10000);
}

void AgentControlTest::writesScreenshotsWhereOnlyTheReaderCanRead()
{
    QTemporaryDir config;
    QTemporaryDir runtime;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    const auto shots = runtime.filePath(QStringLiteral("shots"));
    control.setShotDirectory(shots);
    QSignalSpy requested(&control, &AgentControl::pageRequested);
    connect(&control, &AgentControl::pageRequested, this, [] { });
    openAgentTab(control, QStringLiteral("agent"));
    const auto destination = [&requested](qsizetype index) {
        return requested.at(index)
            .at(1)
            .toMap()
            .value(QStringLiteral("arguments"))
            .toMap()
            .value(QStringLiteral("destination"))
            .toString();
    };
    const auto mode = [](const QString &path) {
        struct stat status {};
        return ::stat(QFile::encodeName(path).constData(), &status) == 0 ? status.st_mode & 0777
                                                                         : 0u;
    };

    // A name is a name inside the shots directory and nothing else.
    for (const auto &name : {QStringLiteral("../escape.png"), QStringLiteral("/tmp/escape.png"),
             QStringLiteral("sub/page.png"), QStringLiteral(".."), QStringLiteral(".hidden.png")}) {
        QCOMPARE(failure(askPage(control, QStringLiteral("agent"), QStringLiteral("shot"),
                     {{QStringLiteral("output"), name}})),
            QStringLiteral("bad-request"));
    }
    QCOMPARE(requested.count(), 0);

    askPage(control, QStringLiteral("agent"), QStringLiteral("shot"),
        {{QStringLiteral("output"), QStringLiteral("page.png")}});
    QCOMPARE(requested.count(), 1);
    QCOMPARE(destination(0), QDir(shots).filePath(QStringLiteral("page.png")));
    QCOMPARE(mode(shots), 0700u);
    QCOMPARE(mode(destination(0)), 0600u);
    // An existing file is never written over.
    QCOMPARE(failure(askPage(control, QStringLiteral("agent"), QStringLiteral("shot"),
                 {{QStringLiteral("output"), QStringLiteral("page.png")}})),
        QStringLiteral("bad-request"));

    // Unnamed shots get names of their own, and the oldest go.
    for (auto index = 0; index < 60; ++index) {
        askPage(control, QStringLiteral("agent"), QStringLiteral("shot"));
    }
    QCOMPARE(requested.count(), 61);
    QVERIFY(destination(1) != destination(2));
    QVERIFY(destination(2).endsWith(u".png"));
    QVERIFY(QDir(shots).entryList({QStringLiteral("*.png")}, QDir::Files).size() <= 50);
    QVERIFY(QFileInfo::exists(destination(60)));
}

// Turning Allow agents off refuses what was already asked of a page, and
// tells the pages to stop.
void AgentControlTest::refusesWhatIsUnderWayWhenAllowAgentsGoesOff()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    QSignalSpy requested(&control, &AgentControl::pageRequested);
    QSignalSpy cancelled(&control, &AgentControl::pageRequestsCancelled);
    connect(&control, &AgentControl::pageRequested, this, [] { });
    openAgentTab(control, QStringLiteral("agent"));

    QList<QJsonObject> replies;
    control.handle(
        {{QStringLiteral("verb"), QStringLiteral("do")},
            {QStringLiteral("name"), QStringLiteral("agent")},
            {QStringLiteral("steps"),
                QJsonArray {QJsonObject {{QStringLiteral("action"), QStringLiteral("back")}}}}},
        [&replies](const QJsonObject &answer) { replies.append(answer); });
    QCOMPARE(requested.count(), 1);
    QVERIFY(replies.isEmpty());

    control.setAllowAgents(false);
    QCOMPARE(cancelled.count(), 1);
    QCOMPARE(replies.size(), 1);
    QCOMPARE(failure(replies.constFirst()), QStringLiteral("allow-agents"));
    // What the page says afterwards goes nowhere.
    control.answerPage(requested.at(0).at(0).toInt(),
        {{QStringLiteral("ok"), true}, {QStringLiteral("look"), QVariantMap {}}});
    QCOMPARE(replies.size(), 1);
}

// The CLI holds no connection open, so a tab is an Agent tab while Agents use
// it and for a while after, and stops being one when it is closed or Allow
// agents goes off.
void AgentControlTest::keepsATabAnAgentTabOnlyWhileAnAgentUsesIt()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    QSignalSpy changed(&control, &AgentControl::agentTabsChanged);

    // A browser command opens a tab, and only a tab of an Agent Space with
    // Allow agents on is an Agent's to render.
    QVERIFY(succeeded(ask(control, QStringLiteral("script"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://example.com/")}})));
    QVERIFY(control.agentTabIds().isEmpty());
    control.setAllowAgents(true);
    QVERIFY(succeeded(ask(control, QStringLiteral("script"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://example.com/")}})));
    QVERIFY(control.agentTabIds().isEmpty());
    const auto first = openAgentTab(control, QStringLiteral("agent"));
    QCOMPARE(control.agentTabIds(), QStringList {first});
    QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("close"))));
    QVERIFY(control.agentTabIds().isEmpty());

    const auto second = openAgentTab(control, QStringLiteral("agent"));
    QCOMPARE(control.agentTabIds(), QStringList {second});
    control.setAllowAgents(false);
    QVERIFY(control.agentTabIds().isEmpty());

    control.setAllowAgents(true);
    control.setAttachmentIdleMs(100);
    const auto third = openAgentTab(control, QStringLiteral("agent"));
    QCOMPARE(control.agentTabIds(), QStringList {third});
    QTRY_VERIFY_WITH_TIMEOUT(control.agentTabIds().isEmpty(), 2000);
    QVERIFY(changed.count() >= 6);
    QVERIFY(control.agentTab(third).isEmpty());
}

// A page verb is answered later than a browser command asked after it on the
// same connection, and the answers still come back in the order asked.
void AgentControlTest::answersASocketsRequestsInTheOrderAsked()
{
    QTemporaryDir config;
    QTemporaryDir runtime;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    openAgentTab(control, QStringLiteral("agent"));
    QSignalSpy requested(&control, &AgentControl::pageRequested);
    connect(&control, &AgentControl::pageRequested, this, [] { });
    ControlSocket socket(&control);
    const auto path = runtime.filePath(QStringLiteral("omaweb/control.sock"));
    QVERIFY(socket.listen(path));

    QLocalSocket client;
    client.connectToServer(path);
    QVERIFY(client.waitForConnected(1000));
    client.write(R"({"verb":"look","name":"agent"})"
                 "\n"
                 R"({"verb":"spaces","name":"agent"})"
                 "\n");
    client.flush();
    QTRY_COMPARE(requested.count(), 1);
    QTest::qWait(50);
    QVERIFY(!client.canReadLine());
    control.answerPage(requested.at(0).at(0).toInt(),
        {{QStringLiteral("ok"), true}, {QStringLiteral("look"), QVariantMap {}}});
    QTRY_VERIFY(client.canReadLine());
    const auto first = QJsonDocument::fromJson(client.readLine()).object();
    QVERIFY(first.contains(QStringLiteral("look")));
    QTRY_VERIFY(client.canReadLine());
    const auto second = QJsonDocument::fromJson(client.readLine()).object();
    QVERIFY(second.contains(QStringLiteral("spaces")));
}

// What a keybind needs: another Space on show, or a tab chosen by its address,
// with nothing turned on.
void AgentControlTest::switchesSpaceAndSelectsATabWithAllowAgentsOff()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    QVERIFY(!control.allowAgents());
    const auto script = QStringLiteral("keybind");

    QVERIFY(succeeded(ask(control, script, QStringLiteral("space"),
        {{QStringLiteral("space"), QStringLiteral("Work")}})));
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("work"));
    QCOMPARE(failure(ask(control, script, QStringLiteral("space"),
                 {{QStringLiteral("space"), QStringLiteral("Nowhere")}})),
        QStringLiteral("not-found"));

    // A part of an address, in whichever Space holds the tab.
    const auto focused = ask(control, script, QStringLiteral("focus"),
        {{QStringLiteral("target"), QStringLiteral("PERSONAL.example")}});
    QVERIFY(succeeded(focused));
    QCOMPARE(focused.value(QStringLiteral("tab")).toString(), QStringLiteral("personal-tab"));
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(browser->activeTabId(), QStringLiteral("personal-tab"));

    // A tab's id comes before any address that holds it, and a Pinned tab is
    // the reader's to look at like any other.
    QVERIFY(succeeded(ask(control, script, QStringLiteral("focus"),
        {{QStringLiteral("target"), QStringLiteral("personal-pin")}})));
    QCOMPARE(browser->activeTabId(), QStringLiteral("personal-pin"));
    QVERIFY(succeeded(ask(control, script, QStringLiteral("focus"),
        {{QStringLiteral("target"), QStringLiteral("work-tab")}})));
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("work"));
    QCOMPARE(browser->activeTabId(), QStringLiteral("work-tab"));

    QCOMPARE(failure(ask(control, script, QStringLiteral("focus"),
                 {{QStringLiteral("target"), QStringLiteral("nowhere.example")}})),
        QStringLiteral("not-found"));
}

// The core decides what is public, and the window, which holds the
// registry, runs it and says whether it ran.
void AgentControlTest::runsOnlyThePublicCommandsInTheWindow()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    const auto script = QStringLiteral("keybind");
    const auto run = [&](const QString &command, const QJsonValue &argument = {}) {
        QJsonObject fields {{QStringLiteral("command"), command}};
        if (!argument.isNull()) {
            fields.insert(QStringLiteral("argument"), argument);
        }
        return ask(control, script, QStringLiteral("run"), fields);
    };

    QCOMPARE(failure(run(QStringLiteral("toggle-sidebar"))), QStringLiteral("unavailable"));

    QList<QVariantMap> asked;
    QVariantMap windowAnswer {{QStringLiteral("ok"), true}};
    connect(&control, &AgentControl::commandRequested, &control,
        [&](int requestId, const QVariantMap &request) {
            asked.append(request);
            control.answerCommand(requestId, windowAnswer);
        });

    QVERIFY(succeeded(run(QStringLiteral("toggle-sidebar"))));
    QCOMPARE(asked.size(), 1);
    QCOMPARE(asked.constLast().value(QStringLiteral("command")).toString(),
        QStringLiteral("toggle-sidebar"));
    QCOMPARE(asked.constLast().value(QStringLiteral("argument")).toInt(), -1);
    QCOMPARE(asked.constLast().value(QStringLiteral("commands")).toStringList(),
        AgentControl::publicCommands());

    // Counted from 1 as the keys are, and from 0 in the window.
    QVERIFY(succeeded(run(QStringLiteral("select-tab"), 2)));
    QCOMPARE(asked.constLast().value(QStringLiteral("argument")).toInt(), 1);
    QCOMPARE(failure(run(QStringLiteral("select-tab"))), QStringLiteral("bad-request"));
    QCOMPARE(failure(run(QStringLiteral("select-space"), 0)), QStringLiteral("bad-request"));
    QCOMPARE(failure(run(QStringLiteral("reload"), 1)), QStringLiteral("bad-request"));
    QCOMPARE(asked.size(), 2);

    // Refused by name, and never put to the window.
    for (const auto &command : {QStringLiteral("private-window"), QStringLiteral("screenshot-page"),
             QStringLiteral("copy-full-page-screenshot"), QStringLiteral("rm-rf")}) {
        const auto refused = run(command);
        QCOMPARE(failure(refused), QStringLiteral("refused"));
        QVERIFY2(refused.value(QStringLiteral("error")).toString().contains(command),
            qPrintable(command));
    }
    QCOMPARE(asked.size(), 2);

    windowAnswer
        = {{QStringLiteral("ok"), false}, {QStringLiteral("code"), QStringLiteral("unavailable")},
            {QStringLiteral("error"), QStringLiteral("\"find-next\" is not available now.")}};
    QCOMPARE(failure(run(QStringLiteral("find-next"))), QStringLiteral("unavailable"));

    windowAnswer = {{QStringLiteral("ok"), true},
        {QStringLiteral("commands"),
            QVariantList {QVariantMap {{QStringLiteral("command"), QStringLiteral("reload")},
                {QStringLiteral("title"), QStringLiteral("Reload")}}}}};
    const auto listed = ask(control, script, QStringLiteral("commands"));
    QVERIFY(succeeded(listed));
    QCOMPARE(
        asked.constLast().value(QStringLiteral("verb")).toString(), QStringLiteral("commands"));
    QCOMPARE(listed.value(QStringLiteral("commands")).toArray().size(), 1);

    // A window that says nothing has not run it.
    windowAnswer = {};
    QCOMPARE(failure(run(QStringLiteral("reload"))), QStringLiteral("failed"));
}

// A command added to the registry is decided here, public or kept in, rather
// than let out or left out without anyone choosing.
void AgentControlTest::decidesEveryCommandOfTheRegistry()
{
    QFile source(QStringLiteral(OMAWEB_BROWSER_COMMANDS_QML));
    QVERIFY(source.open(QIODevice::ReadOnly));
    const auto text = QString::fromUtf8(source.readAll());
    const auto start = text.indexOf(QStringLiteral("readonly property var descriptions"));
    const auto end = text.indexOf(QStringLiteral("readonly property var groupSymbols"));
    QVERIFY(start >= 0 && end > start);
    static const QRegularExpression key(
        QStringLiteral("^\\s*\"([a-z-]+)\": \\{"), QRegularExpression::MultilineOption);
    QStringList registry;
    for (const auto &match : key.globalMatch(text.mid(start, end - start))) {
        registry.append(match.captured(1));
    }
    QVERIFY(registry.size() > 50);

    const QStringList keptIn {QStringLiteral("screenshot-page"), QStringLiteral("copy-screenshot"),
        QStringLiteral("screenshot-full-page"), QStringLiteral("copy-full-page-screenshot"),
        QStringLiteral("private-window")};
    QStringList decided = AgentControl::publicCommands() + keptIn;
    decided.sort();
    registry.sort();
    QCOMPARE(decided, registry);
}

QTEST_GUILESS_MAIN(AgentControlTest)
namespace {

QJsonObject uploadStep(const QString &target, const QStringList &files)
{
    return {{QStringLiteral("action"), QStringLiteral("upload")},
        {QStringLiteral("target"), target},
        {QStringLiteral("files"), QJsonArray::fromStringList(files)}};
}

} // namespace

// An upload is how a page an Agent was sent to could take the reader's files,
// so it is refused outside an Agent Space before anything else is asked about
// the page. That holds whatever lets an Agent into one of the reader's Spaces,
// a Space grant included: the refusal is its own, not the one for a page the
// Agent may not read.
void AgentControlTest::uploadsOnlyInAnAgentSpace()
{
    QTemporaryDir config;
    QTemporaryDir files;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    QSignalSpy requested(&control, &AgentControl::pageRequested);
    connect(&control, &AgentControl::pageRequested, this, [] { });

    const auto report = QDir(files.path()).filePath(QStringLiteral("report.pdf"));
    QFile file(report);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("%PDF");
    file.close();
    const auto batch
        = [&control](const QString &name, const QJsonArray &steps, QJsonObject fields = {}) {
              fields.insert(QStringLiteral("steps"), steps);
              return askPage(control, name, QStringLiteral("do"), fields);
          };

    // The reader's own Spaces: a tab of theirs, and one the Agent opened there.
    const auto opened = ask(control, QStringLiteral("agent"), QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://reader.example/")},
            {QStringLiteral("space"), QStringLiteral("work")}})
                            .value(QStringLiteral("tab"))
                            .toObject()
                            .value(QStringLiteral("id"))
                            .toString();
    for (const auto &tabId : {opened, QStringLiteral("personal-tab"), QStringLiteral("work-tab")}) {
        const auto refused = batch(QStringLiteral("agent"),
            {uploadStep(QStringLiteral("4"), {report})}, {{QStringLiteral("tab"), tabId}});
        QCOMPARE(failure(refused), QStringLiteral("refused"));
        QVERIFY2(refused.value(QStringLiteral("error")).toString().contains(u"upload"),
            qPrintable(refused.value(QStringLiteral("error")).toString()));
        // A batch without one is refused only for being the reader's page.
        const auto reading = batch(QStringLiteral("agent"),
            {QJsonObject {{QStringLiteral("action"), QStringLiteral("back")}}},
            {{QStringLiteral("tab"), tabId}});
        QCOMPARE(failure(reading), QStringLiteral("refused"));
        QVERIFY(!reading.value(QStringLiteral("error")).toString().contains(u"upload"));
    }
    QCOMPARE(requested.count(), 0);

    // In an Agent Space the files named, and only those, reach the page, each
    // one named in full and there to read.
    const auto agentTab = openAgentTab(control, QStringLiteral("agent"));
    QCOMPARE(failure(batch(QStringLiteral("agent"), {uploadStep(QStringLiteral("4"), {})})),
        QStringLiteral("bad-request"));
    QCOMPARE(failure(batch(QStringLiteral("agent"),
                 {uploadStep(QStringLiteral("4"), {QStringLiteral("report.pdf")})})),
        QStringLiteral("bad-request"));
    QCOMPARE(failure(batch(QStringLiteral("agent"),
                 {uploadStep(QStringLiteral("4"),
                     {QDir(files.path()).filePath(QStringLiteral("missing.pdf"))})})),
        QStringLiteral("bad-request"));
    QCOMPARE(
        failure(batch(QStringLiteral("agent"), {uploadStep(QStringLiteral("4"), {files.path()})})),
        QStringLiteral("bad-request"));
    QStringList tooMany;
    for (auto index = 0; index < 17; ++index) {
        tooMany.append(report);
    }
    QCOMPARE(failure(batch(QStringLiteral("agent"), {uploadStep(QStringLiteral("4"), tooMany)})),
        QStringLiteral("bad-request"));
    QCOMPARE(requested.count(), 0);

    const auto roundabout = QDir(files.path())
                                .filePath(QStringLiteral("../") + QFileInfo(files.path()).fileName()
                                    + QStringLiteral("/report.pdf"));
    batch(QStringLiteral("agent"), {uploadStep(QStringLiteral("4"), {roundabout})});
    QCOMPARE(requested.count(), 1);
    const auto request = requested.at(0).at(1).toMap();
    QCOMPARE(request.value(QStringLiteral("tabId")).toString(), agentTab);
    const auto step = request.value(QStringLiteral("arguments"))
                          .toMap()
                          .value(QStringLiteral("steps"))
                          .toList()
                          .constFirst()
                          .toMap();
    QCOMPARE(step.value(QStringLiteral("files")).toStringList(),
        QStringList {QFileInfo(report).canonicalFilePath()});
}

void AgentControlTest::checksADialogStep()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    QSignalSpy requested(&control, &AgentControl::pageRequested);
    connect(&control, &AgentControl::pageRequested, this, [] { });
    openAgentTab(control, QStringLiteral("agent"));

    const auto dialog = [&control](QJsonObject step) {
        step.insert(QStringLiteral("action"), QStringLiteral("dialog"));
        return askPage(control, QStringLiteral("agent"), QStringLiteral("do"),
            {{QStringLiteral("steps"), QJsonArray {step}}});
    };
    QCOMPARE(failure(dialog({})), QStringLiteral("bad-request"));
    QCOMPARE(failure(dialog({{QStringLiteral("answer"), QStringLiteral("maybe")}})),
        QStringLiteral("bad-request"));
    QCOMPARE(failure(dialog({{QStringLiteral("answer"), QStringLiteral("accept")},
                 {QStringLiteral("text"), 5}})),
        QStringLiteral("bad-request"));
    QCOMPARE(requested.count(), 0);
    dialog({{QStringLiteral("answer"), QStringLiteral("accept")},
        {QStringLiteral("text"), QStringLiteral("Helsinki")}});
    dialog({{QStringLiteral("answer"), QStringLiteral("dismiss")}});
    QCOMPARE(requested.count(), 2);
}

// A download from an Agent tab lands in a directory of the connection's own,
// under the reader's downloads location, and whatever the connection calls
// itself the directory stays inside it.
void AgentControlTest::givesEachConnectionItsOwnDownloadDirectory()
{
    QTemporaryDir config;
    QTemporaryDir downloads;
    SessionFixture fixture(readersSession(), config.path());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    QVERIFY(browser->setDownloadDirectory(downloads.path()));
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    QSignalSpy requested(&control, &AgentControl::pageRequested);
    connect(&control, &AgentControl::pageRequested, this, [] { });

    const auto agents = QDir(downloads.path()).filePath(QStringLiteral("Agents"));
    const auto tab = openAgentTab(control, QStringLiteral("claude"));
    QCOMPARE(control.agentTab(tab).value(QStringLiteral("downloadDirectory")).toString(),
        QDir(agents).filePath(QStringLiteral("claude")));
    askPage(control, QStringLiteral("claude"), QStringLiteral("look"));
    QCOMPARE(requested.at(0).at(1).toMap().value(QStringLiteral("downloadDirectory")).toString(),
        QDir(agents).filePath(QStringLiteral("claude")));

    // Another connection using the same tab takes its downloads with it,
    // which the page hears.
    QSignalSpy handedOver(&control, &AgentControl::agentTabsChanged);
    const auto sly = QStringLiteral("../../.ssh/x");
    askPage(control, sly, QStringLiteral("look"), {{QStringLiteral("tab"), tab}});
    const auto directory
        = requested.at(1).at(1).toMap().value(QStringLiteral("downloadDirectory")).toString();
    QCOMPARE(QFileInfo(directory).absolutePath(), QFileInfo(agents).absoluteFilePath());
    QVERIFY(!QFileInfo(directory).fileName().startsWith(u'.'));
    QCOMPARE(
        control.agentTab(tab).value(QStringLiteral("downloadDirectory")).toString(), directory);
    QCOMPARE(handedOver.count(), 1);
    // Nothing is made until something is downloaded.
    QVERIFY(!QFileInfo::exists(agents));
}

// An Auxiliary window an Agent tab's page opens is the Agent's: `do` names it,
// `tabs` lists it, the page verbs reach it by its id, `close` closes it, and
// it stops being the Agent's when Allow agents goes off.
void AgentControlTest::drivesTheWindowsAnAgentTabOpens()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    QSignalSpy requested(&control, &AgentControl::pageRequested);
    QSignalSpy windows(&control, &AgentControl::agentWindowsChanged);
    QSignalSpy closing(&control, &AgentControl::windowCloseRequested);
    connect(&control, &AgentControl::pageRequested, this, [] { });

    // A window the reader's own page opens stays the reader's.
    QVERIFY(control.attachWindow(QStringLiteral("personal-tab")).isEmpty());
    const auto tab = openAgentTab(control, QStringLiteral("agent"));
    const auto spaceId = browser->findTab(tab)->spaceId;

    // The click that opens it is answered with its id.
    QList<QJsonObject> replies;
    control.handle(
        {{QStringLiteral("verb"), QStringLiteral("do")},
            {QStringLiteral("name"), QStringLiteral("agent")},
            {QStringLiteral("steps"),
                QJsonArray {QJsonObject {{QStringLiteral("action"), QStringLiteral("click")},
                    {QStringLiteral("target"), QStringLiteral("2")}}}}},
        [&replies](const QJsonObject &answer) { replies.append(answer); });
    const auto window = control.attachWindow(tab);
    QVERIFY(!window.isEmpty());
    QCOMPARE(windows.count(), 1);
    QCOMPARE(control.agentWindowIds(), QStringList {window});
    QCOMPARE(control.agentWindow(window).value(QStringLiteral("openerTabId")).toString(), tab);
    QCOMPARE(control.agentWindow(window).value(QStringLiteral("spaceId")).toString(), spaceId);
    QCOMPARE(control.agentWindow(window).value(QStringLiteral("connection")).toString(),
        QStringLiteral("agent"));
    control.answerPage(requested.at(0).at(0).toInt(), {{QStringLiteral("ok"), true}});
    QCOMPARE(replies.constFirst().value(QStringLiteral("opened")).toArray(), QJsonArray {window});
    // Said once.
    control.handle({{QStringLiteral("verb"), QStringLiteral("look")},
                       {QStringLiteral("name"), QStringLiteral("agent")}},
        [&replies](const QJsonObject &answer) { replies.append(answer); });
    control.answerPage(requested.at(1).at(0).toInt(), {{QStringLiteral("ok"), true}});
    QVERIFY(!replies.constLast().contains(QStringLiteral("opened")));

    const auto listed = ask(control, QStringLiteral("agent"), QStringLiteral("tabs"))
                            .value(QStringLiteral("tabs"))
                            .toArray();
    QVERIFY(ids(listed).contains(window));

    // A page verb reaches the window, answered by it and not by the tab.
    askPage(control, QStringLiteral("agent"), QStringLiteral("look"),
        {{QStringLiteral("tab"), window}});
    const auto request = requested.at(2).at(1).toMap();
    QCOMPARE(request.value(QStringLiteral("tabId")).toString(), window);
    QCOMPARE(request.value(QStringLiteral("window")).toBool(), true);
    QCOMPARE(request.value(QStringLiteral("spaceId")).toString(), spaceId);
    // Answered, so nothing is left waiting on a reply this test has let go.
    control.answerPage(requested.at(2).at(0).toInt(), {{QStringLiteral("ok"), true}});
    // Its lines are kept, as an Agent tab's are.
    control.recordConsoleMessage(window, QStringLiteral("1"), 2, QStringLiteral("boom"), {}, 1);
    const auto console = ask(control, QStringLiteral("agent"), QStringLiteral("console"),
        {{QStringLiteral("tab"), window}});
    QCOMPARE(console.value(QStringLiteral("messages")).toArray().size(), 1);

    // `close` closes it, and it is gone.
    QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("close"),
        {{QStringLiteral("tab"), window}})));
    QCOMPARE(closing.count(), 1);
    QCOMPARE(closing.at(0).at(0).toString(), window);
    QVERIFY(control.agentWindowIds().isEmpty());
    QCOMPARE(failure(askPage(control, QStringLiteral("agent"), QStringLiteral("look"),
                 {{QStringLiteral("tab"), window}})),
        QStringLiteral("not-found"));

    // Once no Agent holds the opener, its windows are the reader's again:
    // when the Agent closes the opener, and when it has left it alone.
    const auto orphaned = control.attachWindow(tab);
    QVERIFY(!orphaned.isEmpty());
    const auto other = openAgentTab(control, QStringLiteral("other"));
    QVERIFY(!control.attachWindow(other).isEmpty());
    QVERIFY(succeeded(ask(control, QStringLiteral("agent"), QStringLiteral("close"),
        {{QStringLiteral("tab"), tab}})));
    QVERIFY(!control.agentWindowIds().contains(orphaned));
    QCOMPARE(control.agentWindowIds().size(), 1);
    control.setAttachmentIdleMs(50);
    QTRY_VERIFY(control.agentWindowIds().isEmpty());
    control.setAttachmentIdleMs(AgentControl::defaultAttachmentIdleMs);

    // Allow agents off: the window is the reader's again.
    const auto opener = openAgentTab(control, QStringLiteral("agent"));
    const auto second = control.attachWindow(opener);
    QVERIFY(!second.isEmpty());
    control.setAllowAgents(false);
    QVERIFY(control.agentWindowIds().isEmpty());
    QVERIFY(control.agentWindow(second).isEmpty());
    control.windowClosed(second);
}

#include "tst_agentcontrol.moc"
