#include "AgentCommand.h"
#include "AgentMcp.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <memory>
#include <sstream>

using omaweb::AgentMcpLink;
using omaweb::AgentMcpSend;
using omaweb::agentMcpTools;
using omaweb::answerAgentMcp;
using omaweb::readAgentCommand;
using omaweb::readAgentMcpCall;
using omaweb::readAgentMcpName;
using omaweb::serveAgentMcp;

namespace {

QStringList toolNames()
{
    QStringList names;
    for (const auto &tool : agentMcpTools()) {
        names.append(tool.toObject().value(QStringLiteral("name")).toString());
    }
    return names;
}

// What the browser does on the socket: answers each request with its verb and
// how many it has answered, and can hold the first answer back until the
// second request comes, as a browser does with a page that is slow to settle.
class FakeBrowser final : public QObject {
    Q_OBJECT

public:
    explicit FakeBrowser(QString path)
        : m_path(std::move(path))
    {
        connect(&m_server, &QLocalServer::newConnection, this, &FakeBrowser::accept);
    }

    Q_INVOKABLE bool listen() { return m_server.listen(m_path); }

    Q_INVOKABLE void quit()
    {
        for (auto *socket : m_server.findChildren<QLocalSocket *>()) {
            socket->abort();
        }
        m_server.close();
    }

    int connections = 0;
    bool holdFirst = false;
    QList<QJsonObject> requests;

private:
    void accept()
    {
        while (auto *socket = m_server.nextPendingConnection()) {
            ++connections;
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] { read(socket); });
        }
    }

    void read(QLocalSocket *socket)
    {
        while (socket->canReadLine()) {
            requests.append(QJsonDocument::fromJson(socket->readLine()).object());
            if (holdFirst && requests.size() == 1) {
                continue;
            }
            for (auto index = m_answered; index < requests.size(); ++index) {
                const QJsonObject answer {{QStringLiteral("ok"), true},
                    {QStringLiteral("verb"), requests.at(index).value(QStringLiteral("verb"))},
                    {QStringLiteral("answer"), index + 1}};
                socket->write(QJsonDocument(answer).toJson(QJsonDocument::Compact) + '\n');
            }
            m_answered = requests.size();
            socket->flush();
        }
    }

    QString m_path;
    QLocalServer m_server;
    qsizetype m_answered = 0;
};

// The server blocks on its socket as it does in `omaweb mcp`, so it runs on a
// thread of its own while this one answers as the browser.
void runBesideTheBrowser(const std::function<void()> &client)
{
    const std::unique_ptr<QThread> thread(QThread::create(client));
    thread->start();
    QDeadlineTimer deadline(60000);
    while (!thread->isFinished() && !deadline.hasExpired()) {
        QTest::qWait(5);
    }
    thread->wait();
}

QJsonObject spaces()
{
    return {{QStringLiteral("verb"), QStringLiteral("spaces")},
        {QStringLiteral("name"), QStringLiteral("claude")}};
}

QJsonObject message(const QString &method, const QJsonObject &params = {})
{
    return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), 7},
        {QStringLiteral("method"), method}, {QStringLiteral("params"), params}};
}

QJsonObject call(const QString &tool, const QJsonObject &arguments)
{
    return message(QStringLiteral("tools/call"),
        {{QStringLiteral("name"), tool}, {QStringLiteral("arguments"), arguments}});
}

QString textOf(const QJsonObject &reply)
{
    return reply.value(QStringLiteral("result"))
        .toObject()
        .value(QStringLiteral("content"))
        .toArray()
        .first()
        .toObject()
        .value(QStringLiteral("text"))
        .toString();
}

bool isError(const QJsonObject &reply)
{
    return reply.value(QStringLiteral("result"))
        .toObject()
        .value(QStringLiteral("isError"))
        .toBool();
}

} // namespace

class AgentMcpTest final : public QObject {
    Q_OBJECT

private slots:
    void servesEachVerbAsOneTool();
    void keepsTheToolListShort();
    void readsEachToolIntoARequest_data();
    void readsEachToolIntoARequest();
    void refusesAMalformedCall_data();
    void refusesAMalformedCall();
    void answersTheHandshake();
    void answersWhatTheBrowserAnswered();
    void saysWhyTheBrowserCouldNotBeReached();
    void servesOneMessageALine();
    void readsTheConnectionsName_data();
    void readsTheConnectionsName();
    void holdsOneConnectionToTheBrowser();
    void startsABrowserWhenNoneAnswers();
    void startsABrowserThatNeverAnswersOnlyOnce();
    void setsAsideAnAnswerThatCameTooLate();
    void theSkillTeachesEveryVerb();
};

void AgentMcpTest::servesEachVerbAsOneTool()
{
    QStringList names;
    for (const auto &tool : agentMcpTools()) {
        const auto object = tool.toObject();
        names.append(object.value(QStringLiteral("name")).toString());
        QVERIFY(!object.value(QStringLiteral("description")).toString().isEmpty());
        QCOMPARE(object.value(QStringLiteral("inputSchema"))
                     .toObject()
                     .value(QStringLiteral("type"))
                     .toString(),
            QStringLiteral("object"));
    }
    QCOMPARE(names,
        (QStringList {QStringLiteral("spaces"), QStringLiteral("tabs"), QStringLiteral("open"),
            QStringLiteral("close"), QStringLiteral("space"), QStringLiteral("focus"),
            QStringLiteral("commands"), QStringLiteral("run"), QStringLiteral("space_new"),
            QStringLiteral("space_delete"), QStringLiteral("look"), QStringLiteral("read"),
            QStringLiteral("do"), QStringLiteral("shot"), QStringLiteral("eval"),
            QStringLiteral("console")}));
}

// Every conversation the server is registered in pays for the list, and #383
// keeps it under 2,000 tokens. Claude Code counted 1,077 for 2,646 bytes of
// it, about 2.5 bytes a token, so a list under this many bytes stays inside
// the budget.
void AgentMcpTest::keepsTheToolListShort()
{
    const auto bytes = QJsonDocument(agentMcpTools()).toJson(QJsonDocument::Compact).size();
    QVERIFY2(bytes < 4500, qPrintable(QStringLiteral("%1 bytes").arg(bytes)));
}

// Each row is a tool call and the command line that asks the same, since the
// server and the CLI send one request for one verb. Every tool has a row.
void AgentMcpTest::readsEachToolIntoARequest_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QJsonObject>("arguments");
    QTest::addColumn<QJsonObject>("request");
    QTest::addColumn<QStringList>("command");

    const auto base = [](const QString &verb, QJsonObject fields = {}) {
        fields.insert(QStringLiteral("verb"), verb);
        fields.insert(QStringLiteral("name"), QStringLiteral("claude"));
        return fields;
    };
    QSet<QString> covered;
    const auto row = [&covered](const char *tag, const QString &tool) -> QTestData & {
        covered.insert(tool);
        return QTest::newRow(tag) << tool;
    };
    row("spaces", QStringLiteral("spaces")) << QJsonObject {} << base(QStringLiteral("spaces"))
                                            << QStringList {QStringLiteral("spaces")};
    row("tabs of a space", QStringLiteral("tabs"))
        << QJsonObject {{QStringLiteral("space"), QStringLiteral("Work")}}
        << base(QStringLiteral("tabs"), {{QStringLiteral("space"), QStringLiteral("Work")}})
        << QStringList {QStringLiteral("tabs"), QStringLiteral("--space"), QStringLiteral("Work")};
    row("open a new tab in a space", QStringLiteral("open"))
        << QJsonObject {{QStringLiteral("url"), QStringLiteral("http://localhost:3000/")},
               {QStringLiteral("space"), QStringLiteral("Checks")}, {QStringLiteral("new"), true}}
        << base(QStringLiteral("open"),
               {{QStringLiteral("url"), QStringLiteral("http://localhost:3000/")},
                   {QStringLiteral("space"), QStringLiteral("Checks")},
                   {QStringLiteral("new"), true}})
        << QStringList {QStringLiteral("open"), QStringLiteral("http://localhost:3000/"),
               QStringLiteral("--space"), QStringLiteral("Checks"), QStringLiteral("--new")};
    row("open in a tab", QStringLiteral("open"))
        << QJsonObject {{QStringLiteral("url"), QStringLiteral("http://localhost:3000/")},
               {QStringLiteral("tab"), QStringLiteral("t1")}}
        << base(QStringLiteral("open"),
               {{QStringLiteral("url"), QStringLiteral("http://localhost:3000/")},
                   {QStringLiteral("tab"), QStringLiteral("t1")}})
        << QStringList {QStringLiteral("open"), QStringLiteral("http://localhost:3000/"),
               QStringLiteral("--tab"), QStringLiteral("t1")};
    row("close a tab", QStringLiteral("close"))
        << QJsonObject {{QStringLiteral("tab"), QStringLiteral("t1")}}
        << base(QStringLiteral("close"), {{QStringLiteral("tab"), QStringLiteral("t1")}})
        << QStringList {QStringLiteral("close"), QStringLiteral("--tab"), QStringLiteral("t1")};
    row("switch space", QStringLiteral("space"))
        << QJsonObject {{QStringLiteral("space"), QStringLiteral("Work")}}
        << base(QStringLiteral("space"), {{QStringLiteral("space"), QStringLiteral("Work")}})
        << QStringList {QStringLiteral("space"), QStringLiteral("Work")};
    row("focus names its tab as the target", QStringLiteral("focus"))
        << QJsonObject {{QStringLiteral("tab"), QStringLiteral("github")}}
        << base(QStringLiteral("focus"), {{QStringLiteral("target"), QStringLiteral("github")}})
        << QStringList {QStringLiteral("focus"), QStringLiteral("github")};
    row("commands", QStringLiteral("commands"))
        << QJsonObject {} << base(QStringLiteral("commands"))
        << QStringList {QStringLiteral("commands")};
    row("run a command at a position", QStringLiteral("run"))
        << QJsonObject {{QStringLiteral("command"), QStringLiteral("select-space")},
               {QStringLiteral("position"), 2}}
        << base(QStringLiteral("run"),
               {{QStringLiteral("command"), QStringLiteral("select-space")},
                   {QStringLiteral("argument"), 2}})
        << QStringList {QStringLiteral("run"), QStringLiteral("select-space"), QStringLiteral("2")};
    row("space new names the space", QStringLiteral("space_new"))
        << QJsonObject {{QStringLiteral("name"), QStringLiteral("Checks")},
               {QStringLiteral("temporary"), true}}
        << base(QStringLiteral("space new"),
               {{QStringLiteral("space"), QStringLiteral("Checks")},
                   {QStringLiteral("temporary"), true}})
        << QStringList {QStringLiteral("space"), QStringLiteral("new"), QStringLiteral("Checks"),
               QStringLiteral("--temporary")};
    row("space delete", QStringLiteral("space_delete"))
        << QJsonObject {{QStringLiteral("space"), QStringLiteral("Checks")}}
        << base(QStringLiteral("space delete"),
               {{QStringLiteral("space"), QStringLiteral("Checks")}})
        << QStringList {
               QStringLiteral("space"), QStringLiteral("delete"), QStringLiteral("Checks")};
    row("look at everything", QStringLiteral("look"))
        << QJsonObject {{QStringLiteral("all"), true}}
        << base(QStringLiteral("look"), {{QStringLiteral("all"), true}})
        << QStringList {QStringLiteral("look"), QStringLiteral("--all")};
    row("a flag left false is left out", QStringLiteral("look"))
        << QJsonObject {{QStringLiteral("all"), false}} << base(QStringLiteral("look"))
        << QStringList {QStringLiteral("look")};
    row("read what a selector matches", QStringLiteral("read"))
        << QJsonObject {{QStringLiteral("selector"), QStringLiteral("main")},
               {QStringLiteral("tab"), QStringLiteral("t1")}}
        << base(QStringLiteral("read"),
               {{QStringLiteral("selector"), QStringLiteral("main")},
                   {QStringLiteral("tab"), QStringLiteral("t1")}})
        << QStringList {QStringLiteral("read"), QStringLiteral("main"), QStringLiteral("--tab"),
               QStringLiteral("t1")};
    row("do reads its steps as the CLI does", QStringLiteral("do"))
        << QJsonObject {{QStringLiteral("steps"),
                            QJsonArray {QStringLiteral("fill 1 \"A Reader\""),
                                QStringLiteral("click 2; wait text Thanks")}},
               {QStringLiteral("settle"), 500}, {QStringLiteral("timeout"), 9000}}
        << base(QStringLiteral("do"),
               {{QStringLiteral("steps"),
                    QJsonArray {QJsonObject {{QStringLiteral("action"), QStringLiteral("fill")},
                                    {QStringLiteral("target"), QStringLiteral("1")},
                                    {QStringLiteral("text"), QStringLiteral("A Reader")}},
                        QJsonObject {{QStringLiteral("action"), QStringLiteral("click")},
                            {QStringLiteral("target"), QStringLiteral("2")}},
                        QJsonObject {{QStringLiteral("action"), QStringLiteral("wait")},
                            {QStringLiteral("text"), QStringLiteral("Thanks")}}}},
                   {QStringLiteral("settle"), 500}, {QStringLiteral("timeout"), 9000}})
        << QStringList {QStringLiteral("do"), QStringLiteral("fill 1 \"A Reader\""),
               QStringLiteral("click 2; wait text Thanks"), QStringLiteral("--settle"),
               QStringLiteral("500"), QStringLiteral("--timeout"), QStringLiteral("9000")};
    row("shot the whole page to a file", QStringLiteral("shot"))
        << QJsonObject {{QStringLiteral("full"), true},
               {QStringLiteral("output"), QStringLiteral("signup.png")}}
        << base(QStringLiteral("shot"),
               {{QStringLiteral("full"), true},
                   {QStringLiteral("output"), QStringLiteral("signup.png")}})
        << QStringList {QStringLiteral("shot"), QStringLiteral("--full"),
               QStringLiteral("--output"), QStringLiteral("signup.png")};
    row("eval", QStringLiteral("eval"))
        << QJsonObject {{QStringLiteral("expression"), QStringLiteral("--x")},
               {QStringLiteral("tab"), QStringLiteral("t1")}}
        << base(QStringLiteral("eval"),
               {{QStringLiteral("expression"), QStringLiteral("--x")},
                   {QStringLiteral("tab"), QStringLiteral("t1")}})
        << QStringList {QStringLiteral("eval"), QStringLiteral("--tab"), QStringLiteral("t1"),
               QStringLiteral("--"), QStringLiteral("--x")};
    row("console at every level", QStringLiteral("console"))
        << QJsonObject {{QStringLiteral("level"), QStringLiteral("all")}}
        << base(QStringLiteral("console"), {{QStringLiteral("level"), QStringLiteral("all")}})
        << QStringList {
               QStringLiteral("console"), QStringLiteral("--level"), QStringLiteral("all")};
    row("console since a cursor", QStringLiteral("console"))
        << QJsonObject {{QStringLiteral("level"), QStringLiteral("error")},
               {QStringLiteral("since"), 12}}
        << base(QStringLiteral("console"),
               {{QStringLiteral("level"), QStringLiteral("error")}, {QStringLiteral("since"), 12}})
        << QStringList {QStringLiteral("console"), QStringLiteral("--level"),
               QStringLiteral("error"), QStringLiteral("--since"), QStringLiteral("12")};

    const auto tools = toolNames();
    QCOMPARE(covered, QSet<QString>(tools.cbegin(), tools.cend()));
}

void AgentMcpTest::readsEachToolIntoARequest()
{
    QFETCH(QString, tool);
    QFETCH(QJsonObject, arguments);
    QFETCH(QJsonObject, request);
    QFETCH(QStringList, command);

    const auto read = readAgentMcpCall(tool, arguments, QStringLiteral("claude"));
    QCOMPARE(read.error, QString());
    QVERIFY(!read.unknown);
    QCOMPARE(read.request, request);

    const auto fromTheShell = readAgentCommand(
        QStringList {QStringLiteral("omaweb")} + command, QStringLiteral("claude"));
    QCOMPARE(fromTheShell.error, QString());
    QCOMPARE(fromTheShell.request, request);
}

void AgentMcpTest::refusesAMalformedCall_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QJsonObject>("arguments");
    QTest::addColumn<QString>("error");

    QTest::newRow("an argument the tool does not take")
        << QStringLiteral("spaces") << QJsonObject {{QStringLiteral("tab"), QStringLiteral("t1")}}
        << QStringLiteral("spaces takes no argument tab.");
    QTest::newRow("a required argument missing")
        << QStringLiteral("open") << QJsonObject {} << QStringLiteral("open needs url.");
    QTest::newRow("run without a command")
        << QStringLiteral("run") << QJsonObject {} << QStringLiteral("run needs command.");
    QTest::newRow("a string that is not one")
        << QStringLiteral("open") << QJsonObject {{QStringLiteral("url"), 3}}
        << QStringLiteral("url is a string.");
    QTest::newRow("a flag that is not one")
        << QStringLiteral("look") << QJsonObject {{QStringLiteral("all"), QStringLiteral("yes")}}
        << QStringLiteral("all is true or false.");
    QTest::newRow("a fraction of a millisecond")
        << QStringLiteral("do")
        << QJsonObject {{QStringLiteral("steps"), QJsonArray {QStringLiteral("back")}},
               {QStringLiteral("timeout"), 1.5}}
        << QStringLiteral("timeout is a whole number.");
    QTest::newRow("steps that are not a list")
        << QStringLiteral("do") << QJsonObject {{QStringLiteral("steps"), QStringLiteral("back")}}
        << QStringLiteral("do takes at least one step.");
    QTest::newRow("no steps") << QStringLiteral("do")
                              << QJsonObject {{QStringLiteral("steps"), QJsonArray {}}}
                              << QStringLiteral("do takes at least one step.");
    QTest::newRow("a step there is not")
        << QStringLiteral("do")
        << QJsonObject {{QStringLiteral("steps"), QJsonArray {QStringLiteral("hover 3")}}}
        << QStringLiteral("There is no step \"hover\".");
    QTest::newRow("a level there is not")
        << QStringLiteral("console")
        << QJsonObject {{QStringLiteral("level"), QStringLiteral("info")}}
        << QStringLiteral("level is error, warning or all.");
}

void AgentMcpTest::refusesAMalformedCall()
{
    QFETCH(QString, tool);
    QFETCH(QJsonObject, arguments);
    QFETCH(QString, error);

    bool sent = false;
    const AgentMcpSend send = [&](const QJsonObject &, QString &) -> std::optional<QJsonObject> {
        sent = true;
        return QJsonObject {};
    };
    const auto reply = answerAgentMcp(call(tool, arguments), QStringLiteral("claude"), send);
    QVERIFY(reply);
    QVERIFY(isError(*reply));
    QCOMPARE(textOf(*reply), error);
    QVERIFY(!sent);
}

void AgentMcpTest::answersTheHandshake()
{
    const AgentMcpSend send
        = [](const QJsonObject &, QString &) -> std::optional<QJsonObject> { return std::nullopt; };
    const auto name = QStringLiteral("claude");
    const auto initialize = [&](const QString &version) {
        return answerAgentMcp(
            message(QStringLiteral("initialize"), {{QStringLiteral("protocolVersion"), version}}),
            name, send)
            ->value(QStringLiteral("result"))
            .toObject();
    };
    const auto known = initialize(QStringLiteral("2025-03-26"));
    QCOMPARE(
        known.value(QStringLiteral("protocolVersion")).toString(), QStringLiteral("2025-03-26"));
    QVERIFY(
        known.value(QStringLiteral("capabilities")).toObject().contains(QStringLiteral("tools")));
    QVERIFY(!known.value(QStringLiteral("instructions")).toString().isEmpty());
    // A version it does not know is answered with the newest it does.
    QCOMPARE(initialize(QStringLiteral("2099-01-01"))
                 .value(QStringLiteral("protocolVersion"))
                 .toString(),
        QStringLiteral("2025-11-25"));

    QVERIFY(!answerAgentMcp(
        {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("method"), QStringLiteral("notifications/initialized")}},
        name, send));
    const auto ping = answerAgentMcp(message(QStringLiteral("ping")), name, send);
    QVERIFY(ping);
    QCOMPARE(ping->value(QStringLiteral("id")).toInt(), 7);
    QVERIFY(ping->contains(QStringLiteral("result")));
    QCOMPARE(answerAgentMcp(message(QStringLiteral("tools/list")), name, send)
                 ->value(QStringLiteral("result"))
                 .toObject()
                 .value(QStringLiteral("tools"))
                 .toArray(),
        agentMcpTools());
    // An answer to a request this server never sent has no reply, and a
    // request that names no method is not one.
    QVERIFY(
        !answerAgentMcp({{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                            {QStringLiteral("id"), 3}, {QStringLiteral("result"), QJsonObject {}}},
            name, send));
    const auto nameless = answerAgentMcp(
        {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), 3}}, name,
        send);
    QVERIFY(nameless);
    QCOMPARE(
        nameless->value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt(),
        -32600);
    QCOMPARE(answerAgentMcp(message(QStringLiteral("resources/list")), name, send)
                 ->value(QStringLiteral("error"))
                 .toObject()
                 .value(QStringLiteral("code"))
                 .toInt(),
        -32601);
    QCOMPARE(answerAgentMcp(call(QStringLiteral("hover"), {}), name, send)
                 ->value(QStringLiteral("error"))
                 .toObject()
                 .value(QStringLiteral("code"))
                 .toInt(),
        -32602);
}

void AgentMcpTest::answersWhatTheBrowserAnswered()
{
    const QJsonObject look {{QStringLiteral("title"), QStringLiteral("Sign up")},
        {QStringLiteral("url"), QStringLiteral("http://localhost:3000/")},
        {QStringLiteral("outline"), QStringLiteral("# Sign up")},
        {QStringLiteral("targets"),
            QJsonArray {QJsonObject {{QStringLiteral("label"), QStringLiteral("1")},
                {QStringLiteral("kind"), QStringLiteral("textbox")},
                {QStringLiteral("name"), QStringLiteral("Email")}}}}};
    QJsonObject answer;
    QJsonObject asked;
    const AgentMcpSend send
        = [&](const QJsonObject &request, QString &) -> std::optional<QJsonObject> {
        asked = request;
        return answer;
    };
    const auto name = QStringLiteral("claude");

    answer = {{QStringLiteral("ok"), true}, {QStringLiteral("look"), look}};
    auto reply = answerAgentMcp(call(QStringLiteral("look"), {}), name, send);
    QCOMPARE(asked.value(QStringLiteral("verb")).toString(), QStringLiteral("look"));
    QVERIFY(!isError(*reply));
    QCOMPARE(textOf(*reply),
        QStringLiteral("Sign up\nhttp://localhost:3000/\n\n# Sign up\n\n[1] textbox \"Email\""));

    answer = {{QStringLiteral("ok"), true}, {QStringLiteral("tabs"), QJsonArray {}}};
    reply = answerAgentMcp(call(QStringLiteral("tabs"), {}), name, send);
    QCOMPARE(textOf(*reply), QStringLiteral("none"));

    answer = {{QStringLiteral("ok"), true}};
    reply = answerAgentMcp(
        call(QStringLiteral("run"), {{QStringLiteral("command"), QStringLiteral("reload")}}), name,
        send);
    QCOMPARE(textOf(*reply), QStringLiteral("ran"));

    answer
        = {{QStringLiteral("ok"), false}, {QStringLiteral("code"), QStringLiteral("allow-agents")},
            {QStringLiteral("error"), QStringLiteral("Allow agents is off.")}};
    reply = answerAgentMcp(call(QStringLiteral("look"), {}), name, send);
    QVERIFY(isError(*reply));
    QCOMPARE(textOf(*reply), QStringLiteral("Allow agents is off."));

    answer = {{QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("There is no target 9.")},
        {QStringLiteral("steps"),
            QJsonArray {QJsonObject {{QStringLiteral("ok"), false},
                {QStringLiteral("step"), QStringLiteral("click 9")},
                {QStringLiteral("error"), QStringLiteral("There is no target 9.")}}}},
        {QStringLiteral("look"), look}};
    reply = answerAgentMcp(call(QStringLiteral("do"),
                               {{QStringLiteral("steps"), QJsonArray {QStringLiteral("click 9")}}}),
        name, send);
    QVERIFY(isError(*reply));
    QVERIFY(textOf(*reply).startsWith(QStringLiteral("failed click 9: There is no target 9.\n")));
    QVERIFY(textOf(*reply).contains(QStringLiteral("[1] textbox \"Email\"")));
    QVERIFY(textOf(*reply).endsWith(QStringLiteral("\n\nThere is no target 9.")));
}

void AgentMcpTest::saysWhyTheBrowserCouldNotBeReached()
{
    const AgentMcpSend send
        = [](const QJsonObject &, QString &error) -> std::optional<QJsonObject> {
        error = QStringLiteral("No Omaweb is running, and one could not be started.");
        return std::nullopt;
    };
    const auto reply
        = answerAgentMcp(call(QStringLiteral("spaces"), {}), QStringLiteral("claude"), send);
    QVERIFY(isError(*reply));
    QCOMPARE(textOf(*reply), QStringLiteral("No Omaweb is running, and one could not be started."));
}

// Standard input and output are the whole channel: a reply is one line, sent
// as soon as it is made, and nothing else is written there.
void AgentMcpTest::servesOneMessageALine()
{
    std::istringstream input("\n"
                             "not json\n"
                             "[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}]\n"
                             "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n"
                             "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\","
                             "\"params\":{\"name\":\"spaces\",\"arguments\":{}}}\n");
    std::ostringstream output;
    QJsonObject asked;
    serveAgentMcp(input, output, QStringLiteral("claude"),
        [&asked](const QJsonObject &request, QString &) -> std::optional<QJsonObject> {
            asked = request;
            return QJsonObject {{QStringLiteral("ok"), true},
                {QStringLiteral("spaces"),
                    QJsonArray {QJsonObject {{QStringLiteral("id"), QStringLiteral("s1")},
                        {QStringLiteral("name"), QStringLiteral("Work")}}}}};
        });

    const auto lines = QString::fromStdString(output.str()).split(u'\n');
    QCOMPARE(lines.size(), 4);
    QCOMPARE(lines.last(), QString());
    const auto reply = [&lines](qsizetype index) {
        return QJsonDocument::fromJson(lines.at(index).toUtf8()).object();
    };
    const auto code = [](const QJsonObject &reply) {
        return reply.value(QStringLiteral("error"))
            .toObject()
            .value(QStringLiteral("code"))
            .toInt();
    };
    QCOMPARE(code(reply(0)), -32700);
    QVERIFY(reply(0).value(QStringLiteral("id")).isNull());
    QCOMPARE(code(reply(1)), -32600);
    QCOMPARE(reply(2).value(QStringLiteral("id")).toInt(), 2);
    QVERIFY(!isError(reply(2)));
    QCOMPARE(textOf(reply(2)), QStringLiteral("s1\tWork\t"));
    QCOMPARE(asked, spaces());
}

void AgentMcpTest::readsTheConnectionsName_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("malformed");

    QTest::newRow("the parent's name") << QStringList {} << QStringLiteral("claude") << false;
    QTest::newRow("--name then the name")
        << QStringList {QStringLiteral("--name"), QStringLiteral("checker")}
        << QStringLiteral("checker") << false;
    QTest::newRow("--name=") << QStringList {QStringLiteral("--name=checker")}
                             << QStringLiteral("checker") << false;
    QTest::newRow("--name and nothing")
        << QStringList {QStringLiteral("--name")} << QString() << true;
    QTest::newRow("an option there is not")
        << QStringList {QStringLiteral("--json")} << QString() << true;
}

void AgentMcpTest::readsTheConnectionsName()
{
    QFETCH(QStringList, arguments);
    QFETCH(QString, name);
    QFETCH(bool, malformed);

    const auto read = readAgentMcpName(
        QStringList {QStringLiteral("omaweb"), QStringLiteral("mcp")} + arguments,
        QStringLiteral("claude"));
    QCOMPARE(!read, malformed);
    if (read) {
        QCOMPARE(*read, name);
    }
}

// The current tab and a temporary Space belong to a connection, so every call
// goes over the one the server opened first.
void AgentMcpTest::holdsOneConnectionToTheBrowser()
{
    QTemporaryDir runtime;
    FakeBrowser browser(runtime.filePath(QStringLiteral("control.sock")));
    QVERIFY(browser.listen());
    QList<std::optional<QJsonObject>> answers;
    runBesideTheBrowser([&] {
        AgentMcpLink link(
            runtime.filePath(QStringLiteral("control.sock")), [] { return false; }, 0);
        QString error;
        answers.append(link.send(spaces(), error));
        answers.append(link.send(spaces(), error));
    });

    QCOMPARE(browser.connections, 1);
    QCOMPARE(browser.requests, (QList<QJsonObject> {spaces(), spaces()}));
    QCOMPARE(answers.size(), 2);
    QCOMPARE(answers.at(0)->value(QStringLiteral("answer")).toInt(), 1);
    QCOMPARE(answers.at(1)->value(QStringLiteral("answer")).toInt(), 2);
}

// Once when nothing answers, and again when the browser it reached has quit.
void AgentMcpTest::startsABrowserWhenNoneAnswers()
{
    QTemporaryDir runtime;
    FakeBrowser browser(runtime.filePath(QStringLiteral("control.sock")));
    auto starts = 0;
    QList<std::optional<QJsonObject>> answers;
    runBesideTheBrowser([&] {
        const auto start = [&] {
            ++starts;
            bool listening = false;
            QMetaObject::invokeMethod(
                &browser, "listen", Qt::BlockingQueuedConnection, qReturnArg(listening));
            return listening;
        };
        AgentMcpLink link(runtime.filePath(QStringLiteral("control.sock")), start, 5000);
        QString error;
        answers.append(link.send(spaces(), error));
        QMetaObject::invokeMethod(&browser, "quit", Qt::BlockingQueuedConnection);
        answers.append(link.send(spaces(), error));
    });

    QCOMPARE(starts, 2);
    QCOMPARE(answers.size(), 2);
    QVERIFY(answers.at(0));
    QVERIFY(answers.at(1));
    QCOMPARE(browser.connections, 2);
}

// A browser that runs with its socket closed hands a second launch to itself
// and comes forward, so a start that never answers is not tried on every call.
void AgentMcpTest::startsABrowserThatNeverAnswersOnlyOnce()
{
    QTemporaryDir runtime;
    auto starts = 0;
    QStringList errors;
    runBesideTheBrowser([&] {
        AgentMcpLink link(
            runtime.filePath(QStringLiteral("control.sock")),
            [&starts] {
                ++starts;
                return true;
            },
            200);
        for (auto call = 0; call < 2; ++call) {
            QString error;
            if (!link.send(spaces(), error)) {
                errors.append(error);
            }
        }
    });

    QCOMPARE(starts, 1);
    QCOMPARE(errors.size(), 2);
    QVERIFY(errors.at(0).startsWith(QStringLiteral("Omaweb was started but did not answer")));
    QCOMPARE(errors.at(1), QStringLiteral("Omaweb is not answering on its Agent socket."));
}

// The browser answers a connection's requests in order, so the answer to a
// call that timed out comes ahead of the next one's and is not taken for it.
void AgentMcpTest::setsAsideAnAnswerThatCameTooLate()
{
    QTemporaryDir runtime;
    FakeBrowser browser(runtime.filePath(QStringLiteral("control.sock")));
    browser.holdFirst = true;
    QVERIFY(browser.listen());
    QString late;
    std::optional<QJsonObject> first;
    std::optional<QJsonObject> next;
    runBesideTheBrowser([&] {
        AgentMcpLink link(
            runtime.filePath(QStringLiteral("control.sock")), [] { return false; }, 0);
        QString error;
        first = link.send(spaces(), late);
        next = link.send({{QStringLiteral("verb"), QStringLiteral("tabs")},
                             {QStringLiteral("name"), QStringLiteral("claude")}},
            error);
    });

    QVERIFY(!first);
    QCOMPARE(late, QStringLiteral("The browser did not answer in time."));
    QVERIFY(next);
    QCOMPARE(next->value(QStringLiteral("verb")).toString(), QStringLiteral("tabs"));
    QCOMPARE(next->value(QStringLiteral("answer")).toInt(), 2);
}

// The skill teaches the CLI, and an Agent reads only what it lists.
void AgentMcpTest::theSkillTeachesEveryVerb()
{
    QFile skill(QStringLiteral(OMAWEB_AGENT_SKILL_PATH));
    QVERIFY(skill.open(QIODevice::ReadOnly));
    const auto text = QString::fromUtf8(skill.readAll());
    for (auto tool : toolNames()) {
        const auto verb = tool.replace(u'_', u' ');
        QVERIFY2(text.contains(QStringLiteral("\nomaweb %1").arg(verb)), qPrintable(verb));
    }
    QVERIFY(text.contains(QStringLiteral("omaweb space new [name] [--temporary]")));
    QVERIFY(text.contains(QStringLiteral("--name")));
}

QTEST_GUILESS_MAIN(AgentMcpTest)
#include "tst_agentmcp.moc"
