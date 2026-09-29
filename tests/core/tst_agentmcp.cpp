#include "AgentMcp.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using omaweb::AgentMcpSend;
using omaweb::agentMcpTools;
using omaweb::answerAgentMcp;
using omaweb::readAgentMcpCall;

namespace {

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

void AgentMcpTest::readsEachToolIntoARequest_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QJsonObject>("arguments");
    QTest::addColumn<QJsonObject>("request");

    const auto base = [](const QString &verb, QJsonObject fields = {}) {
        fields.insert(QStringLiteral("verb"), verb);
        fields.insert(QStringLiteral("name"), QStringLiteral("claude"));
        return fields;
    };
    QTest::newRow("spaces") << QStringLiteral("spaces") << QJsonObject {}
                            << base(QStringLiteral("spaces"));
    QTest::newRow("open a new tab in a space")
        << QStringLiteral("open")
        << QJsonObject {{QStringLiteral("url"), QStringLiteral("http://localhost:3000/")},
               {QStringLiteral("space"), QStringLiteral("Checks")}, {QStringLiteral("new"), true}}
        << base(QStringLiteral("open"),
               {{QStringLiteral("url"), QStringLiteral("http://localhost:3000/")},
                   {QStringLiteral("space"), QStringLiteral("Checks")},
                   {QStringLiteral("new"), true}});
    QTest::newRow("switch space") << QStringLiteral("space")
                                  << QJsonObject {{QStringLiteral("space"), QStringLiteral("Work")}}
                                  << base(QStringLiteral("space"),
                                         {{QStringLiteral("space"), QStringLiteral("Work")}});
    QTest::newRow("focus names its tab as the target")
        << QStringLiteral("focus")
        << QJsonObject {{QStringLiteral("tab"), QStringLiteral("github")}}
        << base(QStringLiteral("focus"), {{QStringLiteral("target"), QStringLiteral("github")}});
    QTest::newRow("commands") << QStringLiteral("commands") << QJsonObject {}
                              << base(QStringLiteral("commands"));
    QTest::newRow("run a command at a position")
        << QStringLiteral("run")
        << QJsonObject {{QStringLiteral("command"), QStringLiteral("select-space")},
               {QStringLiteral("position"), 2}}
        << base(QStringLiteral("run"),
               {{QStringLiteral("command"), QStringLiteral("select-space")},
                   {QStringLiteral("argument"), 2}});
    QTest::newRow("a flag left false is left out")
        << QStringLiteral("look") << QJsonObject {{QStringLiteral("all"), false}}
        << base(QStringLiteral("look"));
    QTest::newRow("space new names the space")
        << QStringLiteral("space_new")
        << QJsonObject {{QStringLiteral("name"), QStringLiteral("Checks")},
               {QStringLiteral("temporary"), true}}
        << base(QStringLiteral("space new"),
               {{QStringLiteral("space"), QStringLiteral("Checks")},
                   {QStringLiteral("temporary"), true}});
    QTest::newRow("space delete") << QStringLiteral(
        "space_delete") << QJsonObject {{QStringLiteral("space"), QStringLiteral("Checks")}}
                                  << base(QStringLiteral("space delete"),
                                         {{QStringLiteral("space"), QStringLiteral("Checks")}});
    QTest::newRow("do reads its steps as the CLI does")
        << QStringLiteral("do")
        << QJsonObject {{QStringLiteral("steps"),
                            QJsonArray {QStringLiteral("fill 1 \"A Reader\""),
                                QStringLiteral("click 2; wait text Thanks")}},
               {QStringLiteral("settle"), 500}}
        << base(QStringLiteral("do"),
               {{QStringLiteral("steps"),
                    QJsonArray {QJsonObject {{QStringLiteral("action"), QStringLiteral("fill")},
                                    {QStringLiteral("target"), QStringLiteral("1")},
                                    {QStringLiteral("text"), QStringLiteral("A Reader")}},
                        QJsonObject {{QStringLiteral("action"), QStringLiteral("click")},
                            {QStringLiteral("target"), QStringLiteral("2")}},
                        QJsonObject {{QStringLiteral("action"), QStringLiteral("wait")},
                            {QStringLiteral("text"), QStringLiteral("Thanks")}}}},
                   {QStringLiteral("settle"), 500}});
    QTest::newRow("eval") << QStringLiteral("eval")
                          << QJsonObject {{QStringLiteral("expression"), QStringLiteral("--x")},
                                 {QStringLiteral("tab"), QStringLiteral("t1")}}
                          << base(QStringLiteral("eval"),
                                 {{QStringLiteral("expression"), QStringLiteral("--x")},
                                     {QStringLiteral("tab"), QStringLiteral("t1")}});
    QTest::newRow("console since a cursor")
        << QStringLiteral("console")
        << QJsonObject {{QStringLiteral("level"), QStringLiteral("error")},
               {QStringLiteral("since"), 12}}
        << base(QStringLiteral("console"),
               {{QStringLiteral("level"), QStringLiteral("error")}, {QStringLiteral("since"), 12}});
}

void AgentMcpTest::readsEachToolIntoARequest()
{
    QFETCH(QString, tool);
    QFETCH(QJsonObject, arguments);
    QFETCH(QJsonObject, request);

    const auto read = readAgentMcpCall(tool, arguments, QStringLiteral("claude"));
    QCOMPARE(read.error, QString());
    QVERIFY(!read.unknown);
    QCOMPARE(read.request, request);
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
    QCOMPARE(initialize(QStringLiteral("2099-01-01"))
                 .value(QStringLiteral("protocolVersion"))
                 .toString(),
        QStringLiteral("2025-06-18"));

    QVERIFY(!answerAgentMcp(
        {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("method"), QStringLiteral("notifications/initialized")}},
        name, send));
    const auto ping = answerAgentMcp(message(QStringLiteral("ping")), name, send);
    QCOMPARE(ping->value(QStringLiteral("id")).toInt(), 7);
    QVERIFY(ping->contains(QStringLiteral("result")));
    QCOMPARE(answerAgentMcp(message(QStringLiteral("tools/list")), name, send)
                 ->value(QStringLiteral("result"))
                 .toObject()
                 .value(QStringLiteral("tools"))
                 .toArray(),
        agentMcpTools());
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

QTEST_GUILESS_MAIN(AgentMcpTest)
#include "tst_agentmcp.moc"
