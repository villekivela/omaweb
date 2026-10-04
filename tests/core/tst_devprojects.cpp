#include "AgentCommand.h"
#include "AgentControl.h"
#include "BrowserController.h"
#include "BrowserStateExchange.h"
#include "ControlSocket.h"
#include "PrivateSessionFixture.h"
#include "SessionFixture.h"
#include "SpaceListModel.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <memory>

using omaweb::AgentControl;
using omaweb::BrowserController;
using omaweb::BrowserStateExchangeAdapter;
using omaweb::ControlSocket;
using omaweb::SpaceListModel;
using omaweb::SpaceProject;
using omaweb::test::PrivateSessionFixture;
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

// `omaweb dev [address]` as the CLI sends it from `directory`.
QJsonObject dev(const QString &directory, const QString &address = {})
{
    QJsonObject request {{QStringLiteral("verb"), QStringLiteral("dev")},
        {QStringLiteral("name"), QStringLiteral("zsh")}, {QStringLiteral("directory"), directory}};
    if (!address.isEmpty()) {
        request.insert(QStringLiteral("address"), address);
    }
    return request;
}

QString spaceName(BrowserController &browser, const QString &spaceId)
{
    const auto *model = browser.spaces();
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto index = model->index(row, 0);
        if (index.data(SpaceListModel::IdRole).toString() == spaceId) {
            return index.data(SpaceListModel::NameRole).toString();
        }
    }
    return {};
}

// A program that writes down the folder it was started in and its arguments,
// one per NUL, so an argument holding a space or a quote reads back whole.
QString recordingProgram(const QString &path)
{
    QFile script(path);
    if (!script.open(QIODevice::WriteOnly)) {
        return {};
    }
    script.write(QStringLiteral("#!/bin/sh\n"
                                "pwd > '%1.cwd'\n"
                                "for argument in \"$@\"; do printf '%s\\0' \"$argument\"; done "
                                "> '%1.part'\n"
                                "mv '%1.part' '%1.args'\n")
            .arg(path)
            .toUtf8());
    script.close();
    script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return path;
}

// What a recording program was started with, and where, once it has run.
struct Recorded {
    QStringList arguments;
    QString directory;
};

Recorded recorded(const QString &program)
{
    QFile arguments(program + QStringLiteral(".args"));
    QFile directory(program + QStringLiteral(".cwd"));
    if (!arguments.open(QIODevice::ReadOnly) || !directory.open(QIODevice::ReadOnly)) {
        return {};
    }
    auto list = QString::fromUtf8(arguments.readAll()).split(QChar(u'\0'));
    list.removeLast();
    return {.arguments = list, .directory = QString::fromUtf8(directory.readAll()).trimmed()};
}

// What `:ask` hands the agent after its own arguments.
bool isPrompt(const QString &argument, const QString &words)
{
    return argument.startsWith(QStringLiteral("Use the omaweb skill")) && argument.endsWith(words);
}

// The `omaweb` command a shell runs, against a browser answering on a real
// socket. The command blocks on its answer, so it runs beside the loop the
// browser answers on.
int runCommandLine(const QStringList &arguments, const QString &path)
{
    auto status = -1;
    const std::unique_ptr<QThread> shell(QThread::create(
        [&status, &path, &arguments] { status = omaweb::runAgentCommand(arguments, path); }));
    shell->start();
    QDeadlineTimer deadline(10000);
    while (!shell->isFinished() && !deadline.hasExpired()) {
        QTest::qWait(5);
    }
    shell->wait();
    return status;
}

QString folder(const QTemporaryDir &root, const QString &path)
{
    const auto directory = root.filePath(path);
    QDir().mkpath(directory);
    return directory;
}

} // namespace

class DevProjectsTest final : public QObject {
    Q_OBJECT

private slots:
    void opensANewSpaceForAFolderWithItsAddress();
    void findsTheNearestProjectAbove();
    void refusesABareDevWithNoAddressToOpen();
    void selectsATabAlreadyOnTheAddress();
    void waitsForTheAddressToAnswerBeforeLoading();
    void grantsNothing();
    void neverGivesAPrivateWindowAProject();
    void keepsTheProjectOutOfSync();
    void forgetsTheProjectWithItsSpace();
    void asksTheAgentInTheProjectDirectory();
    void asksTheProjectsOwnAgent();
    void asksFromHomeWhenTheFolderIsNotHere();
    void answersTheCommandLine();
};

// The first `omaweb dev` in a folder makes it the project directory of a new
// Space named after it, and brings that Space forward as `focus --raise` does.
void DevProjectsTest::opensANewSpaceForAFolderWithItsAddress()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    QSignalSpy raised(&control, &AgentControl::windowRequested);

    const auto shop = folder(projects, QStringLiteral("shop"));
    const auto answer = control.answer(dev(shop, QStringLiteral("localhost:5173")));
    QVERIFY2(answer.value(QStringLiteral("ok")).toBool(),
        qPrintable(answer.value(QStringLiteral("error")).toString()));
    const auto spaceId = answer.value(QStringLiteral("space")).toString();
    QVERIFY(spaceId != u"personal");
    QCOMPARE(spaceName(*browser, spaceId), QStringLiteral("shop"));
    QCOMPARE(browser->activeSpaceId(), spaceId);
    QCOMPARE(raised.count(), 1);
    const auto project = browser->spaceProject(spaceId);
    QVERIFY(project);
    QCOMPARE(project->directory, shop);
    QCOMPARE(project->address, QStringLiteral("http://localhost:5173"));
    QVERIFY(project->agentCommand.isEmpty());

    // Another folder of the same name is another project, and its Space takes
    // a number.
    const auto otherShop = folder(projects, QStringLiteral("clients/shop"));
    const auto second = control.answer(dev(otherShop, QStringLiteral("localhost:3000")));
    QVERIFY2(second.value(QStringLiteral("ok")).toBool(),
        qPrintable(second.value(QStringLiteral("error")).toString()));
    const auto secondId = second.value(QStringLiteral("space")).toString();
    QVERIFY(secondId != spaceId);
    QCOMPARE(spaceName(*browser, secondId), QStringLiteral("shop 2"));
    QCOMPARE(browser->spaceProject(secondId)->directory, otherShop);

    // The project is kept with the session.
    const auto restarted = fixture.createController();
    QCOMPARE(restarted->spaceProject(spaceId)->address, QStringLiteral("http://localhost:5173"));
}

// A monorepo can hold a project at its root and one in an app below it; a
// folder anywhere further down opens the nearest.
void DevProjectsTest::findsTheNearestProjectAbove()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    const auto spaceOf = [&control](const QJsonObject &request) {
        const auto answer = control.answer(request);
        return answer.value(QStringLiteral("ok")).toBool()
            ? answer.value(QStringLiteral("space")).toString()
            : answer.value(QStringLiteral("error")).toString();
    };

    const auto root = folder(projects, QStringLiteral("repo"));
    const auto web = folder(projects, QStringLiteral("repo/apps/web"));
    const auto rootSpace = spaceOf(dev(root, QStringLiteral("localhost:9090")));
    const auto webSpace = spaceOf(dev(web, QStringLiteral("localhost:5173")));
    QVERIFY(rootSpace != webSpace);
    QCOMPARE(spaceName(*browser, webSpace), QStringLiteral("web"));

    QVERIFY(browser->switchSpace(QStringLiteral("personal")));
    QSignalSpy raised(&control, &AgentControl::windowRequested);
    const auto components = folder(projects, QStringLiteral("repo/apps/web/src/components"));
    const auto reopened = control.answer(dev(components));
    QCOMPARE(reopened.value(QStringLiteral("space")).toString(), webSpace);
    QCOMPARE(reopened.value(QStringLiteral("address")).toString(),
        QStringLiteral("http://localhost:5173"));
    QCOMPARE(browser->activeSpaceId(), webSpace);
    QCOMPARE(raised.count(), 1);

    QCOMPARE(spaceOf(dev(folder(projects, QStringLiteral("repo/docs")))), rootSpace);
    // A folder whose name starts with another's is not below it.
    QCOMPARE(spaceOf(dev(folder(projects, QStringLiteral("repo/apps/webby")))), rootSpace);

    // An address given again in the project's own folder replaces the one
    // remembered, in the same Space.
    QCOMPARE(spaceOf(dev(root, QStringLiteral("localhost:9191"))), rootSpace);
    QCOMPARE(browser->spaceProject(rootSpace)->address, QStringLiteral("http://localhost:9191"));
    QCOMPARE(browser->spaces()->rowCount(), 3);
}

// Omaweb never reads the project's files to guess an address.
void DevProjectsTest::refusesABareDevWithNoAddressToOpen()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    QSignalSpy raised(&control, &AgentControl::windowRequested);
    const auto shop = folder(projects, QStringLiteral("shop"));
    QFile package(QDir(shop).filePath(QStringLiteral("package.json")));
    QVERIFY(package.open(QIODevice::WriteOnly));
    package.write(R"({"scripts": {"dev": "vite --port 5173"}})");
    package.close();

    const auto answer = control.answer(dev(shop));
    QVERIFY(!answer.value(QStringLiteral("ok")).toBool());
    QCOMPARE(answer.value(QStringLiteral("code")).toString(), QStringLiteral("no-address"));
    QVERIFY2(answer.value(QStringLiteral("error")).toString().contains(u"address"),
        qPrintable(answer.value(QStringLiteral("error")).toString()));
    QCOMPARE(browser->spaces()->rowCount(), 1);
    QCOMPARE(browser->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(raised.count(), 0);

    // Words are not an address.
    const auto searched = control.answer(dev(shop, QStringLiteral("my shop")));
    QCOMPARE(searched.value(QStringLiteral("code")).toString(), QStringLiteral("bad-request"));
    QCOMPARE(browser->spaces()->rowCount(), 1);
}

// The app is already open: its tab is brought forward rather than another
// opened beside it, at whatever page the reader had reached.
void DevProjectsTest::selectsATabAlreadyOnTheAddress()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    const auto shop = folder(projects, QStringLiteral("shop"));
    const auto spaceId = control.answer(dev(shop, QStringLiteral("localhost:5173")))
                             .value(QStringLiteral("space"))
                             .toString();
    QVERIFY(!spaceId.isEmpty());
    const auto docs
        = browser->openTabInSpace(spaceId, QUrl(QStringLiteral("https://vite.dev/guide/")));
    const auto cart
        = browser->openTabInSpace(spaceId, QUrl(QStringLiteral("http://localhost:5173/cart")));
    browser->activateTab(docs);
    QVERIFY(browser->switchSpace(QStringLiteral("personal")));
    const auto before = browser->spaceTabs(spaceId).size();

    const auto answer = control.answer(dev(shop));
    QVERIFY2(answer.value(QStringLiteral("ok")).toBool(),
        qPrintable(answer.value(QStringLiteral("error")).toString()));
    QCOMPARE(browser->activeSpaceId(), spaceId);
    QCOMPARE(browser->activeTabId(), cart);
    QCOMPARE(answer.value(QStringLiteral("tab")).toString(), cart);
    QCOMPARE(browser->spaceTabs(spaceId).size(), before);
    QCOMPARE(browser->activeUrl(), QUrl(QStringLiteral("http://localhost:5173/cart")));
}

// The CLI returns at once, and Omaweb never starts the server. Until the
// address answers the Space shows and nothing loads; then the tab does.
void DevProjectsTest::waitsForTheAddressToAnswerBeforeLoading()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    browser->setAddressRetryMs(20);
    AgentControl control(browser.get(), config.path());
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const auto port = server.serverPort();
    server.close();
    const auto address = QStringLiteral("http://127.0.0.1:%1").arg(port);

    const auto answer = control.answer(
        dev(folder(projects, QStringLiteral("shop")), QStringLiteral("127.0.0.1:%1").arg(port)));
    QVERIFY2(answer.value(QStringLiteral("ok")).toBool(),
        qPrintable(answer.value(QStringLiteral("error")).toString()));
    const auto spaceId = answer.value(QStringLiteral("space")).toString();
    QCOMPARE(browser->activeSpaceId(), spaceId);
    QVERIFY(browser->activeSpaceAwaitsAddress());
    QTest::qWait(200);
    QVERIFY(browser->activeSpaceAwaitsAddress());
    QVERIFY(browser->activeTabBlank());
    for (const auto &tab : browser->spaceTabs(spaceId)) {
        QVERIFY(!tab.url.toString().startsWith(address));
    }

    QVERIFY(server.listen(QHostAddress::LocalHost, port));
    QTRY_VERIFY(!browser->activeSpaceAwaitsAddress());
    QCOMPARE(browser->activeUrl(), QUrl(address));
    // The Space's resting tab became the app's, rather than one beside it.
    QCOMPARE(browser->spaceTabs(spaceId).size(), 1);
}

// Any process can run `omaweb dev`, so it never lets an Agent into a Space:
// not the one it makes, and not one the reader granted before.
void DevProjectsTest::grantsNothing()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(SessionSpec {
        .spaces = {readersSession().spaces.constFirst(),
            SpaceSpec {
                .id = QStringLiteral("work"),
                .name = QStringLiteral("Work"),
                .tabs = {TabSpec {.id = QStringLiteral("work-tab"),
                    .url = QUrl(QStringLiteral("https://work.example/"))}},
            }},
        .activeSpaceId = QStringLiteral("personal"),
    });
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    const auto shop = folder(projects, QStringLiteral("shop"));
    QVERIFY(browser->setSpaceProject(QStringLiteral("work"),
        {.directory = shop,
            .address = QStringLiteral("http://localhost:5173"),
            .agentCommand = {}}));
    QVERIFY(browser->grantSpace(QStringLiteral("work")));
    QSignalSpy grantsChanged(browser.get(), &BrowserController::spaceGrantsChanged);

    const auto granted = control.answer(dev(shop));
    QCOMPARE(granted.value(QStringLiteral("space")).toString(), QStringLiteral("work"));
    const auto created = control.answer(
        dev(folder(projects, QStringLiteral("blog")), QStringLiteral("localhost:4321")));
    const auto blog = created.value(QStringLiteral("space")).toString();
    QVERIFY(!blog.isEmpty());

    QVERIFY(!browser->spaceGranted(blog));
    QCOMPARE(browser->grantedSpaceIds(), QStringList {QStringLiteral("work")});
    QCOMPARE(fixture.createController()->grantedSpaceIds(), QStringList {QStringLiteral("work")});
    QCOMPARE(grantsChanged.count(), 0);
    QVERIFY(control.grantRequest().isEmpty());
    // Nor does a tab it selects or opens become an Agent tab.
    QVERIFY(control.agentTabIds().isEmpty());
}

void DevProjectsTest::neverGivesAPrivateWindowAProject()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    PrivateSessionFixture privateSession(config.path());
    const auto privateWindow = privateSession.createController();
    const auto shop = folder(projects, QStringLiteral("shop"));
    const SpaceProject project {
        .directory = shop, .address = QStringLiteral("http://localhost:5173"), .agentCommand = {}};

    QVERIFY(privateWindow->createProjectSpace(project).isEmpty());
    QVERIFY(!privateWindow->setSpaceProject(privateWindow->activeSpaceId(), project));
    QVERIFY(!privateWindow->spaceProject(privateWindow->activeSpaceId()));
    QVERIFY(privateWindow->projectSpaceFor(shop).isEmpty());

    AgentControl privateControl(privateWindow.get(), config.path());
    QSignalSpy raised(&privateControl, &AgentControl::windowRequested);
    const auto answer = privateControl.answer(dev(shop, QStringLiteral("localhost:5173")));
    QCOMPARE(answer.value(QStringLiteral("code")).toString(), QStringLiteral("private"));
    QCOMPARE(raised.count(), 0);
    QVERIFY(!privateWindow->activeSpaceAwaitsAddress());
}

// A folder on this machine means nothing on another. The Space itself syncs
// as any Space does; where its project is, and the agent it runs, do not.
void DevProjectsTest::keepsTheProjectOutOfSync()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    const auto shop = folder(projects, QStringLiteral("shop"));
    const auto spaceId = browser->createProjectSpace({.directory = shop,
        .address = QStringLiteral("http://localhost:5173"),
        .agentCommand = QStringLiteral("incus exec dev --cwd {dir} -- claude")});
    QVERIFY(!spaceId.isEmpty());

    const BrowserStateExchangeAdapter exchange(
        browser.get(), nullptr, nullptr, fixture.dataRoot(), config.path());
    const auto image = exchange.capture({});
    QStringList captured;
    for (const auto &space : image.spaces) {
        captured << space.id << space.name << space.color;
    }
    for (const auto &tabs : image.tabsBySpace) {
        for (const auto &tab : tabs) {
            captured << tab.url.toString() << tab.title;
        }
    }
    for (auto it = image.preferences.cbegin(); it != image.preferences.cend(); ++it) {
        captured << it.key() << it.value();
    }
    QVERIFY(captured.contains(spaceId));
    QVERIFY(captured.contains(QStringLiteral("shop")));
    for (const auto &kept : {shop, QStringLiteral("localhost:5173"), QStringLiteral("{dir}")}) {
        for (const auto &field : std::as_const(captured)) {
            QVERIFY2(!field.contains(kept), qPrintable(field));
        }
    }
}

// `:ask` in a project's Space starts the agent in the project directory, so an
// agent on this machine reads that project's CLAUDE.md and every one above it.
// In any other Space it starts as it always has.
void DevProjectsTest::asksTheAgentInTheProjectDirectory()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    const auto terminal = recordingProgram(projects.filePath(QStringLiteral("terminal")));
    control.setTerminalProgram(terminal);
    const auto agent = recordingProgram(projects.filePath(QStringLiteral("agent")));
    control.setAgentCommand(QStringLiteral("\"%1\" --cwd {dir}").arg(agent));
    const auto shop = QDir(folder(projects, QStringLiteral("my shop"))).canonicalPath();
    const auto spaceId = browser->createProjectSpace({.directory = shop,
        .address = QStringLiteral("http://localhost:5173"),
        .agentCommand = {}});
    const auto tabId
        = browser->openTabInSpace(spaceId, QUrl(QStringLiteral("http://localhost:5173/")));

    QVERIFY(control.askAgent(tabId, QStringLiteral("why does this fail"))
            .value(QStringLiteral("ok"))
            .toBool());
    QTRY_VERIFY(!recorded(terminal).arguments.isEmpty());
    auto started = recorded(terminal);
    QCOMPARE(started.directory, shop);
    QCOMPARE(started.arguments.size(), 5);
    QCOMPARE(started.arguments.mid(0, 4),
        QStringList({QStringLiteral("--dir=") + shop, agent, QStringLiteral("--cwd"), shop}));
    QVERIFY(isPrompt(started.arguments.constLast(), QStringLiteral("why does this fail")));

    // Any other Space is as before: from home, and `{dir}` is no one's.
    QVERIFY(QFile::remove(terminal + QStringLiteral(".args")));
    QVERIFY(control.askAgent(QStringLiteral("personal-tab"), QStringLiteral("summarize"))
            .value(QStringLiteral("ok"))
            .toBool());
    QTRY_VERIFY(!recorded(terminal).arguments.isEmpty());
    started = recorded(terminal);
    QCOMPARE(started.directory, QDir(QDir::homePath()).canonicalPath());
    QCOMPARE(started.arguments.mid(0, 3),
        QStringList({agent, QStringLiteral("--cwd"), QStringLiteral("{dir}")}));
}

// `--agent` records a command for the project, which `:ask` runs there in
// place of the global one. `{dir}` is replaced after the command is split,
// inside each argument, so a path with a space stays one argument, and no
// shell ever reads it.
void DevProjectsTest::asksTheProjectsOwnAgent()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    const auto terminal = recordingProgram(projects.filePath(QStringLiteral("terminal")));
    control.setTerminalProgram(terminal);
    const auto global = recordingProgram(projects.filePath(QStringLiteral("claude")));
    control.setAgentCommand(global);
    const auto ssh = recordingProgram(projects.filePath(QStringLiteral("ssh")));
    // A space and quotes in the path: replaced before the split, they would
    // be read as the command's own.
    const auto shop = QDir(folder(projects, QStringLiteral("my \"shop\""))).canonicalPath();

    auto request = dev(shop, QStringLiteral("localhost:5173"));
    request.insert(
        QStringLiteral("agent"), QStringLiteral("%1 -t h \"cd {dir} && claude\"").arg(ssh));
    const auto spaceId = control.answer(request).value(QStringLiteral("space")).toString();
    QVERIFY(!spaceId.isEmpty());
    QCOMPARE(browser->spaceProject(spaceId)->agentCommand,
        QStringLiteral("%1 -t h \"cd {dir} && claude\"").arg(ssh));
    const auto tabId
        = browser->openTabInSpace(spaceId, QUrl(QStringLiteral("http://localhost:5173/")));

    QVERIFY(
        control.askAgent(tabId, QStringLiteral("$(whoami)")).value(QStringLiteral("ok")).toBool());
    QTRY_VERIFY(!recorded(terminal).arguments.isEmpty());
    const auto started = recorded(terminal).arguments;
    QCOMPARE(started.size(), 6);
    QCOMPARE(started.mid(0, 5),
        QStringList({QStringLiteral("--dir=") + shop, ssh, QStringLiteral("-t"),
            QStringLiteral("h"), QStringLiteral("cd ") + shop + QStringLiteral(" && claude")}));
    QVERIFY(isPrompt(started.constLast(), QStringLiteral("$(whoami)")));

    // A bare `omaweb dev` keeps the command; Forget project clears it with the
    // rest, and the global one is back.
    QCOMPARE(control.answer(dev(shop)).value(QStringLiteral("space")).toString(), spaceId);
    QVERIFY(!browser->spaceProject(spaceId)->agentCommand.isEmpty());
    QVERIFY(browser->forgetSpaceProject(spaceId));
    QVERIFY(!browser->spaceProject(spaceId));
    QVERIFY(!fixture.createController()->spaceProject(spaceId));
    QVERIFY(QFile::remove(terminal + QStringLiteral(".args")));
    QVERIFY(control.askAgent(tabId, QStringLiteral("again")).value(QStringLiteral("ok")).toBool());
    QTRY_VERIFY(!recorded(terminal).arguments.isEmpty());
    QCOMPARE(recorded(terminal).arguments.constFirst(), global);
}

// A project recorded from inside a container, without the same path mounted
// here, has no folder on this machine to open a terminal in. The agent's
// command still gets the path it was recorded with.
void DevProjectsTest::asksFromHomeWhenTheFolderIsNotHere()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    const auto terminal = recordingProgram(projects.filePath(QStringLiteral("terminal")));
    control.setTerminalProgram(terminal);
    const auto incus = recordingProgram(projects.filePath(QStringLiteral("incus")));
    const auto inContainer = QStringLiteral("/workspace/omaweb-test-not-here/shop");
    const auto spaceId = browser->createProjectSpace({.directory = inContainer,
        .address = QStringLiteral("http://localhost:5173"),
        .agentCommand = incus + QStringLiteral(" exec dev --cwd {dir} -- claude")});
    const auto tabId
        = browser->openTabInSpace(spaceId, QUrl(QStringLiteral("http://localhost:5173/")));

    QVERIFY(control.askAgent(tabId, QStringLiteral("hi")).value(QStringLiteral("ok")).toBool());
    QTRY_VERIFY(!recorded(terminal).arguments.isEmpty());
    const auto started = recorded(terminal);
    QCOMPARE(started.directory, QDir(QDir::homePath()).canonicalPath());
    QCOMPARE(started.arguments.mid(0, 6),
        QStringList({incus, QStringLiteral("exec"), QStringLiteral("dev"), QStringLiteral("--cwd"),
            inContainer, QStringLiteral("--")}));
}

// `omaweb dev` from a shell: it returns once the Space is on show, and a bare
// one with no address to open fails.
void DevProjectsTest::answersTheCommandLine()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    QTemporaryDir runtime(QDir::tempPath() + QStringLiteral("/omaweb-XXXXXX"));
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    ControlSocket socket(&control);
    const auto path = runtime.filePath(QStringLiteral("c.sock"));
    QVERIFY(socket.listen(path));
    const auto shop = QDir(folder(projects, QStringLiteral("shop"))).canonicalPath();
    const auto source = QDir(folder(projects, QStringLiteral("shop/src"))).canonicalPath();
    const auto previous = QDir::currentPath();
    const auto program = QStringLiteral("omaweb");

    QVERIFY(QDir::setCurrent(source));
    QCOMPARE(runCommandLine({program, QStringLiteral("dev")}, path), 1);
    QCOMPARE(browser->spaces()->rowCount(), 1);
    QVERIFY(QDir::setCurrent(shop));
    QCOMPARE(
        runCommandLine({program, QStringLiteral("dev"), QStringLiteral("localhost:5173")}, path),
        0);
    const auto spaceId = browser->projectSpaceFor(shop);
    QVERIFY(!spaceId.isEmpty());
    QCOMPARE(browser->activeSpaceId(), spaceId);
    QVERIFY(browser->switchSpace(QStringLiteral("personal")));
    QVERIFY(QDir::setCurrent(source));
    QCOMPARE(runCommandLine({program, QStringLiteral("dev")}, path), 0);
    QCOMPARE(browser->activeSpaceId(), spaceId);
    QVERIFY(QDir::setCurrent(previous));
}

// Deleting the Space takes its project, and the folder makes a new one the
// next time, rather than finding a Space that is gone.
void DevProjectsTest::forgetsTheProjectWithItsSpace()
{
    QTemporaryDir config;
    QTemporaryDir projects;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    const auto shop = folder(projects, QStringLiteral("shop"));
    const auto spaceId = control.answer(dev(shop, QStringLiteral("localhost:5173")))
                             .value(QStringLiteral("space"))
                             .toString();
    QVERIFY(browser->awaitsAddress(spaceId));

    QVERIFY(browser->deleteSpace(spaceId, QStringLiteral("shop")));
    QVERIFY(!browser->spaceProject(spaceId));
    QVERIFY(!browser->awaitsAddress(spaceId));
    QVERIFY(browser->projectSpaceFor(shop).isEmpty());
    QVERIFY(fixture.createController()->projectSpaceFor(shop).isEmpty());
    QCOMPARE(control.answer(dev(shop)).value(QStringLiteral("code")).toString(),
        QStringLiteral("no-address"));
}

QTEST_GUILESS_MAIN(DevProjectsTest)

#include "tst_devprojects.moc"
