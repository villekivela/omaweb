#include "AgentActivityLog.h"
#include "AgentControl.h"
#include "BrowserController.h"
#include "SessionFixture.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::AgentActivityLog;
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
            .activeTabId = QStringLiteral("personal-tab"),
        }},
        .activeSpaceId = QStringLiteral("personal"),
    };
}

QJsonObject ask(AgentControl &control, const QString &verb, QJsonObject fields = {})
{
    fields.insert(QStringLiteral("name"), QStringLiteral("claude"));
    fields.insert(QStringLiteral("verb"), verb);
    QJsonObject answered;
    control.handle(fields, [&answered](const QJsonObject &answer) { answered = answer; });
    return answered;
}

QByteArray contents(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
}

} // namespace

class AgentActivityLogTest final : public QObject {
    Q_OBJECT

private slots:
    void logsTheLabelAFillStepTypedIntoAndNeverTheValue();
    void keepsWhatAnAgentTypedOutOfEveryLine();
    void forgetsWhatIsOlderThanAWeekAtTheNextStart();
    void keepsTheActivityPageFromAgents();
    void keepsTheActivityPageOutOfSplits();
};

void AgentActivityLogTest::logsTheLabelAFillStepTypedIntoAndNeverTheValue()
{
    QTemporaryDir config;
    QTemporaryDir data;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentActivityLog log(data.path());
    AgentControl control(browser.get(), config.path());
    control.setActivityLog(&log);
    control.setAllowAgents(true);
    // The page answers every verb at once, as a page that did what it was
    // asked would.
    connect(&control, &AgentControl::pageRequested, this,
        [&control](int requestId, const QVariantMap &) {
            control.answerPage(requestId, {{QStringLiteral("ok"), true}});
        });

    const auto space = ask(control, QStringLiteral("space new"),
        {{QStringLiteral("space"), QStringLiteral("Research")}})
                           .value(QStringLiteral("space"))
                           .toObject()
                           .value(QStringLiteral("id"))
                           .toString();
    QVERIFY(!space.isEmpty());
    const auto opened = ask(control, QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("https://shop.example/login")},
            {QStringLiteral("space"), space}});
    const auto tab = opened.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id"));
    QVERIFY(!tab.toString().isEmpty());

    const auto secret = QStringLiteral("correct horse battery staple");
    const auto done = ask(control, QStringLiteral("do"),
        {{QStringLiteral("steps"),
            QJsonArray {QJsonObject {{QStringLiteral("action"), QStringLiteral("fill")},
                            {QStringLiteral("target"), QStringLiteral("7")},
                            {QStringLiteral("text"), secret}},
                QJsonObject {{QStringLiteral("action"), QStringLiteral("click")},
                    {QStringLiteral("target"), QStringLiteral("9")}}}}});
    QVERIFY(done.value(QStringLiteral("ok")).toBool());

    const auto entries = log.entries();
    QCOMPARE(entries.size(), 3);
    const auto &step = entries.constLast();
    QCOMPARE(step.agent, QStringLiteral("claude"));
    QCOMPARE(step.verb, QStringLiteral("do"));
    QCOMPARE(step.target, QStringLiteral("fill 7, click 9"));
    QCOMPARE(step.tabId, tab.toString());
    QCOMPARE(step.address, QStringLiteral("https://shop.example/login"));
    QCOMPARE(step.spaceId, space);
    QCOMPARE(step.space, QStringLiteral("Research"));
    QCOMPARE(step.outcome, QStringLiteral("ok"));

    const auto written = contents(log.path());
    QVERIFY(written.contains("fill 7"));
    QVERIFY(!written.contains(secret.toUtf8()));
    QVERIFY(!written.contains("horse"));
}

// A value can reach a line other than through `fill`: as the option a step
// chose, a key, the text it waited for, a selector, an expression, a target
// that is not a label, or the query of an address a form was sent to.
void AgentActivityLogTest::keepsWhatAnAgentTypedOutOfEveryLine()
{
    QTemporaryDir config;
    QTemporaryDir data;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentActivityLog log(data.path());
    AgentControl control(browser.get(), config.path());
    control.setActivityLog(&log);
    control.setAllowAgents(true);
    connect(&control, &AgentControl::pageRequested, this,
        [&control](int requestId, const QVariantMap &) {
            control.answerPage(requestId, {{QStringLiteral("ok"), true}});
        });

    const auto space = ask(control, QStringLiteral("space new"))
                           .value(QStringLiteral("space"))
                           .toObject()
                           .value(QStringLiteral("id"))
                           .toString();
    ask(control, QStringLiteral("open"),
        {{QStringLiteral("url"),
             QStringLiteral("https://shop.example/search?q=secret-query#secret-fragment")},
            {QStringLiteral("space"), space}});
    const auto step = [](const QString &action, QJsonObject fields) {
        fields.insert(QStringLiteral("action"), action);
        return fields;
    };
    ask(control, QStringLiteral("do"),
        {{QStringLiteral("steps"),
            QJsonArray {step(QStringLiteral("select"),
                            {{QStringLiteral("target"), QStringLiteral("4")},
                                {QStringLiteral("text"), QStringLiteral("secret-option")}}),
                step(QStringLiteral("press"),
                    {{QStringLiteral("key"), QStringLiteral("secret-key")}}),
                step(QStringLiteral("wait"),
                    {{QStringLiteral("text"), QStringLiteral("secret-wait")}}),
                step(QStringLiteral("click"),
                    {{QStringLiteral("target"), QStringLiteral("secret-target")}})}}});
    ask(control, QStringLiteral("read"),
        {{QStringLiteral("selector"), QStringLiteral("#secret-selector")}});
    ask(control, QStringLiteral("eval"),
        {{QStringLiteral("expression"), QStringLiteral("secretExpression()")}});
    ask(control, QStringLiteral("open"),
        {{QStringLiteral("url"), QStringLiteral("javascript:secretScript()")},
            {QStringLiteral("space"), space}});

    const auto written = contents(log.path());
    QVERIFY(!written.isEmpty());
    QVERIFY2(!written.contains("secret"), written.constData());
    const auto &entries = log.entries();
    QCOMPARE(entries.at(1).target, QStringLiteral("https://shop.example/search"));
    QCOMPARE(entries.at(2).target, QStringLiteral("select 4, press, wait, click"));
    QCOMPARE(entries.at(2).address, QStringLiteral("https://shop.example/search"));
    QCOMPARE(entries.constLast().outcome, QStringLiteral("refused"));
}

void AgentActivityLogTest::forgetsWhatIsOlderThanAWeekAtTheNextStart()
{
    QTemporaryDir data;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    const auto day = 24LL * 60 * 60 * 1000;
    {
        AgentActivityLog log(data.path());
        log.record({.time = now - 8 * day,
            .agent = QStringLiteral("old"),
            .verb = QStringLiteral("look"),
            .outcome = QStringLiteral("ok")});
        log.record({.time = now - 6 * day,
            .agent = QStringLiteral("recent"),
            .verb = QStringLiteral("look"),
            .outcome = QStringLiteral("ok")});
    }
    QFile file(QDir(data.path()).filePath(QStringLiteral("agent-activity.jsonl")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append));
    file.write(QStringLiteral(R"({"time":%1,"agent":"ancient","verb":"read","outcome":"ok"})")
                   .arg(now - 30 * day)
                   .toUtf8()
        + '\n');
    file.close();

    AgentActivityLog restarted(data.path());
    QCOMPARE(restarted.entries().size(), 1);
    QCOMPARE(restarted.entries().constFirst().agent, QStringLiteral("recent"));
    const auto written = contents(restarted.path());
    QVERIFY(written.contains("recent"));
    QVERIFY(!written.contains("\"old\""));
    QVERIFY(!written.contains("ancient"));
}

// The page is the reader's account of what Agents did, so no Agent reads it,
// even in an Agent Space.
void AgentActivityLogTest::keepsTheActivityPageFromAgents()
{
    QTemporaryDir config;
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    AgentControl control(browser.get(), config.path());
    control.setAllowAgents(true);
    QSignalSpy requested(&control, &AgentControl::pageRequested);
    connect(&control, &AgentControl::pageRequested, this, [] { });

    const auto space = ask(control, QStringLiteral("space new"))
                           .value(QStringLiteral("space"))
                           .toObject()
                           .value(QStringLiteral("id"))
                           .toString();
    QVERIFY(browser->switchSpace(space));
    QVERIFY(browser->openAgentActivity());
    const auto page = browser->activeTabId();

    for (const auto &verb : {QStringLiteral("look"), QStringLiteral("read"), QStringLiteral("do"),
             QStringLiteral("shot"), QStringLiteral("eval"), QStringLiteral("console")}) {
        const auto answer = ask(control, verb,
            {{QStringLiteral("tab"), page}, {QStringLiteral("expression"), QStringLiteral("1")},
                {QStringLiteral("steps"),
                    QJsonArray {
                        QJsonObject {{QStringLiteral("action"), QStringLiteral("back")}}}}});
        QCOMPARE(answer.value(QStringLiteral("code")).toString(), QStringLiteral("refused"));
    }
    QCOMPARE(requested.count(), 0);
}

// The page is drawn over the whole page area, so it takes no pane of a split.
void AgentActivityLogTest::keepsTheActivityPageOutOfSplits()
{
    SessionFixture fixture(readersSession());
    QVERIFY_SESSION_READY(fixture);
    const auto browser = fixture.createController();
    QVERIFY(browser->openAgentActivity());
    const auto page = browser->activeTabId();
    QVERIFY(!browser->addSplit());
    QVERIFY(!browser->addSplit(QStringLiteral("personal-tab")));

    browser->activateTab(QStringLiteral("personal-tab"));
    QVERIFY(!browser->splittableTabIds().contains(page));
    QVERIFY(!browser->addSplit(page));
    QVERIFY(!browser->splitOnShow());
}

QTEST_GUILESS_MAIN(AgentActivityLogTest)
#include "tst_agentactivitylog.moc"
