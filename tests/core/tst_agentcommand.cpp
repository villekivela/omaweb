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

QTEST_GUILESS_MAIN(AgentCommandTest)
#include "tst_agentcommand.moc"
