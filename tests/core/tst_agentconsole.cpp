#include "AgentConsole.h"
#include "AgentControl.h"
#include "BrowserController.h"
#include "SessionFixture.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using omaweb::AgentConsole;
using omaweb::AgentControl;
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

QStringList texts(const QVector<AgentConsole::Message> &messages)
{
    QStringList said;
    for (const auto &message : messages) {
        said.append(message.text);
    }
    return said;
}

QStringList texts(const QJsonObject &answer)
{
    QStringList said;
    for (const auto &message : answer.value(QStringLiteral("messages")).toArray()) {
        said.append(message.toObject().value(QStringLiteral("message")).toString());
    }
    return said;
}

QString code(const QJsonObject &answer) { return answer.value(QStringLiteral("code")).toString(); }

} // namespace

class AgentConsoleTest final : public QObject {
    Q_OBJECT

private slots:
    void filtersByLevelInTheOrderLogged();
    void answersOnlyWhatCameAfterTheCursor();
    void startsAgainWithEachDocument();
    void keepsOnlyTheNewestMessages();
    void readsOnlyTheLevelsItNames();
    void answersAnAgentTabsConsole();
    void listensOnlyToAgentTabs();
    void waitsForAllowAgentsAndAnAgentSpace();
    void forgetsATabThatIsNoLongerAnAgents();
};

void AgentConsoleTest::filtersByLevelInTheOrderLogged()
{
    AgentConsole console;
    console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Info,
        QStringLiteral("hello"), QStringLiteral("https://a.example/app.js"), 3);
    console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Error,
        QStringLiteral("boom"), QStringLiteral("https://a.example/app.js"), 7);
    console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Warning,
        QStringLiteral("careful"), QStringLiteral("https://a.example/app.js"), 9);
    console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Error,
        QStringLiteral("again"), {}, 0);

    QCOMPARE(texts(console.read(QStringLiteral("t"), AgentConsole::Info, 0).messages),
        QStringList({QStringLiteral("hello"), QStringLiteral("boom"), QStringLiteral("careful"),
            QStringLiteral("again")}));
    QCOMPARE(texts(console.read(QStringLiteral("t"), AgentConsole::Warning, 0).messages),
        QStringList({QStringLiteral("boom"), QStringLiteral("careful"), QStringLiteral("again")}));
    const auto errors = console.read(QStringLiteral("t"), AgentConsole::Error, 0);
    QCOMPARE(
        texts(errors.messages), QStringList({QStringLiteral("boom"), QStringLiteral("again")}));
    QCOMPARE(errors.messages.constFirst().source, QStringLiteral("https://a.example/app.js"));
    QCOMPARE(errors.messages.constFirst().line, 7);
    QVERIFY(texts(console.read(QStringLiteral("other"), AgentConsole::Info, 0).messages).isEmpty());
}

// The cursor is the newest message kept, whatever the level asked for, so a
// reader filtering for errors does not see an old warning come back later.
void AgentConsoleTest::answersOnlyWhatCameAfterTheCursor()
{
    AgentConsole console;
    console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Error,
        QStringLiteral("first"), {}, 1);
    console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Info,
        QStringLiteral("chatter"), {}, 2);
    const auto first = console.read(QStringLiteral("t"), AgentConsole::Error, 0);
    QCOMPARE(texts(first.messages), QStringList {QStringLiteral("first")});

    const auto nothingNew = console.read(QStringLiteral("t"), AgentConsole::Error, first.cursor);
    QVERIFY(nothingNew.messages.isEmpty());
    QCOMPARE(nothingNew.cursor, first.cursor);

    console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Warning,
        QStringLiteral("late"), {}, 3);
    console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Error,
        QStringLiteral("later"), {}, 4);
    const auto next = console.read(QStringLiteral("t"), AgentConsole::Warning, first.cursor);
    QCOMPARE(texts(next.messages), QStringList({QStringLiteral("late"), QStringLiteral("later")}));
    QVERIFY(next.cursor > first.cursor);
    QVERIFY(!next.truncated);
}

void AgentConsoleTest::startsAgainWithEachDocument()
{
    AgentConsole console;
    console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Error,
        QStringLiteral("old page"), {}, 1);
    const auto before = console.read(QStringLiteral("t"), AgentConsole::Info, 0);
    console.record(QStringLiteral("t"), QStringLiteral("2"), AgentConsole::Error,
        QStringLiteral("new page"), {}, 1);
    QCOMPARE(texts(console.read(QStringLiteral("t"), AgentConsole::Info, 0).messages),
        QStringList {QStringLiteral("new page")});
    // A cursor from the last document still answers with only what is newer.
    const auto after = console.read(QStringLiteral("t"), AgentConsole::Info, before.cursor);
    QCOMPARE(texts(after.messages), QStringList {QStringLiteral("new page")});
    QVERIFY(!after.truncated);

    // A page built again numbers its documents from the start, so the page
    // area names the engine as well, and the same number from another engine
    // is another document.
    console.record(QStringLiteral("t"), QStringLiteral("1:1"), AgentConsole::Error,
        QStringLiteral("first engine"), {}, 1);
    console.record(QStringLiteral("t"), QStringLiteral("2:1"), AgentConsole::Error,
        QStringLiteral("rebuilt"), {}, 1);
    QCOMPARE(texts(console.read(QStringLiteral("t"), AgentConsole::Info, 0).messages),
        QStringList {QStringLiteral("rebuilt")});

    // A document that says nothing still starts again, and saying so twice,
    // or for a tab with nothing kept, changes nothing.
    console.start(QStringLiteral("t"), QStringLiteral("2:2"));
    QVERIFY(console.read(QStringLiteral("t"), AgentConsole::Info, 0).messages.isEmpty());
    console.record(QStringLiteral("t"), QStringLiteral("2:2"), AgentConsole::Error,
        QStringLiteral("quiet page"), {}, 1);
    console.start(QStringLiteral("t"), QStringLiteral("2:2"));
    console.start(QStringLiteral("other"), QStringLiteral("1:1"));
    QCOMPARE(texts(console.read(QStringLiteral("t"), AgentConsole::Info, 0).messages),
        QStringList {QStringLiteral("quiet page")});
    QVERIFY(console.read(QStringLiteral("other"), AgentConsole::Info, 0).messages.isEmpty());
}

void AgentConsoleTest::keepsOnlyTheNewestMessages()
{
    AgentConsole console;
    const auto overflow = 20;
    for (qsizetype index = 0; index < AgentConsole::maximumMessages + overflow; ++index) {
        console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Info,
            QString::number(index), {}, static_cast<int>(index));
    }
    const auto reading = console.read(QStringLiteral("t"), AgentConsole::Info, 0);
    QCOMPARE(reading.messages.size(), AgentConsole::maximumMessages);
    QCOMPARE(reading.messages.constFirst().text, QString::number(overflow));
    QVERIFY(reading.truncated);
    QVERIFY(!console.read(QStringLiteral("t"), AgentConsole::Info, reading.cursor).truncated);

    console.record(QStringLiteral("t"), QStringLiteral("1"), AgentConsole::Error,
        QString(AgentConsole::maximumMessageLength * 2, u'x'), {}, 0);
    const auto long_ = console.read(QStringLiteral("t"), AgentConsole::Error, 0);
    QCOMPARE(long_.messages.constFirst().text.size(), AgentConsole::maximumMessageLength);
}

void AgentConsoleTest::readsOnlyTheLevelsItNames()
{
    auto level = AgentConsole::Error;
    QVERIFY(AgentConsole::parseThreshold({}, &level));
    QCOMPARE(level, AgentConsole::Info);
    QVERIFY(AgentConsole::parseThreshold(QStringLiteral("warning"), &level));
    QCOMPARE(level, AgentConsole::Warning);
    QVERIFY(AgentConsole::parseThreshold(QStringLiteral("error"), &level));
    QCOMPARE(level, AgentConsole::Error);
    QVERIFY(!AgentConsole::parseThreshold(QStringLiteral("info"), &level));
    QCOMPARE(AgentConsole::levelName(AgentConsole::Warning), QStringLiteral("warning"));

    AgentConsole console;
    console.record(
        QStringLiteral("t"), QStringLiteral("1"), 7, QStringLiteral("unknown level"), {}, 0);
    QCOMPARE(console.read(QStringLiteral("t"), AgentConsole::Info, 0).messages.constFirst().level,
        AgentConsole::Info);
}

void AgentConsoleTest::answersAnAgentTabsConsole()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    const auto ask = [&control](QJsonObject fields) {
        fields.insert(QStringLiteral("name"), QStringLiteral("agent"));
        return control.answer(fields);
    };
    QVERIFY(ask({{QStringLiteral("verb"), QStringLiteral("space new")}})
            .value(QStringLiteral("ok"))
            .toBool());
    const auto opened = ask({{QStringLiteral("verb"), QStringLiteral("open")},
        {QStringLiteral("url"), QStringLiteral("http://localhost:3000/")}});
    const auto tabId
        = opened.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
    QVERIFY(control.agentTabIds().contains(tabId));

    control.recordConsoleMessage(tabId, QStringLiteral("1"), 0, QStringLiteral("booting"),
        QStringLiteral("http://localhost:3000/app.js"), 1);
    control.recordConsoleMessage(tabId, QStringLiteral("1"), 2,
        QStringLiteral("Uncaught TypeError: x is undefined"),
        QStringLiteral("http://localhost:3000/app.js"), 42);
    control.recordConsoleMessage(tabId, QStringLiteral("1"), 1, QStringLiteral("deprecated"),
        QStringLiteral("http://localhost:3000/app.js"), 50);

    const auto errors = ask({{QStringLiteral("verb"), QStringLiteral("console")},
        {QStringLiteral("level"), QStringLiteral("error")}});
    QVERIFY(errors.value(QStringLiteral("ok")).toBool());
    QCOMPARE(texts(errors), QStringList {QStringLiteral("Uncaught TypeError: x is undefined")});
    const auto error = errors.value(QStringLiteral("messages")).toArray().at(0).toObject();
    QCOMPARE(error.value(QStringLiteral("level")).toString(), QStringLiteral("error"));
    QCOMPARE(error.value(QStringLiteral("source")).toString(),
        QStringLiteral("http://localhost:3000/app.js"));
    QCOMPARE(error.value(QStringLiteral("line")).toInt(), 42);
    QCOMPARE(errors.value(QStringLiteral("tab")).toString(), tabId);

    const auto cursor = errors.value(QStringLiteral("cursor"));
    control.recordConsoleMessage(tabId, QStringLiteral("1"), 1, QStringLiteral("after"), {}, 0);
    const auto newer = ask(
        {{QStringLiteral("verb"), QStringLiteral("console")}, {QStringLiteral("since"), cursor}});
    QCOMPARE(texts(newer), QStringList {QStringLiteral("after")});

    // The page goes on to a document that says nothing: the last one's lines
    // are not its.
    control.startConsoleDocument(tabId, QStringLiteral("2"));
    QVERIFY(texts(ask({{QStringLiteral("verb"), QStringLiteral("console")}})).isEmpty());

    QCOMPARE(code(ask({{QStringLiteral("verb"), QStringLiteral("console")},
                 {QStringLiteral("level"), QStringLiteral("loud")}})),
        QStringLiteral("bad-request"));
    QCOMPARE(code(ask({{QStringLiteral("verb"), QStringLiteral("console")},
                 {QStringLiteral("since"), QStringLiteral("soon")}})),
        QStringLiteral("bad-request"));
}

void AgentConsoleTest::listensOnlyToAgentTabs()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    const auto spaceId = browser->createAgentSpace(QStringLiteral("Checks"), QStringLiteral("a"));
    const auto tabId = browser->openTabInSpace(spaceId, QUrl(QStringLiteral("https://a.example/")));

    // Not yet an Agent tab: nothing it says is kept.
    control.recordConsoleMessage(tabId, QStringLiteral("1"), 2, QStringLiteral("before"), {}, 0);
    const auto answer = control.answer({{QStringLiteral("verb"), QStringLiteral("console")},
        {QStringLiteral("name"), QStringLiteral("agent")}, {QStringLiteral("tab"), tabId}});
    QVERIFY(answer.value(QStringLiteral("ok")).toBool());
    QVERIFY(texts(answer).isEmpty());
    // Reading its console made it one.
    QVERIFY(control.agentTabIds().contains(tabId));
    control.recordConsoleMessage(tabId, QStringLiteral("1"), 2, QStringLiteral("after"), {}, 0);
    QCOMPARE(texts(control.answer({{QStringLiteral("verb"), QStringLiteral("console")},
                 {QStringLiteral("name"), QStringLiteral("agent")}})),
        QStringList {QStringLiteral("after")});

    control.recordConsoleMessage(
        QStringLiteral("personal-tab"), QStringLiteral("1"), 2, QStringLiteral("reader"), {}, 0);
    QCOMPARE(code(control.answer({{QStringLiteral("verb"), QStringLiteral("console")},
                 {QStringLiteral("name"), QStringLiteral("agent")},
                 {QStringLiteral("tab"), QStringLiteral("personal-tab")}})),
        QStringLiteral("refused"));
}

void AgentConsoleTest::waitsForAllowAgentsAndAnAgentSpace()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    QVERIFY(AgentControl::gated(QStringLiteral("console")));
    QVERIFY(!AgentControl::pageVerb(QStringLiteral("console")));

    const auto off = control.answer({{QStringLiteral("verb"), QStringLiteral("console")},
        {QStringLiteral("name"), QStringLiteral("agent")},
        {QStringLiteral("tab"), QStringLiteral("personal-tab")}});
    QCOMPARE(code(off), QStringLiteral("allow-agents"));

    control.setAllowAgents(true);
    QCOMPARE(code(control.answer({{QStringLiteral("verb"), QStringLiteral("console")},
                 {QStringLiteral("name"), QStringLiteral("agent")}})),
        QStringLiteral("no-current-tab"));
    // The reader's own tab, even one an Agent opened, is the reader's page.
    const auto opened = control.answer({{QStringLiteral("verb"), QStringLiteral("open")},
        {QStringLiteral("name"), QStringLiteral("agent")},
        {QStringLiteral("url"), QStringLiteral("https://a.example/")}});
    QVERIFY(opened.value(QStringLiteral("ok")).toBool());
    QCOMPARE(code(control.answer({{QStringLiteral("verb"), QStringLiteral("console")},
                 {QStringLiteral("name"), QStringLiteral("agent")}})),
        QStringLiteral("refused"));
}

void AgentConsoleTest::forgetsATabThatIsNoLongerAnAgents()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    control.setAttachmentIdleMs(50);
    const auto spaceId = browser->createAgentSpace(QStringLiteral("Checks"), QStringLiteral("a"));
    const auto opened = control.answer({{QStringLiteral("verb"), QStringLiteral("open")},
        {QStringLiteral("name"), QStringLiteral("agent")},
        {QStringLiteral("url"), QStringLiteral("https://a.example/")},
        {QStringLiteral("space"), spaceId}});
    const auto tabId
        = opened.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
    const QJsonObject read {{QStringLiteral("verb"), QStringLiteral("console")},
        {QStringLiteral("name"), QStringLiteral("agent")}};

    control.recordConsoleMessage(tabId, QStringLiteral("1"), 2, QStringLiteral("kept"), {}, 0);
    QTRY_VERIFY(!control.agentTabIds().contains(tabId));
    QVERIFY(texts(control.answer(read)).isEmpty());

    control.recordConsoleMessage(
        tabId, QStringLiteral("1"), 2, QStringLiteral("before off"), {}, 0);
    control.setAllowAgents(false);
    control.setAllowAgents(true);
    QVERIFY(texts(control.answer(read)).isEmpty());
}

QTEST_GUILESS_MAIN(AgentConsoleTest)
#include "tst_agentconsole.moc"
