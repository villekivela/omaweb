#include "AgentCommand.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

using omaweb::AgentCommand;
using omaweb::formatAgentAnswer;
using omaweb::isAgentCommand;
using omaweb::readAgentCommand;

class AgentCommandTest final : public QObject {
    Q_OBJECT

private slots:
    void tellsAVerbFromAnAddressToOpen();
    void readsEachVerbIntoARequest_data();
    void readsEachVerbIntoARequest();
    void refusesAMalformedCommand_data();
    void refusesAMalformedCommand();
    void namesTheConnectionAfterItsParentUnlessTold();
    void printsOneLinePerRowForAScript();
    void readsTheStepsOfABatch();
    void printsWhatAPageVerbSaw();
};

void AgentCommandTest::tellsAVerbFromAnAddressToOpen()
{
    const auto program = QStringLiteral("omaweb");
    QVERIFY(isAgentCommand({program, QStringLiteral("tabs")}));
    QVERIFY(isAgentCommand({program, QStringLiteral("space"), QStringLiteral("new")}));
    QVERIFY(!isAgentCommand({program}));
    QVERIFY(!isAgentCommand({program, QStringLiteral("https://example.com/")}));
    QVERIFY(!isAgentCommand({program, QStringLiteral("--version")}));
}

void AgentCommandTest::readsEachVerbIntoARequest_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QJsonObject>("request");
    QTest::addColumn<bool>("json");

    const auto base = [](const QString &verb, QJsonObject fields = {}) {
        fields.insert(QStringLiteral("verb"), verb);
        fields.insert(QStringLiteral("name"), QStringLiteral("claude"));
        return fields;
    };
    QTest::newRow("spaces") << QStringList {QStringLiteral("spaces")}
                            << base(QStringLiteral("spaces")) << false;
    QTest::newRow("tabs in a space")
        << QStringList {QStringLiteral("tabs"), QStringLiteral("--space"), QStringLiteral("Work"),
               QStringLiteral("--json")}
        << base(QStringLiteral("tabs"), {{QStringLiteral("space"), QStringLiteral("Work")}})
        << true;
    QTest::newRow("open in a tab") << QStringList {QStringLiteral("open"),
        QStringLiteral("example.com"), QStringLiteral("--tab=t1")}
                                   << base(QStringLiteral("open"),
                                          {{QStringLiteral("url"), QStringLiteral("example.com")},
                                              {QStringLiteral("tab"), QStringLiteral("t1")}})
                                   << false;
    QTest::newRow("open a new tab")
        << QStringList {QStringLiteral("open"), QStringLiteral("--new"),
               QStringLiteral("http://localhost:3000/")}
        << base(QStringLiteral("open"),
               {{QStringLiteral("url"), QStringLiteral("http://localhost:3000/")},
                   {QStringLiteral("new"), true}})
        << false;
    QTest::newRow("close") << QStringList {QStringLiteral("close")} << base(QStringLiteral("close"))
                           << false;
    QTest::newRow("space new with a name")
        << QStringList {QStringLiteral("space"), QStringLiteral("new"), QStringLiteral("Checks")}
        << base(QStringLiteral("space new"), {{QStringLiteral("space"), QStringLiteral("Checks")}})
        << false;
    QTest::newRow("space new temporary")
        << QStringList {QStringLiteral("space"), QStringLiteral("new"),
               QStringLiteral("--temporary")}
        << base(QStringLiteral("space new"), {{QStringLiteral("temporary"), true}}) << false;
    QTest::newRow("space new without one")
        << QStringList {QStringLiteral("space"), QStringLiteral("new")}
        << base(QStringLiteral("space new")) << false;
    QTest::newRow("space delete") << QStringList {QStringLiteral("space"), QStringLiteral("delete"),
        QStringLiteral("s1")}
                                  << base(QStringLiteral("space delete"),
                                         {{QStringLiteral("space"), QStringLiteral("s1")}})
                                  << false;
    QTest::newRow("look at the whole page")
        << QStringList {QStringLiteral("look"), QStringLiteral("--all")}
        << base(QStringLiteral("look"), {{QStringLiteral("all"), true}}) << false;
    QTest::newRow("read a part") << QStringList {QStringLiteral("read"), QStringLiteral("main")}
                                 << base(QStringLiteral("read"),
                                        {{QStringLiteral("selector"), QStringLiteral("main")}})
                                 << false;
    QTest::newRow("shot the whole page of a tab")
        << QStringList {QStringLiteral("shot"), QStringLiteral("--full"), QStringLiteral("--tab"),
               QStringLiteral("t1")}
        << base(QStringLiteral("shot"),
               {{QStringLiteral("full"), true}, {QStringLiteral("tab"), QStringLiteral("t1")}})
        << false;
    QTest::newRow("eval an expression in words")
        << QStringList {QStringLiteral("eval"), QStringLiteral("document.title"),
               QStringLiteral("+"), QStringLiteral("'!'")}
        << base(QStringLiteral("eval"),
               {{QStringLiteral("expression"), QStringLiteral("document.title + '!'")}})
        << false;
}

void AgentCommandTest::readsEachVerbIntoARequest()
{
    QFETCH(QStringList, arguments);
    QFETCH(QJsonObject, request);
    QFETCH(bool, json);
    arguments.prepend(QStringLiteral("omaweb"));
    const auto command = readAgentCommand(arguments, QStringLiteral("claude"));
    QVERIFY2(command.error.isEmpty(), qPrintable(command.error));
    QCOMPARE(command.request, request);
    QCOMPARE(command.json, json);
}

void AgentCommandTest::refusesAMalformedCommand_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::newRow("open without an address") << QStringList {QStringLiteral("open")};
    QTest::newRow("an option without its value")
        << QStringList {QStringLiteral("tabs"), QStringLiteral("--space")};
    QTest::newRow("an unknown option")
        << QStringList {QStringLiteral("spaces"), QStringLiteral("--all")};
    QTest::newRow("a space verb that is not one")
        << QStringList {QStringLiteral("space"), QStringLiteral("rename")};
    QTest::newRow("space delete without a space")
        << QStringList {QStringLiteral("space"), QStringLiteral("delete")};
    QTest::newRow("two addresses") << QStringList {
        QStringLiteral("open"), QStringLiteral("a.example"), QStringLiteral("b.example")};
    QTest::newRow("do without a step") << QStringList {QStringLiteral("do")};
    QTest::newRow("a step that is not one")
        << QStringList {QStringLiteral("do"), QStringLiteral("dance 3")};
    QTest::newRow("click without a label")
        << QStringList {QStringLiteral("do"), QStringLiteral("click")};
    QTest::newRow("a quote left open")
        << QStringList {QStringLiteral("do"), QStringLiteral("fill 3 \"half")};
    QTest::newRow("wait for nothing")
        << QStringList {QStringLiteral("do"), QStringLiteral("wait 3")};
    QTest::newRow("a settle that is not a number") << QStringList {QStringLiteral("do"),
        QStringLiteral("back"), QStringLiteral("--settle"), QStringLiteral("soon")};
    QTest::newRow("eval of nothing") << QStringList {QStringLiteral("eval")};
    QTest::newRow("look at one part")
        << QStringList {QStringLiteral("look"), QStringLiteral("main")};
}

void AgentCommandTest::refusesAMalformedCommand()
{
    QFETCH(QStringList, arguments);
    arguments.prepend(QStringLiteral("omaweb"));
    QVERIFY(!readAgentCommand(arguments, QStringLiteral("claude")).error.isEmpty());
}

void AgentCommandTest::namesTheConnectionAfterItsParentUnlessTold()
{
    const auto told = readAgentCommand({QStringLiteral("omaweb"), QStringLiteral("spaces"),
                                           QStringLiteral("--name"), QStringLiteral("checker")},
        QStringLiteral("zsh"));
    QCOMPARE(told.request.value(QStringLiteral("name")).toString(), QStringLiteral("checker"));
    const auto untold
        = readAgentCommand({QStringLiteral("omaweb"), QStringLiteral("spaces")}, QString {});
    QVERIFY(!untold.request.value(QStringLiteral("name")).toString().isEmpty());
}

void AgentCommandTest::printsOneLinePerRowForAScript()
{
    const QJsonObject spaces {
        {QStringLiteral("ok"), true},
        {QStringLiteral("spaces"),
            QJsonArray {
                QJsonObject {{QStringLiteral("id"), QStringLiteral("s1")},
                    {QStringLiteral("name"), QStringLiteral("Personal")},
                    {QStringLiteral("onShow"), true}, {QStringLiteral("agent"), false}},
                QJsonObject {{QStringLiteral("id"), QStringLiteral("s2")},
                    {QStringLiteral("name"), QStringLiteral("Agent")},
                    {QStringLiteral("onShow"), false}, {QStringLiteral("agent"), true}},
            }},
    };
    QCOMPARE(formatAgentAnswer(QStringLiteral("spaces"), spaces),
        QStringLiteral("s1\tPersonal\ton-show\ns2\tAgent\tagent\n"));

    const QJsonObject tabs {
        {QStringLiteral("ok"), true},
        {QStringLiteral("tabs"),
            QJsonArray {QJsonObject {{QStringLiteral("id"), QStringLiteral("t1")},
                {QStringLiteral("url"), QStringLiteral("https://example.com/")},
                {QStringLiteral("title"), QStringLiteral("Example")},
                {QStringLiteral("pinned"), false}, {QStringLiteral("current"), true}}}},
    };
    QCOMPARE(formatAgentAnswer(QStringLiteral("tabs"), tabs),
        QStringLiteral("t1\thttps://example.com/\tExample\tcurrent\n"));

    const QJsonObject opened {{QStringLiteral("ok"), true},
        {QStringLiteral("tab"), QJsonObject {{QStringLiteral("id"), QStringLiteral("t2")}}}};
    QCOMPARE(formatAgentAnswer(QStringLiteral("open"), opened), QStringLiteral("t2\n"));
}

// Each argument is a step, or several separated by `;`, and a quoted run is
// one word with its spacing kept.
void AgentCommandTest::readsTheStepsOfABatch()
{
    const auto command = readAgentCommand(
        {QStringLiteral("omaweb"), QStringLiteral("do"), QStringLiteral("click 3"),
            QStringLiteral("fill 5 \"two  words\"; press Control+a"),
            QStringLiteral("select 7 New Zealand"),
            QStringLiteral("scroll down; back; wait text 'Thank you'; wait url /done"),
            QStringLiteral("fill 9 ''"), QStringLiteral("--settle"), QStringLiteral("500"),
            QStringLiteral("--timeout=2000")},
        QStringLiteral("claude"));
    QVERIFY2(command.error.isEmpty(), qPrintable(command.error));
    const auto step = [](QJsonObject fields) { return fields; };
    const QJsonArray expected {
        step({{QStringLiteral("action"), QStringLiteral("click")},
            {QStringLiteral("target"), QStringLiteral("3")}}),
        step({{QStringLiteral("action"), QStringLiteral("fill")},
            {QStringLiteral("target"), QStringLiteral("5")},
            {QStringLiteral("text"), QStringLiteral("two  words")}}),
        step({{QStringLiteral("action"), QStringLiteral("press")},
            {QStringLiteral("key"), QStringLiteral("Control+a")}}),
        step({{QStringLiteral("action"), QStringLiteral("select")},
            {QStringLiteral("target"), QStringLiteral("7")},
            {QStringLiteral("text"), QStringLiteral("New Zealand")}}),
        step({{QStringLiteral("action"), QStringLiteral("scroll")},
            {QStringLiteral("target"), QStringLiteral("down")}}),
        step({{QStringLiteral("action"), QStringLiteral("back")}}),
        step({{QStringLiteral("action"), QStringLiteral("wait")},
            {QStringLiteral("text"), QStringLiteral("Thank you")}}),
        step({{QStringLiteral("action"), QStringLiteral("wait")},
            {QStringLiteral("url"), QStringLiteral("/done")}}),
        step({{QStringLiteral("action"), QStringLiteral("fill")},
            {QStringLiteral("target"), QStringLiteral("9")}, {QStringLiteral("text"), QString()}}),
    };
    QCOMPARE(command.request.value(QStringLiteral("steps")).toArray(), expected);
    QCOMPARE(command.request.value(QStringLiteral("settle")).toInt(), 500);
    QCOMPARE(command.request.value(QStringLiteral("timeout")).toInt(), 2000);

    // A screenshot's file is named as given. The browser writes it in its own
    // directory, and refuses a name that is a path.
    const auto shot = readAgentCommand({QStringLiteral("omaweb"), QStringLiteral("shot"),
                                           QStringLiteral("--output"), QStringLiteral("page.png")},
        QStringLiteral("claude"));
    QCOMPARE(shot.request.value(QStringLiteral("output")).toString(), QStringLiteral("page.png"));
}

void AgentCommandTest::printsWhatAPageVerbSaw()
{
    const QJsonObject look {
        {QStringLiteral("title"), QStringLiteral("Sign up")},
        {QStringLiteral("url"), QStringLiteral("http://localhost:3000/join")},
        {QStringLiteral("outline"), QStringLiteral("# Join us")},
        {QStringLiteral("targets"),
            QJsonArray {
                QJsonObject {{QStringLiteral("label"), QStringLiteral("1")},
                    {QStringLiteral("kind"), QStringLiteral("email")},
                    {QStringLiteral("name"), QStringLiteral("Email")},
                    {QStringLiteral("value"), QStringLiteral("a@b.example")}},
                QJsonObject {{QStringLiteral("label"), QStringLiteral("2")},
                    {QStringLiteral("kind"), QStringLiteral("checkbox")},
                    {QStringLiteral("name"), QStringLiteral("Terms")},
                    {QStringLiteral("checked"), true}},
            }},
        {QStringLiteral("above"), 0},
        {QStringLiteral("below"), 41},
    };
    const auto seen = QStringLiteral("Sign up\nhttp://localhost:3000/join\n\n# Join us\n\n"
                                     "[1] email \"Email\" = \"a@b.example\"\n"
                                     "[2] checkbox \"Terms\" (checked)\n41 more below\n");
    QCOMPARE(formatAgentAnswer(QStringLiteral("look"),
                 {{QStringLiteral("ok"), true}, {QStringLiteral("look"), look}}),
        seen);

    const QJsonObject batch {
        {QStringLiteral("ok"), false},
        {QStringLiteral("steps"),
            QJsonArray {QJsonObject {{QStringLiteral("step"), QStringLiteral("click 2")},
                            {QStringLiteral("ok"), true}, {QStringLiteral("settled"), true}},
                QJsonObject {{QStringLiteral("step"), QStringLiteral("click 9")},
                    {QStringLiteral("ok"), false},
                    {QStringLiteral("error"),
                        QStringLiteral("Label 9 names an element that has gone.")}}}},
        {QStringLiteral("look"), look},
    };
    QCOMPARE(formatAgentAnswer(QStringLiteral("do"), batch),
        QStringLiteral("ok click 2\nfailed click 9: Label 9 names an element that has gone.\n\n")
            + seen);

    QCOMPARE(formatAgentAnswer(QStringLiteral("eval"),
                 {{QStringLiteral("ok"), true},
                     {QStringLiteral("value"), QJsonObject {{QStringLiteral("n"), 1}}}}),
        QStringLiteral("{\"n\":1}\n"));
    QCOMPARE(formatAgentAnswer(QStringLiteral("eval"),
                 {{QStringLiteral("ok"), true}, {QStringLiteral("value"), 42}}),
        QStringLiteral("42\n"));
}

QTEST_GUILESS_MAIN(AgentCommandTest)
#include "tst_agentcommand.moc"
