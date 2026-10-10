#include "AgentCommand.h"
#include "AgentProtocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// The `omaweb` command as a reader runs it: a process, with a browser beside
// it or none, and a socket the test answers on as the browser does.

namespace {

// Answers a hello with `hello` and every other request with `{"ok": true}`,
// keeping what it was asked.
class FakeBrowser final : public QObject {
    Q_OBJECT

public:
    explicit FakeBrowser(const QString &path)
    {
        connect(&m_server, &QLocalServer::newConnection, this, [this] {
            while (auto *socket = m_server.nextPendingConnection()) {
                connect(socket, &QLocalSocket::readyRead, this, [this, socket] { read(socket); });
            }
        });
        listening = m_server.listen(path);
    }

    bool listening = false;
    QJsonObject hello = omaweb::agentHelloAnswer(QStringLiteral(OMAWEB_VERSION));
    QList<QJsonObject> hellos;
    QList<QJsonObject> requests;

private:
    void read(QLocalSocket *socket)
    {
        while (socket->canReadLine()) {
            const auto request = QJsonDocument::fromJson(socket->readLine()).object();
            auto answer = QJsonObject {{QStringLiteral("ok"), true}};
            if (request.value(QStringLiteral("verb")).toString() == u"hello") {
                hellos.append(request);
                answer = hello;
            } else {
                requests.append(request);
            }
            socket->write(QJsonDocument(answer).toJson(QJsonDocument::Compact) + '\n');
            socket->flush();
        }
    }

    QLocalServer m_server;
};

struct Run {
    int code = -1;
    QString out;
    QString err;
};

// Runs `program` until it exits, letting this thread answer as the browser
// meanwhile.
Run run(const QString &program, const QStringList &arguments,
    const QProcessEnvironment &environment, const QByteArray &input = {})
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessEnvironment(environment);
    QSignalSpy finished(&process, &QProcess::finished);
    process.start();
    if (!process.waitForStarted()) {
        return {};
    }
    process.write(input);
    process.closeWriteChannel();
    if (!finished.wait(30000)) {
        process.kill();
        process.waitForFinished();
        return {};
    }
    return {process.exitCode(), QString::fromUtf8(process.readAllStandardOutput()),
        QString::fromUtf8(process.readAllStandardError())};
}

// A copy of the client alone in a directory of its own, so what sits beside it
// is the test's to say.
QString placeClient(const QTemporaryDir &directory)
{
    const auto client = directory.filePath(QStringLiteral("omaweb"));
    if (!QFile::copy(QStringLiteral(OMAWEB_CLIENT_PATH), client)) {
        return {};
    }
    return client;
}

// A browser that writes down the command line it was run with and exits 7, so
// an exit of 7 is the browser having taken over the process.
bool placeBrowser(const QTemporaryDir &directory)
{
    QFile browser(directory.filePath(QStringLiteral("omaweb-browser")));
    if (!browser.open(QIODevice::WriteOnly)) {
        return false;
    }
    browser.write("#!/bin/sh\nprintf '%s\\n' \"$0\" \"$@\" > \"$OMAWEB_TEST_RECORD\"\nexit 7\n");
    browser.close();
    return browser.setPermissions(browser.permissions() | QFileDevice::ExeOwner);
}

QProcessEnvironment environmentFor(const QTemporaryDir &directory)
{
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("OMAWEB_CONTROL_SOCKET"),
        directory.filePath(QStringLiteral("control.sock")));
    environment.insert(
        QStringLiteral("OMAWEB_TEST_RECORD"), directory.filePath(QStringLiteral("record")));
    return environment;
}

QStringList recorded(const QTemporaryDir &directory)
{
    QFile record(directory.filePath(QStringLiteral("record")));
    if (!record.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(record.readAll()).split(u'\n', Qt::SkipEmptyParts);
}

// Every binary that answers to a verb: the client, and the browser where this
// build makes one.
QStringList verbRunners()
{
    QStringList runners {QStringLiteral(OMAWEB_CLIENT_PATH)};
#ifdef OMAWEB_BROWSER_PATH
    runners.append(QStringLiteral(OMAWEB_BROWSER_PATH));
#endif
    return runners;
}

} // namespace

class AgentClientTest final : public QObject {
    Q_OBJECT

private slots:
    void handsALaunchToTheBrowserBesideIt_data();
    void handsALaunchToTheBrowserBesideIt();
    void refusesAnAgentOptionWithNoVerb_data();
    void refusesAnAgentOptionWithNoVerb();
    void saysTheBrowserIsNotInstalled_data();
    void saysTheBrowserIsNotInstalled();
    void answersItsOwnVersionWithoutABrowser();
    void readsTheSocketFromTheEnvironmentFirst();
    void sendsEveryVerbAsTheBrowserDoes_data();
    void sendsEveryVerbAsTheBrowserDoes();
    void saysWhichProtocolItSpeaks();
    void warnsOnceWhenTheBrowserSpeaksAnother_data();
    void warnsOnceWhenTheBrowserSpeaksAnother();
    void warnsOnceForAWholeMcpSession();
    void leavesAnMcpSessionItCannotStartABrowserFor();
    void saysWhereItLookedWhenNothingAnswers();
};

void AgentClientTest::handsALaunchToTheBrowserBesideIt_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::newRow("a launch") << QStringList {};
    QTest::newRow("an address from xdg-open")
        << QStringList {QStringLiteral("https://example.com/a b?c=d")};
    QTest::newRow("the browser's own switches")
        << QStringList {QStringLiteral("--remote-debugging=9222"), QStringLiteral("--version")};
    QTest::newRow("a QML validation") << QStringList {QStringLiteral("--validate-qml")};
}

void AgentClientTest::handsALaunchToTheBrowserBesideIt()
{
    QFETCH(QStringList, arguments);
    QTemporaryDir directory;
    const auto client = placeClient(directory);
    QVERIFY(!client.isEmpty());
    QVERIFY(placeBrowser(directory));

    const auto ran = run(client, arguments, environmentFor(directory));

    QCOMPARE(ran.code, 7);
    auto commandLine = recorded(directory);
    QVERIFY(!commandLine.isEmpty());
    QCOMPARE(QFileInfo(commandLine.takeFirst()).canonicalFilePath(),
        QFileInfo(directory.filePath(QStringLiteral("omaweb-browser"))).canonicalFilePath());
    QCOMPARE(commandLine, arguments);
}

void AgentClientTest::refusesAnAgentOptionWithNoVerb_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("option");
    QTest::newRow("json and an address")
        << QStringList {QStringLiteral("--json"), QStringLiteral("https://example.com")}
        << QStringLiteral("--json");
    QTest::newRow("a name") << QStringList {QStringLiteral("--name"), QStringLiteral("x")}
                            << QStringLiteral("--name");
    QTest::newRow("json alone") << QStringList {QStringLiteral("--json")}
                                << QStringLiteral("--json");
}

// The browser beside the client records what it is run with, so a launch would show: the command
// fails with the option named and the browser is never run (#683).
void AgentClientTest::refusesAnAgentOptionWithNoVerb()
{
    QFETCH(QStringList, arguments);
    QFETCH(QString, option);
    QTemporaryDir directory;
    const auto client = placeClient(directory);
    QVERIFY(!client.isEmpty());
    QVERIFY(placeBrowser(directory));

    const auto ran = run(client, arguments, environmentFor(directory));

    QCOMPARE(ran.code, 2);
    QVERIFY2(ran.err.contains(option), qPrintable(ran.err));
    QVERIFY2(ran.err.contains(QStringLiteral("Agent verb")), qPrintable(ran.err));
    QVERIFY(recorded(directory).isEmpty());
}

void AgentClientTest::saysTheBrowserIsNotInstalled_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::newRow("a launch") << QStringList {};
    QTest::newRow("an address") << QStringList {QStringLiteral("https://example.com/")};
}

// A sandbox has the client and no browser: what it can do is reach one.
void AgentClientTest::saysTheBrowserIsNotInstalled()
{
    QFETCH(QStringList, arguments);
    QTemporaryDir directory;
    const auto client = placeClient(directory);
    QVERIFY(!client.isEmpty());

    const auto ran = run(client, arguments, environmentFor(directory));

    QCOMPARE(ran.code, 1);
    QVERIFY2(ran.err.contains(QStringLiteral("not installed")), qPrintable(ran.err));
    QVERIFY2(ran.err.contains(QStringLiteral("only over its Agent socket")), qPrintable(ran.err));
    QVERIFY2(
        ran.err.contains(directory.filePath(QStringLiteral("control.sock"))), qPrintable(ran.err));
}

void AgentClientTest::answersItsOwnVersionWithoutABrowser()
{
    QTemporaryDir directory;
    const auto client = placeClient(directory);
    QVERIFY(!client.isEmpty());

    const auto ran = run(client, {QStringLiteral("--version")}, environmentFor(directory));

    QCOMPARE(ran.code, 0);
    QVERIFY2(ran.out.startsWith(QStringLiteral("Omaweb %1\n").arg(QStringLiteral(OMAWEB_VERSION))),
        qPrintable(ran.out));
}

// A sandbox has no login session, so no runtime directory of its own: the
// socket it was handed is named in the environment and has to be the one used.
void AgentClientTest::readsTheSocketFromTheEnvironmentFirst()
{
    QTemporaryDir directory;
    // Short, because a socket's path has a length limit and this one nests.
    QTemporaryDir session(QDir::tempPath() + QStringLiteral("/ow-XXXXXX"));
    QVERIFY(QDir(session.path()).mkdir(QStringLiteral("omaweb")));
    auto environment = environmentFor(directory);
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), session.path());
    environment.insert(QStringLiteral("TMPDIR"), session.path());
    FakeBrowser named(directory.filePath(QStringLiteral("control.sock")));
    FakeBrowser sessions(session.filePath(QStringLiteral("omaweb/control.sock")));
    QVERIFY(named.listening);
    QVERIFY(sessions.listening);

    const auto ran
        = run(QStringLiteral(OMAWEB_CLIENT_PATH), {QStringLiteral("spaces")}, environment);
    QCOMPARE(ran.code, 0);
    QCOMPARE(named.requests.size(), 1);
    QCOMPARE(sessions.requests.size(), 0);

    // And with nothing named, the session's is the one.
    environment.remove(QStringLiteral("OMAWEB_CONTROL_SOCKET"));
    QCOMPARE(
        run(QStringLiteral(OMAWEB_CLIENT_PATH), {QStringLiteral("spaces")}, environment).code, 0);
    QCOMPARE(sessions.requests.size(), 1);
}

void AgentClientTest::sendsEveryVerbAsTheBrowserDoes_data()
{
    QTest::addColumn<QStringList>("arguments");
    const auto row
        = [](const char *name, const QStringList &arguments) { QTest::newRow(name) << arguments; };
    row("spaces", {QStringLiteral("spaces")});
    row("tabs", {QStringLiteral("tabs"), QStringLiteral("--space"), QStringLiteral("Work")});
    row("tabs as json",
        {QStringLiteral("tabs"), QStringLiteral("--all"), QStringLiteral("--json")});
    row("open",
        {QStringLiteral("open"), QStringLiteral("--new"), QStringLiteral("https://example.com/")});
    row("close", {QStringLiteral("close"), QStringLiteral("--tab"), QStringLiteral("t1")});
    row("space new", {QStringLiteral("space"), QStringLiteral("new"), QStringLiteral("Checks")});
    row("space delete", {QStringLiteral("space"), QStringLiteral("delete"), QStringLiteral("s1")});
    row("space", {QStringLiteral("space"), QStringLiteral("work")});
    row("look", {QStringLiteral("look"), QStringLiteral("--all")});
    row("read", {QStringLiteral("read"), QStringLiteral("main")});
    row("do", {QStringLiteral("do"), QStringLiteral("click 3; press Enter")});
    row("shot", {QStringLiteral("shot"), QStringLiteral("--full")});
    row("eval", {QStringLiteral("eval"), QStringLiteral("document.title")});
    row("console", {QStringLiteral("console"), QStringLiteral("--level"), QStringLiteral("error")});
    row("commands", {QStringLiteral("commands"), QStringLiteral("--json")});
    row("run", {QStringLiteral("run"), QStringLiteral("toggle-sidebar")});
    row("focus",
        {QStringLiteral("focus"), QStringLiteral("--raise"), QStringLiteral("github.com")});
    row("dev", {QStringLiteral("dev"), QStringLiteral("localhost:5173")});
}

// The same request on the socket and the same answer printed, from the client
// and from the browser binary that answered verbs before there was a client.
void AgentClientTest::sendsEveryVerbAsTheBrowserDoes()
{
    QFETCH(QStringList, arguments);
    arguments << QStringLiteral("--name") << QStringLiteral("probe");
    const auto command
        = omaweb::readAgentCommand(QStringList {QStringLiteral("omaweb")} + arguments, {});
    QVERIFY2(command.error.isEmpty(), qPrintable(command.error));
    const QJsonObject answer {{QStringLiteral("ok"), true}};
    const auto printed = command.json
        ? QString::fromUtf8(QJsonDocument(answer).toJson(QJsonDocument::Compact)) + u'\n'
        : omaweb::formatAgentAnswer(
              command.request.value(QStringLiteral("verb")).toString(), answer);

    for (const auto &runner : verbRunners()) {
        QTemporaryDir directory;
        FakeBrowser browser(directory.filePath(QStringLiteral("control.sock")));
        QVERIFY(browser.listening);

        const auto ran = run(runner, arguments, environmentFor(directory));

        QVERIFY2(ran.code == 0, qPrintable(runner + u'\n' + ran.err));
        QCOMPARE(browser.requests, QList<QJsonObject> {command.request});
        QCOMPARE(ran.out, printed);
    }
}

void AgentClientTest::saysWhichProtocolItSpeaks()
{
    QTemporaryDir directory;
    FakeBrowser browser(directory.filePath(QStringLiteral("control.sock")));
    QVERIFY(browser.listening);

    const auto ran = run(
        QStringLiteral(OMAWEB_CLIENT_PATH), {QStringLiteral("spaces")}, environmentFor(directory));

    QCOMPARE(ran.code, 0);
    QCOMPARE(ran.err, QString());
    QCOMPARE(browser.hellos.size(), 1);
    QCOMPARE(browser.hellos.first().value(QStringLiteral("protocol")).toInt(),
        omaweb::agentProtocolVersion);
    QCOMPARE(browser.hellos.first().value(QStringLiteral("version")).toString(),
        QStringLiteral(OMAWEB_VERSION));
}

void AgentClientTest::warnsOnceWhenTheBrowserSpeaksAnother_data()
{
    QTest::addColumn<QJsonObject>("hello");
    QTest::addColumn<QString>("named");
    QTest::newRow("a newer browser") << QJsonObject {{QStringLiteral("ok"), true},
        {QStringLiteral("protocol"), omaweb::agentProtocolVersion + 1},
        {QStringLiteral("version"), QStringLiteral("99.1.0")}}
                                     << QStringLiteral("99.1.0");
    // What Omaweb 0.10.0 answers: it has no hello, so it names no version.
    QTest::newRow("a browser older than the protocol")
        << QJsonObject {{QStringLiteral("ok"), false},
               {QStringLiteral("code"), QStringLiteral("bad-request")},
               {QStringLiteral("error"), QStringLiteral("Omaweb has no verb \"hello\".")}}
        << QStringLiteral("older");
}

// The command still goes: most verbs read the same across protocols, and the
// browser refuses one it does not know.
void AgentClientTest::warnsOnceWhenTheBrowserSpeaksAnother()
{
    QFETCH(QJsonObject, hello);
    QFETCH(QString, named);
    QTemporaryDir directory;
    FakeBrowser browser(directory.filePath(QStringLiteral("control.sock")));
    QVERIFY(browser.listening);
    browser.hello = hello;

    const auto ran = run(
        QStringLiteral(OMAWEB_CLIENT_PATH), {QStringLiteral("spaces")}, environmentFor(directory));

    QCOMPARE(ran.code, 0);
    QCOMPARE(browser.requests.size(), 1);
    QCOMPARE(ran.err.count(u'\n'), 1);
    QVERIFY2(ran.err.contains(QStringLiteral(OMAWEB_VERSION)), qPrintable(ran.err));
    QVERIFY2(ran.err.contains(named), qPrintable(ran.err));
}

// An MCP server holds its connection for a whole session, and says so once.
void AgentClientTest::warnsOnceForAWholeMcpSession()
{
    QTemporaryDir directory;
    FakeBrowser browser(directory.filePath(QStringLiteral("control.sock")));
    QVERIFY(browser.listening);
    browser.hello = {{QStringLiteral("ok"), true},
        {QStringLiteral("protocol"), omaweb::agentProtocolVersion + 1},
        {QStringLiteral("version"), QStringLiteral("99.1.0")}};
    const auto call = [](int id) {
        return QJsonDocument(
                   QJsonObject {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                       {QStringLiteral("id"), id},
                       {QStringLiteral("method"), QStringLiteral("tools/call")},
                       {QStringLiteral("params"),
                           QJsonObject {{QStringLiteral("name"), QStringLiteral("spaces")},
                               {QStringLiteral("arguments"), QJsonObject {}}}}})
                   .toJson(QJsonDocument::Compact)
            + '\n';
    };

    const auto ran = run(QStringLiteral(OMAWEB_CLIENT_PATH), {QStringLiteral("mcp")},
        environmentFor(directory), call(1) + call(2));

    QCOMPARE(ran.code, 0);
    QCOMPARE(browser.requests.size(), 2);
    QCOMPARE(browser.hellos.size(), 1);
    QCOMPARE(ran.err.count(QStringLiteral("99.1.0")), 1);
}

// The MCP server starts the browser when none answers, and a sandbox has none
// to start: it says so to the Agent and to whoever reads its log, and goes.
void AgentClientTest::leavesAnMcpSessionItCannotStartABrowserFor()
{
    QTemporaryDir directory;
    const auto client = placeClient(directory);
    QVERIFY(!client.isEmpty());
    const auto message = [](int id, const QString &method, const QJsonObject &params) {
        return QJsonDocument(QJsonObject {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                 {QStringLiteral("id"), id}, {QStringLiteral("method"), method},
                                 {QStringLiteral("params"), params}})
                   .toJson(QJsonDocument::Compact)
            + '\n';
    };
    const QJsonObject spaces {{QStringLiteral("name"), QStringLiteral("spaces")},
        {QStringLiteral("arguments"), QJsonObject {}}};

    const auto ran = run(client, {QStringLiteral("mcp")}, environmentFor(directory),
        message(1, QStringLiteral("initialize"), {})
            + message(2, QStringLiteral("tools/call"), spaces)
            + message(3, QStringLiteral("tools/call"), spaces));

    QCOMPARE(ran.code, 1);
    const auto answers = ran.out.split(u'\n', Qt::SkipEmptyParts);
    QCOMPARE(answers.size(), 2);
    const auto call = QJsonDocument::fromJson(answers.at(1).toUtf8()).object();
    const auto result = call.value(QStringLiteral("result")).toObject();
    QVERIFY(result.value(QStringLiteral("isError")).toBool());
    const auto said = result.value(QStringLiteral("content"))
                          .toArray()
                          .first()
                          .toObject()
                          .value(QStringLiteral("text"))
                          .toString();
    QVERIFY2(said.contains(QStringLiteral("no browser installed")), qPrintable(said));
    QVERIFY2(said.contains(directory.filePath(QStringLiteral("control.sock"))), qPrintable(said));
    QVERIFY2(said.contains(QStringLiteral("Agent in a container")), qPrintable(said));
    QVERIFY2(ran.err.contains(said), qPrintable(ran.err));
}

// From a sandbox the likeliest reason is a socket nobody forwarded, so the
// failure says which path was tried, where that path came from, and what to
// forward, rather than that no browser runs.
void AgentClientTest::saysWhereItLookedWhenNothingAnswers()
{
    QTemporaryDir directory;
    auto environment = environmentFor(directory);

    const auto named
        = run(QStringLiteral(OMAWEB_CLIENT_PATH), {QStringLiteral("spaces")}, environment);
    QCOMPARE(named.code, 3);
    QVERIFY2(named.err.contains(directory.filePath(QStringLiteral("control.sock"))),
        qPrintable(named.err));
    QVERIFY2(
        named.err.contains(QStringLiteral("OMAWEB_CONTROL_SOCKET names")), qPrintable(named.err));
    QVERIFY2(named.err.contains(QStringLiteral("forwarded from the host")), qPrintable(named.err));
    QVERIFY2(named.err.contains(QStringLiteral("#agent-in-a-container")), qPrintable(named.err));

    QTemporaryDir session(QDir::tempPath() + QStringLiteral("/ow-XXXXXX"));
    environment.remove(QStringLiteral("OMAWEB_CONTROL_SOCKET"));
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), session.path());
    environment.insert(QStringLiteral("TMPDIR"), session.path());
    const auto fallen
        = run(QStringLiteral(OMAWEB_CLIENT_PATH), {QStringLiteral("spaces")}, environment);
    QCOMPARE(fallen.code, 3);
    QVERIFY2(fallen.err.contains(QStringLiteral("omaweb/control.sock")), qPrintable(fallen.err));
    QVERIFY2(fallen.err.contains(QStringLiteral("the default")), qPrintable(fallen.err));
    QVERIFY2(fallen.err.contains(QStringLiteral("OMAWEB_CONTROL_SOCKET")), qPrintable(fallen.err));
}

QTEST_GUILESS_MAIN(AgentClientTest)
#include "tst_agentclient.moc"
