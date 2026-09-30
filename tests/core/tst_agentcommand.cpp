#include "AgentCommand.h"

#include <QDir>
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
    void readsTheStepsThatAnswerAPage();
    void printsWhatAPageAsksOfTheAgent();
};

void AgentCommandTest::tellsAVerbFromAnAddressToOpen()
{
    const auto program = QStringLiteral("omaweb");
    QVERIFY(isAgentCommand({program, QStringLiteral("tabs")}));
    QVERIFY(isAgentCommand({program, QStringLiteral("space"), QStringLiteral("new")}));
    QVERIFY(isAgentCommand({program, QStringLiteral("space"), QStringLiteral("work")}));
    QVERIFY(isAgentCommand({program, QStringLiteral("run"), QStringLiteral("toggle-sidebar")}));
    QVERIFY(isAgentCommand({program, QStringLiteral("focus"), QStringLiteral("github")}));
    QVERIFY(isAgentCommand({program, QStringLiteral("commands")}));
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
    QTest::newRow("console") << QStringList {QStringLiteral("console")}
                             << base(QStringLiteral("console")) << false;
    QTest::newRow("console errors since a cursor")
        << QStringList {QStringLiteral("console"), QStringLiteral("--level"),
               QStringLiteral("error"), QStringLiteral("--since=12"), QStringLiteral("--json")}
        << base(QStringLiteral("console"),
               {{QStringLiteral("level"), QStringLiteral("error")}, {QStringLiteral("since"), 12}})
        << true;
    QTest::newRow("space delete") << QStringList {QStringLiteral("space"), QStringLiteral("delete"),
        QStringLiteral("s1")}
                                  << base(QStringLiteral("space delete"),
                                         {{QStringLiteral("space"), QStringLiteral("s1")}})
                                  << false;
    QTest::newRow("switch space") << QStringList {QStringLiteral("space"), QStringLiteral("work")}
                                  << base(QStringLiteral("space"),
                                         {{QStringLiteral("space"), QStringLiteral("work")}})
                                  << false;
    QTest::newRow("focus a tab by its address")
        << QStringList {QStringLiteral("focus"), QStringLiteral("github.com")}
        << base(QStringLiteral("focus"), {{QStringLiteral("target"), QStringLiteral("github.com")}})
        << false;
    QTest::newRow("commands") << QStringList {QStringLiteral("commands"), QStringLiteral("--json")}
                              << base(QStringLiteral("commands")) << true;
    QTest::newRow("run a command")
        << QStringList {QStringLiteral("run"), QStringLiteral("toggle-sidebar")}
        << base(QStringLiteral("run"),
               {{QStringLiteral("command"), QStringLiteral("toggle-sidebar")}})
        << false;
    QTest::newRow("run a command with a position")
        << QStringList {QStringLiteral("run"), QStringLiteral("select-tab"), QStringLiteral("3")}
        << base(QStringLiteral("run"),
               {{QStringLiteral("command"), QStringLiteral("select-tab")},
                   {QStringLiteral("argument"), 3}})
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
    QTest::newRow("space without a space") << QStringList {QStringLiteral("space")};
    QTest::newRow("space with two")
        << QStringList {QStringLiteral("space"), QStringLiteral("Work"), QStringLiteral("Home")};
    QTest::newRow("focus without a tab") << QStringList {QStringLiteral("focus")};
    QTest::newRow("commands takes no argument")
        << QStringList {QStringLiteral("commands"), QStringLiteral("tabs")};
    QTest::newRow("run without a command") << QStringList {QStringLiteral("run")};
    QTest::newRow("a position that is not a number") << QStringList {
        QStringLiteral("run"), QStringLiteral("select-tab"), QStringLiteral("first")};
    QTest::newRow("run with two arguments") << QStringList {QStringLiteral("run"),
        QStringLiteral("select-tab"), QStringLiteral("1"), QStringLiteral("2")};
    QTest::newRow("space delete without a space")
        << QStringList {QStringLiteral("space"), QStringLiteral("delete")};
    QTest::newRow("a level that is not one") << QStringList {
        QStringLiteral("console"), QStringLiteral("--level"), QStringLiteral("loud")};
    QTest::newRow("an empty level")
        << QStringList {QStringLiteral("console"), QStringLiteral("--level=")};
    QTest::newRow("a cursor that is not one") << QStringList {
        QStringLiteral("console"), QStringLiteral("--since"), QStringLiteral("soon")};
    QTest::newRow("console takes no argument")
        << QStringList {QStringLiteral("console"), QStringLiteral("errors")};
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

    const QJsonObject commands {
        {QStringLiteral("ok"), true},
        {QStringLiteral("commands"),
            QJsonArray {QJsonObject {{QStringLiteral("command"), QStringLiteral("toggle-sidebar")},
                {QStringLiteral("title"), QStringLiteral("Hide or show the sidebar")},
                {QStringLiteral("group"), QStringLiteral("interface")}}}},
    };
    QCOMPARE(formatAgentAnswer(QStringLiteral("commands"), commands),
        QStringLiteral("toggle-sidebar\tHide or show the sidebar\n"));
    QCOMPARE(formatAgentAnswer(QStringLiteral("run"), {{QStringLiteral("ok"), true}}), QString());
    QCOMPARE(formatAgentAnswer(QStringLiteral("focus"),
                 {{QStringLiteral("ok"), true}, {QStringLiteral("tab"), QStringLiteral("t1")}}),
        QStringLiteral("t1\n"));

    const QJsonObject opened {{QStringLiteral("ok"), true},
        {QStringLiteral("tab"), QJsonObject {{QStringLiteral("id"), QStringLiteral("t2")}}}};
    QCOMPARE(formatAgentAnswer(QStringLiteral("open"), opened), QStringLiteral("t2\n"));

    const QJsonObject console {
        {QStringLiteral("ok"), true},
        {QStringLiteral("messages"),
            QJsonArray {
                QJsonObject {{QStringLiteral("level"), QStringLiteral("error")},
                    {QStringLiteral("message"), QStringLiteral("boom\nat app.js\r\tcol 3")},
                    {QStringLiteral("source"), QStringLiteral("http://localhost/app.js")},
                    {QStringLiteral("line"), 42}},
                QJsonObject {{QStringLiteral("level"), QStringLiteral("info")},
                    {QStringLiteral("message"), QStringLiteral("hi")},
                    {QStringLiteral("source"), QString {}}, {QStringLiteral("line"), 0}},
            }},
        {QStringLiteral("cursor"), 17},
        {QStringLiteral("truncated"), true},
    };
    QCOMPARE(formatAgentAnswer(QStringLiteral("console"), console),
        QStringLiteral("truncated\tolder messages were dropped before this call\n"
                       "error\thttp://localhost/app.js:42\tboom\\nat app.js\\r\\tcol 3\n"
                       "info\t\thi\n"
                       "cursor\t17\n"));
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

// A dialog is answered and a file chooser given files by steps of their own.
// An upload's paths are the Agent's, so one relative to where it runs is made
// whole before it leaves.
void AgentCommandTest::readsTheStepsThatAnswerAPage()
{
    const auto command = readAgentCommand(
        {QStringLiteral("omaweb"), QStringLiteral("do"), QStringLiteral("dialog accept"),
            QStringLiteral("dialog accept 'New York'; dialog dismiss"),
            QStringLiteral("upload 4 report.pdf /tmp/photo.jpg")},
        QStringLiteral("claude"));
    QVERIFY2(command.error.isEmpty(), qPrintable(command.error));
    const QJsonArray expected {
        QJsonObject {{QStringLiteral("action"), QStringLiteral("dialog")},
            {QStringLiteral("answer"), QStringLiteral("accept")}},
        QJsonObject {{QStringLiteral("action"), QStringLiteral("dialog")},
            {QStringLiteral("answer"), QStringLiteral("accept")},
            {QStringLiteral("text"), QStringLiteral("New York")}},
        QJsonObject {{QStringLiteral("action"), QStringLiteral("dialog")},
            {QStringLiteral("answer"), QStringLiteral("dismiss")}},
        QJsonObject {{QStringLiteral("action"), QStringLiteral("upload")},
            {QStringLiteral("target"), QStringLiteral("4")},
            {QStringLiteral("files"),
                QJsonArray {QDir::current().absoluteFilePath(QStringLiteral("report.pdf")),
                    QStringLiteral("/tmp/photo.jpg")}}},
    };
    QCOMPARE(command.request.value(QStringLiteral("steps")).toArray(), expected);

    for (const auto &malformed : {QStringLiteral("dialog"), QStringLiteral("dialog maybe"),
             QStringLiteral("dialog dismiss now"), QStringLiteral("upload 4")}) {
        const auto refused = readAgentCommand(
            {QStringLiteral("omaweb"), QStringLiteral("do"), malformed}, QStringLiteral("claude"));
        QVERIFY2(!refused.error.isEmpty(), qPrintable(malformed));
    }
}

void AgentCommandTest::printsWhatAPageAsksOfTheAgent()
{
    const QJsonObject stopped {
        {QStringLiteral("title"), QStringLiteral("Shop")},
        {QStringLiteral("url"), QStringLiteral("https://shop.example/")},
        {QStringLiteral("dialog"),
            QJsonObject {{QStringLiteral("kind"), QStringLiteral("prompt")},
                {QStringLiteral("message"), QStringLiteral("Your city?")},
                {QStringLiteral("defaultText"), QStringLiteral("Oulu")}}},
        {QStringLiteral("downloads"),
            QJsonArray {QJsonObject {{QStringLiteral("path"), QStringLiteral("/d/Agents/c/a.pdf")},
                            {QStringLiteral("state"), QStringLiteral("completed")}},
                QJsonObject {{QStringLiteral("held"), true},
                    {QStringLiteral("fileName"), QStringLiteral("setup.sh")},
                    {QStringLiteral("risk"), QStringLiteral("script")}}}},
    };
    const QJsonObject batch {
        {QStringLiteral("ok"), true},
        {QStringLiteral("steps"),
            QJsonArray {QJsonObject {{QStringLiteral("step"), QStringLiteral("click 2")},
                {QStringLiteral("ok"), true}, {QStringLiteral("settled"), true}}}},
        {QStringLiteral("opened"), QJsonArray {QStringLiteral("window-1")}},
        {QStringLiteral("look"), stopped},
    };
    QCOMPARE(formatAgentAnswer(QStringLiteral("do"), batch),
        QStringLiteral("ok click 2\nOpened window-1\n\nShop\nhttps://shop.example/\n\n"
                       "Dialog (prompt) \"Your city?\" = \"Oulu\"\n"
                       "The page waits for `dialog accept` or `dialog dismiss`.\n"
                       "Download completed: /d/Agents/c/a.pdf\n"
                       "Download held for the reader: setup.sh (script)\n"));

    const QJsonObject tabs {{QStringLiteral("ok"), true},
        {QStringLiteral("tabs"),
            QJsonArray {QJsonObject {{QStringLiteral("id"), QStringLiteral("window-1")},
                {QStringLiteral("window"), true},
                {QStringLiteral("opener"), QStringLiteral("t2")}}}}};
    QCOMPARE(formatAgentAnswer(QStringLiteral("tabs"), tabs),
        QStringLiteral("window-1\t\t\twindow of t2\n"));
}

QTEST_GUILESS_MAIN(AgentCommandTest)
#include "tst_agentcommand.moc"
