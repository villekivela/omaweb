#include "AgentCommand.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

using omaweb::agentAnswerTimeoutMs;
using omaweb::AgentCommand;
using omaweb::formatAgentAnswer;
using omaweb::isAgentCommand;
using omaweb::misplacedAgentOption;
using omaweb::misplacedAgentOptionMessage;
using omaweb::readAgentCommand;

class AgentCommandTest final : public QObject {
    Q_OBJECT

private slots:
    void tellsAVerbFromAnAddressToOpen();
    void readsEachVerbIntoARequest_data();
    void readsEachVerbIntoARequest();
    void readsOptionsBeforeTheVerbAsAfterIt_data();
    void readsOptionsBeforeTheVerbAsAfterIt();
    void readsTheClientsOptionsBeforeTheVerb();
    void refusesAnAgentOptionThatLeadsNoVerb_data();
    void refusesAnAgentOptionThatLeadsNoVerb();
    void leavesTheBrowsersOwnArgumentsAlone();
    void readsThePickerAsATabsRequestForEverySpace();
    void refusesAMalformedCommand_data();
    void refusesAMalformedCommand();
    void namesTheConnectionAfterItsParentUnlessTold();
    void namesTheArgumentItDoesNotTake();
    void printsOneLinePerRowForAScript();
    void readsTheStepsOfABatch();
    void printsWhatAPageVerbSaw();
    void readsTheStepsThatAnswerAPage();
    void printsWhatAPageAsksOfTheAgent();
    void waitsForTheReaderToGrantASpace();
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
    QVERIFY(isAgentCommand({program, QStringLiteral("dev")}));
#ifdef OMAWEB_FILM_HOOKS
    QVERIFY(isAgentCommand({program, QStringLiteral("film-hover"), QStringLiteral("Work")}));
#else
    // The introductory film's hook is not a verb of a browser built to ship.
    QVERIFY(!isAgentCommand({program, QStringLiteral("film-hover"), QStringLiteral("Work")}));
#endif
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
    QTest::newRow("every space's tabs")
        << QStringList {QStringLiteral("tabs"), QStringLiteral("--all"), QStringLiteral("--json")}
        << base(QStringLiteral("tabs"), {{QStringLiteral("all"), true}}) << true;
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
    QTest::newRow("focus a tab and raise the window")
        << QStringList {QStringLiteral("focus"), QStringLiteral("--raise"), QStringLiteral("--"),
               QStringLiteral("t1")}
        << base(QStringLiteral("focus"),
               {{QStringLiteral("target"), QStringLiteral("t1")}, {QStringLiteral("raise"), true}})
        << false;
    // `dev` names the folder it runs in, which the browser cannot know.
    const auto here = QDir::currentPath();
    QTest::newRow("dev with an address")
        << QStringList {QStringLiteral("dev"), QStringLiteral("localhost:5173")}
        << base(QStringLiteral("dev"),
               {{QStringLiteral("address"), QStringLiteral("localhost:5173")},
                   {QStringLiteral("directory"), here}})
        << false;
    QTest::newRow("bare dev") << QStringList {QStringLiteral("dev")}
                              << base(QStringLiteral("dev"), {{QStringLiteral("directory"), here}})
                              << false;
    QTest::newRow("dev with the project's agent")
        << QStringList {QStringLiteral("dev"), QStringLiteral("--agent"),
               QStringLiteral("ssh -t devbox \"cd {dir} && claude\"")}
        << base(QStringLiteral("dev"),
               {{QStringLiteral("agent"), QStringLiteral("ssh -t devbox \"cd {dir} && claude\"")},
                   {QStringLiteral("directory"), here}})
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
    QTest::newRow("run a command on a tab")
        << QStringList {QStringLiteral("run"), QStringLiteral("pop-out-tab"),
               QStringLiteral("a1b2c3")}
        << base(QStringLiteral("run"),
               {{QStringLiteral("command"), QStringLiteral("pop-out-tab")},
                   {QStringLiteral("argument"), QStringLiteral("a1b2c3")}})
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
    QTest::newRow("eval after the options end")
        << QStringList {QStringLiteral("eval"), QStringLiteral("--json"), QStringLiteral("--"),
               QStringLiteral("--count"), QStringLiteral("--")}
        << base(QStringLiteral("eval"),
               {{QStringLiteral("expression"), QStringLiteral("--count --")}})
        << true;
#ifdef OMAWEB_FILM_HOOKS
    QTest::newRow("film hover") << QStringList {QStringLiteral("film-hover"),
        QStringLiteral("Work"), QStringLiteral("--json")}
                                << base(QStringLiteral("film-hover"),
                                       {{QStringLiteral("space"), QStringLiteral("Work")}})
                                << true;
    QTest::newRow("film hover let go") << QStringList {QStringLiteral("film-hover")}
                                       << base(QStringLiteral("film-hover")) << false;
#endif
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

void AgentCommandTest::readsOptionsBeforeTheVerbAsAfterIt_data()
{
    QTest::addColumn<QStringList>("before");
    QTest::addColumn<QStringList>("after");
    const auto url = QStringLiteral("http://localhost:8561/");
    QTest::newRow("a name") << QStringList {"--name", "x", "open", url}
                            << QStringList {"open", url, "--name", "x"};
    QTest::newRow("a name with its value inline")
        << QStringList {"--name=x", "tabs"} << QStringList {"tabs", "--name=x"};
    QTest::newRow("json and a name before a two-word verb") << QStringList {"--json", "--name", "x",
        "space", "new", "probe"} << QStringList {"space", "new", "probe", "--json", "--name", "x"};
    QTest::newRow("a tab") << QStringList {"--tab", "t", "look"}
                           << QStringList {"look", "--tab", "t"};
    QTest::newRow("a space and a flag") << QStringList {"--space", "work", "--all", "tabs"}
                                        << QStringList {"tabs", "--space", "work", "--all"};
    QTest::newRow("options on both sides") << QStringList {"--tab", "t", "do", "--settle", "5",
        "click 3"} << QStringList {"do", "--tab", "t", "--settle", "5", "click 3"};
    QTest::newRow("a flag before the verb") << QStringList {"--temporary", "space", "new"}
                                            << QStringList {"space", "new", "--temporary"};
    QTest::newRow("a badly valued option") << QStringList {"--settle", "soon", "do", "click 3"}
                                           << QStringList {"do", "--settle", "soon", "click 3"};
    QTest::newRow("an option the verb does not take")
        << QStringList {"--tab", "t", "spaces"} << QStringList {"spaces", "--tab", "t"};
    QTest::newRow("the picker with json")
        << QStringList {"--json", "--pick", "tabs"} << QStringList {"tabs", "--pick", "--json"};
}

// The options a verb takes, the values they take and the errors they give are the same wherever
// they stand (#683).
void AgentCommandTest::readsOptionsBeforeTheVerbAsAfterIt()
{
    QFETCH(QStringList, before);
    QFETCH(QStringList, after);
    const auto program = QStringLiteral("omaweb");

    QVERIFY(isAgentCommand(QStringList {program} + before));
    const auto early = readAgentCommand(QStringList {program} + before, QStringLiteral("claude"));
    const auto late = readAgentCommand(QStringList {program} + after, QStringLiteral("claude"));

    QCOMPARE(early.error, late.error);
    QCOMPARE(early.request, late.request);
    QCOMPARE(early.json, late.json);
    QCOMPARE(early.pick, late.pick);
}

void AgentCommandTest::readsTheClientsOptionsBeforeTheVerb()
{
    const auto command = readAgentCommand(
        {QStringLiteral("omaweb"), QStringLiteral("--name"), QStringLiteral("p561"),
            QStringLiteral("open"), QStringLiteral("http://localhost:8561/")},
        QStringLiteral("claude"));
    QCOMPARE(command.error, QString());
    QCOMPARE(command.request,
        (QJsonObject {{QStringLiteral("verb"), QStringLiteral("open")},
            {QStringLiteral("url"), QStringLiteral("http://localhost:8561/")},
            {QStringLiteral("name"), QStringLiteral("p561")}}));

    const auto tab = readAgentCommand({QStringLiteral("omaweb"), QStringLiteral("--tab"),
                                          QStringLiteral("t"), QStringLiteral("look")},
        QStringLiteral("claude"));
    QCOMPARE(tab.error, QString());
    QCOMPARE(tab.request,
        (QJsonObject {{QStringLiteral("verb"), QStringLiteral("look")},
            {QStringLiteral("tab"), QStringLiteral("t")},
            {QStringLiteral("name"), QStringLiteral("claude")}}));
}

void AgentCommandTest::refusesAnAgentOptionThatLeadsNoVerb_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("option");
    const auto url = QStringLiteral("https://example.com");
    QTest::newRow("json and an address") << QStringList {"--json", url} << "--json";
    QTest::newRow("a name and an address") << QStringList {"--name", "x", url} << "--name";
    QTest::newRow("a name alone") << QStringList {"--name", "x"} << "--name";
    QTest::newRow("json alone") << QStringList {"--json"} << "--json";
    QTest::newRow("a name without its value") << QStringList {"--name"} << "--name";
    QTest::newRow("a tab and a launch") << QStringList {"--tab=t"} << "--tab=t";
    QTest::newRow("a verb's flag") << QStringList {"--all", url} << "--all";
}

void AgentCommandTest::refusesAnAgentOptionThatLeadsNoVerb()
{
    QFETCH(QStringList, arguments);
    QFETCH(QString, option);
    arguments.prepend(QStringLiteral("omaweb"));

    QVERIFY(!isAgentCommand(arguments));
    QCOMPARE(misplacedAgentOption(arguments), option);
    const auto message = misplacedAgentOptionMessage(option);
    QVERIFY(message.contains(option));
    QVERIFY(message.contains(QStringLiteral("Agent verb")));
    // An example would put a verb where a valued option reads it as its value.
    QVERIFY2(!message.contains(QStringLiteral("spaces")), qPrintable(message));
}

// What is no Agent option is the browser's, and still launches it.
void AgentCommandTest::leavesTheBrowsersOwnArgumentsAlone()
{
    const auto program = QStringLiteral("omaweb");
    for (const auto &arguments : QList<QStringList> {
             {program},
             {program, QStringLiteral("--version")},
             {program, QStringLiteral("--validate-qml")},
             {program, QStringLiteral("--remote-debugging=9222"), QStringLiteral("--version")},
             {program, QStringLiteral("https://example.com/")},
             {program, QStringLiteral("mcp")},
         }) {
        QVERIFY2(misplacedAgentOption(arguments).isEmpty(), qPrintable(arguments.join(u' ')));
    }
    // An option ahead of a verb, or of `mcp`, has somewhere to go.
    QVERIFY(misplacedAgentOption({program, QStringLiteral("--json"), QStringLiteral("tabs")})
            .isEmpty());
    QVERIFY(misplacedAgentOption(
        {program, QStringLiteral("--name"), QStringLiteral("x"), QStringLiteral("mcp")})
            .isEmpty());
}

// The picker is the CLI's own: what goes over the socket is the list of every Space's tabs, and
// the choice is made here.
void AgentCommandTest::readsThePickerAsATabsRequestForEverySpace()
{
    const auto command = readAgentCommand(
        {QStringLiteral("omaweb"), QStringLiteral("tabs"), QStringLiteral("--pick"),
            QStringLiteral("--name"), QStringLiteral("hypr")},
        QStringLiteral("fish"));

    QCOMPARE(command.error, QString());
    QVERIFY(command.pick);
    QCOMPARE(command.request,
        (QJsonObject {{QStringLiteral("verb"), QStringLiteral("tabs")},
            {QStringLiteral("all"), true}, {QStringLiteral("name"), QStringLiteral("hypr")}}));
    QVERIFY(
        !readAgentCommand({QStringLiteral("omaweb"), QStringLiteral("tabs")}, QStringLiteral("x"))
            .pick);
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
    QTest::newRow("dev with two addresses") << QStringList {
        QStringLiteral("dev"), QStringLiteral("localhost:5173"), QStringLiteral("localhost:3000")};
    QTest::newRow("dev with an agent and no command")
        << QStringList {QStringLiteral("dev"), QStringLiteral("--agent")};
    QTest::newRow("the picker beside a space") << QStringList {QStringLiteral("tabs"),
        QStringLiteral("--pick"), QStringLiteral("--space"), QStringLiteral("work")};
    QTest::newRow("the picker beside json")
        << QStringList {QStringLiteral("tabs"), QStringLiteral("--pick"), QStringLiteral("--json")};
    QTest::newRow("the picker on another verb")
        << QStringList {QStringLiteral("spaces"), QStringLiteral("--pick")};
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
    // As `omaweb mcp` refuses them: a wait is never shorter than none.
    QTest::newRow("a settle before now") << QStringList {
        QStringLiteral("do"), QStringLiteral("back"), QStringLiteral("--settle=-5")};
    QTest::newRow("a timeout before now") << QStringList {QStringLiteral("do"),
        QStringLiteral("back"), QStringLiteral("--timeout"), QStringLiteral("-1")};
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

void AgentCommandTest::namesTheArgumentItDoesNotTake()
{
    const auto command = readAgentCommand(
        {QStringLiteral("omaweb"), QStringLiteral("space"), QStringLiteral("new"),
            QStringLiteral("Checks"), QStringLiteral("Extra")},
        QStringLiteral("claude"));
    QCOMPARE(command.error, QStringLiteral("`space new` takes no argument Extra."));
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

    // `dev` says which Space it opened and the address it is showing there.
    QCOMPARE(formatAgentAnswer(QStringLiteral("dev"),
                 {{QStringLiteral("ok"), true}, {QStringLiteral("space"), QStringLiteral("s1")},
                     {QStringLiteral("spaceName"), QStringLiteral("shop")},
                     {QStringLiteral("directory"), QStringLiteral("/home/reader/shop")},
                     {QStringLiteral("address"), QStringLiteral("http://localhost:5173")}}),
        QStringLiteral("shop\thttp://localhost:5173\n"));

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

// A verb that can reach one of the reader's Spaces may wait a minute for the
// reader's grant before the page is asked, and the CLI waits that long too,
// so the browser's answer is the one heard. A browser command never waits.
void AgentCommandTest::waitsForTheReaderToGrantASpace()
{
    const auto wait = [](const QString &verb) {
        return agentAnswerTimeoutMs({{QStringLiteral("verb"), verb}});
    };
    for (const auto &verb : {QStringLiteral("look"), QStringLiteral("read"), QStringLiteral("shot"),
             QStringLiteral("eval"), QStringLiteral("console")}) {
        QVERIFY2(wait(verb) >= 60000 + 10000, qPrintable(verb));
    }
    QVERIFY(agentAnswerTimeoutMs({{QStringLiteral("verb"), QStringLiteral("do")},
                {QStringLiteral("steps"), QJsonArray {QJsonObject {}}}})
        > 60000 + 65000);
    QVERIFY(wait(QStringLiteral("look")) > 60000 + 60000);
    QCOMPARE(wait(QStringLiteral("tabs")), 10000);
}

QTEST_GUILESS_MAIN(AgentCommandTest)
#include "tst_agentcommand.moc"
