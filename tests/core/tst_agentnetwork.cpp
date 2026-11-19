#include "AgentActivityLog.h"
#include "AgentControl.h"
#include "AgentNetwork.h"
#include "BrowserController.h"
#include "SessionFixture.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::AgentActivityLog;
using omaweb::AgentControl;
using omaweb::AgentNetwork;
using omaweb::BrowserController;
using omaweb::test::SessionFixture;
using omaweb::test::SessionSpec;
using omaweb::test::SpaceSpec;
using omaweb::test::TabSpec;

namespace {

using Header = AgentNetwork::Header;

QVariantMap began(const QString &key, const QString &document, const QString &url,
    const QString &method = QStringLiteral("GET"))
{
    return {
        {QStringLiteral("key"), key},
        {QStringLiteral("document"), document},
        {QStringLiteral("method"), method},
        {QStringLiteral("url"), url},
        {QStringLiteral("type"), QStringLiteral("Fetch")},
    };
}

QStringList urls(const QVector<AgentNetwork::Request> &requests)
{
    QStringList listed;
    for (const auto &request : requests) {
        listed.append(request.url);
    }
    return listed;
}

QVariantList pairs(const QVector<Header> &headers)
{
    QVariantList list;
    for (const auto &header : headers) {
        list.append(QVariant(QVariantList {header.name, header.value}));
    }
    return list;
}

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

QString code(const QJsonObject &answer) { return answer.value(QStringLiteral("code")).toString(); }

QStringList listedUrls(const QJsonObject &answer)
{
    QStringList listed;
    for (const auto &request : answer.value(QStringLiteral("requests")).toArray()) {
        listed.append(request.toObject().value(QStringLiteral("url")).toString());
    }
    return listed;
}

// A browser with Allow agents on, whose engine offers the session or not.
struct Browser {
    explicit Browser(bool inspection = true)
        : fixture(readersSession(), config.path())
    {
        browser = fixture.createController();
        control = std::make_unique<AgentControl>(browser.get(), config.path());
        control->setAllowAgents(true);
        control->setNetworkInspection(inspection);
    }

    QJsonObject ask(QJsonObject fields, const QString &name = QStringLiteral("agent"))
    {
        fields.insert(QStringLiteral("name"), name);
        return control->answer(fields);
    }

    // A tab of a new Agent Space, made the connection's current tab.
    QString agentTab(const QString &url = QStringLiteral("http://localhost:3000/"))
    {
        const auto spaceId
            = browser->createAgentSpace(QStringLiteral("Checks"), QStringLiteral("agent"));
        const auto opened = ask({{QStringLiteral("verb"), QStringLiteral("open")},
            {QStringLiteral("url"), url}, {QStringLiteral("space"), spaceId}});
        return opened.value(QStringLiteral("tab"))
            .toObject()
            .value(QStringLiteral("id"))
            .toString();
    }

    // The reader's own tab, in a Space they granted, made the connection's
    // current tab.
    QString grantedTab()
    {
        const auto asked = ask({{QStringLiteral("verb"), QStringLiteral("network")},
            {QStringLiteral("tab"), QStringLiteral("personal-tab")}});
        if (code(asked) == u"pending") {
            control->answerGrant(QStringLiteral("personal"), true);
        }
        ask({{QStringLiteral("verb"), QStringLiteral("network")},
            {QStringLiteral("tab"), QStringLiteral("personal-tab")}});
        return QStringLiteral("personal-tab");
    }

    // The id `network` gives the newest request of the current tab.
    double newestId()
    {
        const auto requests = ask({{QStringLiteral("verb"), QStringLiteral("network")}})
                                  .value(QStringLiteral("requests"))
                                  .toArray();
        return requests.isEmpty()
            ? 0
            : requests.last().toObject().value(QStringLiteral("id")).toDouble();
    }

    QTemporaryDir config;
    SessionFixture fixture;
    std::unique_ptr<BrowserController> browser;
    std::unique_ptr<AgentControl> control;
};

QVariantMap credentialed(const QString &key, const QString &document)
{
    auto report = began(key, document, QStringLiteral("https://api.example/me"));
    report.insert(QStringLiteral("requestHeaders"),
        pairs({{QStringLiteral("Cookie"), QStringLiteral("a=1; b=2; c=3")},
            {QStringLiteral("Authorization"), QStringLiteral("Bearer secret-token")},
            {QStringLiteral("Proxy-Authorization"), QStringLiteral("Basic cHJveHk6cGFzcw==")},
            {QStringLiteral("Accept"), QStringLiteral("application/json")}}));
    report.insert(QStringLiteral("responseHeaders"),
        pairs({{QStringLiteral("Set-Cookie"), QStringLiteral("s=1; HttpOnly\nt=2; Secure")},
            {QStringLiteral("Content-Type"), QStringLiteral("application/json")}}));
    report.insert(QStringLiteral("status"), 200);
    report.insert(QStringLiteral("sentBody"), true);
    report.insert(QStringLiteral("body"),
        QVariantMap {{QStringLiteral("requestId"), QStringLiteral("engine-1")}});
    return report;
}

// The value a header of `request`'s answer shows, from its request or its
// response.
QString headerShown(const QJsonObject &answer, const QString &name)
{
    const auto request = answer.value(QStringLiteral("request")).toObject();
    for (const auto *side : {"requestHeaders", "responseHeaders"}) {
        for (const auto &pair : request.value(QLatin1StringView(side)).toArray()) {
            if (pair.toArray().at(0).toString() == name) {
                return pair.toArray().at(1).toString();
            }
        }
    }
    return {};
}

} // namespace

class AgentNetworkTest final : public QObject {
    Q_OBJECT

private slots:
    void numbersARequestOnceAndAddsWhatFollows();
    void answersOnlyRequestsThatBeganAfterTheCursor();
    void startsAgainWithEachDocument();
    void keepsOnlyTheNewestRequests();
    void listsTheFailedOnesWhenAsked();
    void redactsEachCredentialHeader_data();
    void redactsEachCredentialHeader();
    void leavesOtherHeadersAsTheyAre();
    void refusesOnAnEngineWithoutTheSession();
    void answersAnAgentTabsNetwork();
    void listensOnlyToAgentTabs();
    void waitsForAllowAgentsAndAGrant();
    void credentialsAreRedactedInAGrantedSpace_data();
    void credentialsAreRedactedInAGrantedSpace();
    void credentialsAreShownInAnAgentSpace_data();
    void credentialsAreShownInAnAgentSpace();
    void refusesARequestBodyOutsideAnAgentSpace();
    void writesAResponseBodyToTheConnectionsDirectory();
    void cutsAResponseBodyAtTheLimit();
    void dropsTheRequestsWhenTheAgentDetaches();
    void dropsTheRequestsWhenAllowAgentsGoesOff();
    void dropsTheRequestsWhenTheTabCloses();
    void logsNoAddressHeaderOrBody();
};

void AgentNetworkTest::numbersARequestOnceAndAddsWhatFollows()
{
    AgentNetwork network;
    auto first
        = began(QStringLiteral("a"), QStringLiteral("1"), QStringLiteral("https://a.example/"));
    first.insert(QStringLiteral("requestHeaders"),
        pairs({{QStringLiteral("Accept"), QStringLiteral("*/*")}}));
    network.record(QStringLiteral("t"), first);
    network.record(QStringLiteral("t"),
        {{QStringLiteral("key"), QStringLiteral("a")},
            {QStringLiteral("document"), QStringLiteral("1")}, {QStringLiteral("status"), 404},
            {QStringLiteral("statusText"), QStringLiteral("Not Found")},
            {QStringLiteral("responseHeaders"),
                pairs({{QStringLiteral("Content-Type"), QStringLiteral("text/plain")}})}});
    network.record(QStringLiteral("t"),
        {{QStringLiteral("key"), QStringLiteral("a")},
            {QStringLiteral("document"), QStringLiteral("1")}, {QStringLiteral("size"), 1234}});

    const auto reading = network.read(QStringLiteral("t"), false, 0);
    QCOMPARE(reading.requests.size(), 1);
    const auto request = reading.requests.constFirst();
    QVERIFY(request.id > 0);
    QCOMPARE(reading.cursor, request.id);
    QCOMPARE(request.method, QStringLiteral("GET"));
    QCOMPARE(request.url, QStringLiteral("https://a.example/"));
    QCOMPARE(request.type, QStringLiteral("Fetch"));
    QCOMPARE(request.status, 404);
    QCOMPARE(request.statusText, QStringLiteral("Not Found"));
    QCOMPARE(request.size, 1234);
    QCOMPARE(request.requestHeaders,
        QVector<Header>({{QStringLiteral("Accept"), QStringLiteral("*/*")}}));
    QCOMPARE(request.responseHeaders,
        QVector<Header>({{QStringLiteral("Content-Type"), QStringLiteral("text/plain")}}));

    const auto found = network.find(QStringLiteral("t"), request.id);
    QVERIFY(found);
    QCOMPARE(found->url, request.url);
    QVERIFY(!network.find(QStringLiteral("other"), request.id));
    QVERIFY(!network.find(QStringLiteral("t"), request.id + 1));
}

void AgentNetworkTest::answersOnlyRequestsThatBeganAfterTheCursor()
{
    AgentNetwork network;
    network.record(QStringLiteral("t"),
        began(QStringLiteral("a"), QStringLiteral("1"), QStringLiteral("https://a.example/one")));
    const auto first = network.read(QStringLiteral("t"), false, 0);
    QCOMPARE(urls(first.requests), QStringList {QStringLiteral("https://a.example/one")});

    // What arrives later about a request already read does not list it again.
    network.record(QStringLiteral("t"),
        {{QStringLiteral("key"), QStringLiteral("a")},
            {QStringLiteral("document"), QStringLiteral("1")}, {QStringLiteral("status"), 200}});
    const auto nothingNew = network.read(QStringLiteral("t"), false, first.cursor);
    QVERIFY(nothingNew.requests.isEmpty());
    QCOMPARE(nothingNew.cursor, first.cursor);

    network.record(QStringLiteral("t"),
        began(QStringLiteral("b"), QStringLiteral("1"), QStringLiteral("https://a.example/two")));
    const auto next = network.read(QStringLiteral("t"), false, first.cursor);
    QCOMPARE(urls(next.requests), QStringList {QStringLiteral("https://a.example/two")});
    QVERIFY(next.cursor > first.cursor);
    QVERIFY(!next.truncated);
}

void AgentNetworkTest::startsAgainWithEachDocument()
{
    AgentNetwork network;
    network.record(QStringLiteral("t"),
        began(QStringLiteral("a"), QStringLiteral("1"), QStringLiteral("https://a.example/old")));
    const auto before = network.read(QStringLiteral("t"), false, 0);
    network.record(QStringLiteral("t"),
        began(QStringLiteral("b"), QStringLiteral("2"), QStringLiteral("https://a.example/new")));
    QCOMPARE(urls(network.read(QStringLiteral("t"), false, 0).requests),
        QStringList {QStringLiteral("https://a.example/new")});
    QVERIFY(!network.find(QStringLiteral("t"), before.cursor));
    const auto after = network.read(QStringLiteral("t"), false, before.cursor);
    QCOMPARE(urls(after.requests), QStringList {QStringLiteral("https://a.example/new")});
    QVERIFY(!after.truncated);

    // News of a request of the last document, which arrives after the next
    // has begun, is not the new document's.
    network.record(QStringLiteral("t"),
        {{QStringLiteral("key"), QStringLiteral("a")},
            {QStringLiteral("document"), QStringLiteral("1")}, {QStringLiteral("status"), 200}});
    QCOMPARE(urls(network.read(QStringLiteral("t"), false, 0).requests),
        QStringList {QStringLiteral("https://a.example/new")});
}

void AgentNetworkTest::keepsOnlyTheNewestRequests()
{
    AgentNetwork network;
    const auto overflow = 20;
    for (qsizetype index = 0; index < AgentNetwork::maximumRequests + overflow; ++index) {
        network.record(QStringLiteral("t"),
            began(QString::number(index), QStringLiteral("1"),
                QStringLiteral("https://a.example/%1").arg(index)));
    }
    const auto reading = network.read(QStringLiteral("t"), false, 0);
    QCOMPARE(reading.requests.size(), AgentNetwork::maximumRequests);
    QCOMPARE(
        reading.requests.constFirst().url, QStringLiteral("https://a.example/%1").arg(overflow));
    QVERIFY(reading.truncated);
    QVERIFY(!network.read(QStringLiteral("t"), false, reading.cursor).truncated);
    // A cursor from before the oldest kept, but after the last one dropped,
    // missed nothing.
    QVERIFY(
        !network.read(QStringLiteral("t"), false, reading.requests.constFirst().id - 1).truncated);
    QVERIFY(
        network.read(QStringLiteral("t"), false, reading.requests.constFirst().id - 2).truncated);
}

void AgentNetworkTest::listsTheFailedOnesWhenAsked()
{
    AgentNetwork network;
    for (const auto &[key, status, failure] : {std::tuple {QStringLiteral("ok"), 200, QString {}},
             std::tuple {QStringLiteral("redirected"), 302, QString {}},
             std::tuple {QStringLiteral("missing"), 404, QString {}},
             std::tuple {QStringLiteral("broken"), 503, QString {}},
             std::tuple {
                 QStringLiteral("refused"), 0, QStringLiteral("net::ERR_CONNECTION_REFUSED")},
             std::tuple {QStringLiteral("waiting"), 0, QString {}}}) {
        auto report = began(key, QStringLiteral("1"), QStringLiteral("https://a.example/") + key);
        report.insert(QStringLiteral("status"), status);
        if (!failure.isEmpty()) {
            report.insert(QStringLiteral("failure"), failure);
        }
        network.record(QStringLiteral("t"), report);
    }
    QCOMPARE(urls(network.read(QStringLiteral("t"), true, 0).requests),
        QStringList({QStringLiteral("https://a.example/missing"),
            QStringLiteral("https://a.example/broken"),
            QStringLiteral("https://a.example/refused")}));
    // The cursor is the newest request kept, failed or not.
    QCOMPARE(network.read(QStringLiteral("t"), true, 0).cursor,
        network.read(QStringLiteral("t"), false, 0).cursor);
}

void AgentNetworkTest::redactsEachCredentialHeader_data()
{
    QTest::addColumn<Header>("header");
    QTest::addColumn<QString>("shown");

    QTest::newRow("Cookie") << Header {QStringLiteral("Cookie"), QStringLiteral("a=1; b=2; c=3")}
                            << QStringLiteral("<redacted, 3 values>");
    QTest::newRow("Set-Cookie") << Header {QStringLiteral("set-cookie"),
        QStringLiteral("a=1; Path=/; HttpOnly\nb=2; Secure")}
                                << QStringLiteral("<redacted, 2 values>");
    QTest::newRow("Authorization")
        << Header {QStringLiteral("Authorization"), QStringLiteral("Bearer secret")}
        << QStringLiteral("<redacted, 1 value>");
    QTest::newRow("Proxy-Authorization")
        << Header {QStringLiteral("PROXY-AUTHORIZATION"), QStringLiteral("Basic dXNlcjpwYXNz")}
        << QStringLiteral("<redacted, 1 value>");
}

void AgentNetworkTest::redactsEachCredentialHeader()
{
    QFETCH(Header, header);
    QFETCH(QString, shown);
    const auto redacted = AgentNetwork::redacted({header});
    QCOMPARE(redacted, QVector<Header>({{header.name, shown}}));
}

void AgentNetworkTest::leavesOtherHeadersAsTheyAre()
{
    const QVector<Header> headers {
        {QStringLiteral("Content-Type"), QStringLiteral("text/html")},
        {QStringLiteral("X-Cookie-Note"), QStringLiteral("not a cookie")},
    };
    QCOMPARE(AgentNetwork::redacted(headers), headers);
}

void AgentNetworkTest::refusesOnAnEngineWithoutTheSession()
{
    Browser without(false);
    without.agentTab();
    QCOMPARE(code(without.ask({{QStringLiteral("verb"), QStringLiteral("network")}})),
        QStringLiteral("unsupported"));
    QCOMPARE(code(without.ask(
                 {{QStringLiteral("verb"), QStringLiteral("request")}, {QStringLiteral("id"), 1}})),
        QStringLiteral("unsupported"));
}

void AgentNetworkTest::answersAnAgentTabsNetwork()
{
    Browser browser;
    const auto tabId = browser.agentTab();
    QVERIFY(browser.control->agentTabIds().contains(tabId));

    auto page = began(
        QStringLiteral("doc"), QStringLiteral("1"), QStringLiteral("http://localhost:3000/"));
    page.insert(QStringLiteral("type"), QStringLiteral("Document"));
    page.insert(QStringLiteral("status"), 200);
    page.insert(QStringLiteral("size"), 5120);
    browser.control->recordNetworkRequest(tabId, page);
    auto missing = began(QStringLiteral("api"), QStringLiteral("1"),
        QStringLiteral("http://localhost:3000/api/items"), QStringLiteral("POST"));
    missing.insert(QStringLiteral("status"), 500);
    missing.insert(QStringLiteral("statusText"), QStringLiteral("Internal Server Error"));
    browser.control->recordNetworkRequest(tabId, missing);

    const auto all = browser.ask({{QStringLiteral("verb"), QStringLiteral("network")}});
    QVERIFY(all.value(QStringLiteral("ok")).toBool());
    QCOMPARE(all.value(QStringLiteral("tab")).toString(), tabId);
    QCOMPARE(listedUrls(all),
        QStringList({QStringLiteral("http://localhost:3000/"),
            QStringLiteral("http://localhost:3000/api/items")}));
    const auto first = all.value(QStringLiteral("requests")).toArray().at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("method")).toString(), QStringLiteral("GET"));
    QCOMPARE(first.value(QStringLiteral("status")).toInt(), 200);
    QCOMPARE(first.value(QStringLiteral("type")).toString(), QStringLiteral("Document"));
    QCOMPARE(first.value(QStringLiteral("size")).toInt(), 5120);
    QVERIFY(first.value(QStringLiteral("id")).toDouble() > 0);
    QVERIFY(!all.value(QStringLiteral("truncated")).toBool());

    QCOMPARE(listedUrls(browser.ask({{QStringLiteral("verb"), QStringLiteral("network")},
                 {QStringLiteral("failed"), true}})),
        QStringList {QStringLiteral("http://localhost:3000/api/items")});

    const auto cursor = all.value(QStringLiteral("cursor"));
    browser.control->recordNetworkRequest(tabId,
        began(QStringLiteral("late"), QStringLiteral("1"),
            QStringLiteral("http://localhost:3000/late")));
    QCOMPARE(listedUrls(browser.ask({{QStringLiteral("verb"), QStringLiteral("network")},
                 {QStringLiteral("since"), cursor}})),
        QStringList {QStringLiteral("http://localhost:3000/late")});

    QCOMPARE(code(browser.ask({{QStringLiteral("verb"), QStringLiteral("network")},
                 {QStringLiteral("since"), QStringLiteral("soon")}})),
        QStringLiteral("bad-request"));
    QCOMPARE(code(browser.ask({{QStringLiteral("verb"), QStringLiteral("request")},
                 {QStringLiteral("id"), 999999}})),
        QStringLiteral("not-found"));
    QCOMPARE(code(browser.ask({{QStringLiteral("verb"), QStringLiteral("request")}})),
        QStringLiteral("bad-request"));
}

// Only an Agent tab is listened to, and `--since` says when its buffer dropped
// requests the reader had not yet read.
void AgentNetworkTest::listensOnlyToAgentTabs()
{
    Browser browser;
    const auto spaceId
        = browser.browser->createAgentSpace(QStringLiteral("Checks"), QStringLiteral("a"));
    const auto tabId
        = browser.browser->openTabInSpace(spaceId, QUrl(QStringLiteral("https://a.example/")));

    // Not yet an Agent tab: nothing it asks for is kept.
    browser.control->recordNetworkRequest(tabId,
        began(QStringLiteral("before"), QStringLiteral("1"),
            QStringLiteral("https://a.example/before")));
    const auto answer = browser.ask(
        {{QStringLiteral("verb"), QStringLiteral("network")}, {QStringLiteral("tab"), tabId}});
    QVERIFY(answer.value(QStringLiteral("ok")).toBool());
    QVERIFY(listedUrls(answer).isEmpty());
    // Reading its network made it one.
    QVERIFY(browser.control->agentTabIds().contains(tabId));
    browser.control->recordNetworkRequest(tabId,
        began(QStringLiteral("after"), QStringLiteral("1"),
            QStringLiteral("https://a.example/after")));
    QCOMPARE(listedUrls(browser.ask({{QStringLiteral("verb"), QStringLiteral("network")}})),
        QStringList {QStringLiteral("https://a.example/after")});

    for (qsizetype index = 0; index < AgentNetwork::maximumRequests + 1; ++index) {
        browser.control->recordNetworkRequest(tabId,
            began(QString::number(index), QStringLiteral("1"),
                QStringLiteral("https://a.example/%1").arg(index)));
    }
    const auto overflowed = browser.ask(
        {{QStringLiteral("verb"), QStringLiteral("network")}, {QStringLiteral("since"), 0}});
    QVERIFY(overflowed.value(QStringLiteral("truncated")).toBool());
}

void AgentNetworkTest::waitsForAllowAgentsAndAGrant()
{
    Browser browser;
    QVERIFY(AgentControl::gated(QStringLiteral("network")));
    QVERIFY(AgentControl::gated(QStringLiteral("request")));
    browser.control->setAllowAgents(false);
    QCOMPARE(code(browser.ask({{QStringLiteral("verb"), QStringLiteral("network")},
                 {QStringLiteral("tab"), QStringLiteral("personal-tab")}})),
        QStringLiteral("allow-agents"));
    browser.control->setAllowAgents(true);
    QCOMPARE(code(browser.ask({{QStringLiteral("verb"), QStringLiteral("network")},
                 {QStringLiteral("tab"), QStringLiteral("personal-tab")}})),
        QStringLiteral("pending"));
    QCOMPARE(browser.control->grantRequest().value(QStringLiteral("spaceId")).toString(),
        QStringLiteral("personal"));
}

void AgentNetworkTest::credentialsAreRedactedInAGrantedSpace_data()
{
    QTest::addColumn<QString>("header");
    QTest::addColumn<QString>("shown");
    QTest::newRow("Cookie") << QStringLiteral("Cookie") << QStringLiteral("<redacted, 3 values>");
    QTest::newRow("Set-Cookie") << QStringLiteral("Set-Cookie")
                                << QStringLiteral("<redacted, 2 values>");
    QTest::newRow("Authorization")
        << QStringLiteral("Authorization") << QStringLiteral("<redacted, 1 value>");
    QTest::newRow("Proxy-Authorization")
        << QStringLiteral("Proxy-Authorization") << QStringLiteral("<redacted, 1 value>");
}

void AgentNetworkTest::credentialsAreRedactedInAGrantedSpace()
{
    QFETCH(QString, header);
    QFETCH(QString, shown);
    Browser browser;
    const auto tabId = browser.grantedTab();
    browser.control->recordNetworkRequest(
        tabId, credentialed(QStringLiteral("me"), QStringLiteral("1")));
    const auto answer = browser.ask({{QStringLiteral("verb"), QStringLiteral("request")},
        {QStringLiteral("id"), browser.newestId()}});
    QVERIFY2(answer.value(QStringLiteral("ok")).toBool(), qPrintable(code(answer)));
    QCOMPARE(headerShown(answer, header), shown);
    QCOMPARE(headerShown(answer, QStringLiteral("Accept")), QStringLiteral("application/json"));
    const auto text = QString::fromUtf8(QJsonDocument(answer).toJson());
    for (const auto *secret : {"b=2", "secret-token", "cHJveHk6cGFzcw==", "HttpOnly"}) {
        QVERIFY2(!text.contains(QLatin1StringView(secret)), secret);
    }
}

void AgentNetworkTest::credentialsAreShownInAnAgentSpace_data()
{
    QTest::addColumn<QString>("header");
    QTest::addColumn<QString>("shown");
    QTest::newRow("Cookie") << QStringLiteral("Cookie") << QStringLiteral("a=1; b=2; c=3");
    QTest::newRow("Set-Cookie") << QStringLiteral("Set-Cookie")
                                << QStringLiteral("s=1; HttpOnly\nt=2; Secure");
    QTest::newRow("Authorization")
        << QStringLiteral("Authorization") << QStringLiteral("Bearer secret-token");
    QTest::newRow("Proxy-Authorization")
        << QStringLiteral("Proxy-Authorization") << QStringLiteral("Basic cHJveHk6cGFzcw==");
}

void AgentNetworkTest::credentialsAreShownInAnAgentSpace()
{
    QFETCH(QString, header);
    QFETCH(QString, shown);
    Browser browser;
    const auto tabId = browser.agentTab();
    browser.control->recordNetworkRequest(
        tabId, credentialed(QStringLiteral("me"), QStringLiteral("1")));
    const auto answer = browser.ask({{QStringLiteral("verb"), QStringLiteral("request")},
        {QStringLiteral("id"), browser.newestId()}});
    QVERIFY2(answer.value(QStringLiteral("ok")).toBool(), qPrintable(code(answer)));
    QCOMPARE(headerShown(answer, header), shown);
}

void AgentNetworkTest::refusesARequestBodyOutsideAnAgentSpace()
{
    Browser browser;
    QSignalSpy requested(browser.control.get(), &AgentControl::pageRequested);
    connect(browser.control.get(), &AgentControl::pageRequested, this, [] { });
    const auto tabId = browser.grantedTab();
    browser.control->recordNetworkRequest(
        tabId, credentialed(QStringLiteral("me"), QStringLiteral("1")));
    const auto id = browser.newestId();

    QCOMPARE(code(browser.ask({{QStringLiteral("verb"), QStringLiteral("request")},
                 {QStringLiteral("id"), id}, {QStringLiteral("sent"), true}})),
        QStringLiteral("refused"));
    QCOMPARE(requested.count(), 0);
    // What came back is the page's to read in either.
    QCOMPARE(code(browser.ask({{QStringLiteral("verb"), QStringLiteral("request")},
                 {QStringLiteral("id"), id}, {QStringLiteral("body"), true}})),
        QStringLiteral("pending"));
    QCOMPARE(requested.count(), 1);

    Browser own;
    QSignalSpy asked(own.control.get(), &AgentControl::pageRequested);
    connect(own.control.get(), &AgentControl::pageRequested, this, [] { });
    const auto agentTab = own.agentTab();
    own.control->recordNetworkRequest(
        agentTab, credentialed(QStringLiteral("me"), QStringLiteral("1")));
    QCOMPARE(code(own.ask({{QStringLiteral("verb"), QStringLiteral("request")},
                 {QStringLiteral("id"), own.newestId()}, {QStringLiteral("sent"), true}})),
        QStringLiteral("pending"));
    QCOMPARE(asked.count(), 1);
    const auto arguments = asked.at(0).at(1).toMap().value(QStringLiteral("arguments")).toMap();
    QCOMPARE(asked.at(0).at(1).toMap().value(QStringLiteral("verb")).toString(),
        QStringLiteral("request"));
    QCOMPARE(arguments.value(QStringLiteral("part")).toString(), QStringLiteral("sent"));
    QCOMPARE(arguments.value(QStringLiteral("body")).toMap().value(QStringLiteral("requestId")),
        QVariant(QStringLiteral("engine-1")));

    // A request that sent nothing has no body to read.
    own.control->recordNetworkRequest(agentTab,
        began(QStringLiteral("plain"), QStringLiteral("1"), QStringLiteral("https://a.example/")));
    QCOMPARE(code(own.ask({{QStringLiteral("verb"), QStringLiteral("request")},
                 {QStringLiteral("id"), own.newestId()}, {QStringLiteral("sent"), true}})),
        QStringLiteral("not-found"));
}

void AgentNetworkTest::writesAResponseBodyToTheConnectionsDirectory()
{
    Browser browser;
    QTemporaryDir downloads;
    QVERIFY(browser.browser->setDownloadDirectory(downloads.path()));
    connect(browser.control.get(), &AgentControl::pageRequested, this,
        [&browser](int requestId, const QVariantMap &) {
            browser.control->answerPage(requestId,
                {{QStringLiteral("ok"), true},
                    {QStringLiteral("body"),
                        QString::fromLatin1(QByteArray("{\"a\":1}").toBase64())}});
        });
    const auto tabId = browser.agentTab();
    browser.control->recordNetworkRequest(
        tabId, credentialed(QStringLiteral("me"), QStringLiteral("1")));
    const auto id = browser.newestId();

    QJsonObject written;
    browser.control->handle({{QStringLiteral("verb"), QStringLiteral("request")},
                                {QStringLiteral("name"), QStringLiteral("agent")},
                                {QStringLiteral("id"), id}, {QStringLiteral("body"), true}},
        [&written](const QJsonObject &answer) { written = answer; });
    QTRY_VERIFY(!written.isEmpty());
    QVERIFY2(written.value(QStringLiteral("ok")).toBool(), qPrintable(code(written)));
    const auto path = written.value(QStringLiteral("path")).toString();
    QCOMPARE(QFileInfo(path).absolutePath(),
        QDir(downloads.path()).filePath(QStringLiteral("Agents/agent")));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("{\"a\":1}"));
    QCOMPARE(file.permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther),
        QFileDevice::Permissions {});
    QCOMPARE(written.value(QStringLiteral("bytes")).toInt(), 7);
    QVERIFY(!written.value(QStringLiteral("cut")).toBool());

    // A name the Agent gives is a file name in that directory, and nothing
    // else.
    written = {};
    browser.control->handle(
        {{QStringLiteral("verb"), QStringLiteral("request")},
            {QStringLiteral("name"), QStringLiteral("agent")}, {QStringLiteral("id"), id},
            {QStringLiteral("body"), true}, {QStringLiteral("output"), QStringLiteral("me.json")}},
        [&written](const QJsonObject &answer) { written = answer; });
    QTRY_VERIFY(!written.isEmpty());
    QCOMPARE(QFileInfo(written.value(QStringLiteral("path")).toString()).fileName(),
        QStringLiteral("me.json"));
    for (const auto *name : {"../me.json", "/tmp/me.json", ".me.json"}) {
        QCOMPARE(code(browser.ask({{QStringLiteral("verb"), QStringLiteral("request")},
                     {QStringLiteral("id"), id}, {QStringLiteral("body"), true},
                     {QStringLiteral("output"), QLatin1StringView(name)}})),
            QStringLiteral("bad-request"));
    }
}

void AgentNetworkTest::cutsAResponseBodyAtTheLimit()
{
    Browser browser;
    QTemporaryDir downloads;
    QVERIFY(browser.browser->setDownloadDirectory(downloads.path()));
    const auto size = AgentNetwork::maximumBodyBytes + 1000;
    connect(browser.control.get(), &AgentControl::pageRequested, this,
        [&browser, size](int requestId, const QVariantMap &) {
            browser.control->answerPage(requestId,
                {{QStringLiteral("ok"), true},
                    {QStringLiteral("body"),
                        QString::fromLatin1(QByteArray(size, 'x').toBase64())}});
        });
    const auto tabId = browser.agentTab();
    browser.control->recordNetworkRequest(
        tabId, credentialed(QStringLiteral("me"), QStringLiteral("1")));

    QJsonObject written;
    browser.control->handle(
        {{QStringLiteral("verb"), QStringLiteral("request")},
            {QStringLiteral("name"), QStringLiteral("agent")},
            {QStringLiteral("id"), browser.newestId()}, {QStringLiteral("body"), true}},
        [&written](const QJsonObject &answer) { written = answer; });
    QTRY_VERIFY(!written.isEmpty());
    QVERIFY2(written.value(QStringLiteral("ok")).toBool(), qPrintable(code(written)));
    QVERIFY(written.value(QStringLiteral("cut")).toBool());
    QCOMPARE(written.value(QStringLiteral("bytes")).toDouble(),
        static_cast<double>(AgentNetwork::maximumBodyBytes));
    QCOMPARE(written.value(QStringLiteral("size")).toDouble(), static_cast<double>(size));
    QCOMPARE(QFileInfo(written.value(QStringLiteral("path")).toString()).size(),
        AgentNetwork::maximumBodyBytes);
}

void AgentNetworkTest::dropsTheRequestsWhenTheAgentDetaches()
{
    Browser browser;
    browser.control->setAttachmentIdleMs(50);
    const auto tabId = browser.agentTab();
    browser.control->recordNetworkRequest(tabId,
        began(QStringLiteral("a"), QStringLiteral("1"), QStringLiteral("https://a.example/kept")));
    QTRY_VERIFY(!browser.control->agentTabIds().contains(tabId));
    QVERIFY(listedUrls(browser.ask({{QStringLiteral("verb"), QStringLiteral("network")},
                           {QStringLiteral("tab"), tabId}}))
            .isEmpty());

    // And when the reader stops the Agent.
    browser.control->setAttachmentIdleMs(AgentControl::defaultAttachmentIdleMs);
    browser.ask(
        {{QStringLiteral("verb"), QStringLiteral("network")}, {QStringLiteral("tab"), tabId}});
    browser.control->recordNetworkRequest(tabId,
        began(
            QStringLiteral("b"), QStringLiteral("1"), QStringLiteral("https://a.example/stopped")));
    browser.control->stopAgent(tabId);
    QVERIFY(!browser.control->agentTabIds().contains(tabId));
    QVERIFY(listedUrls(browser.ask({{QStringLiteral("verb"), QStringLiteral("network")},
                           {QStringLiteral("tab"), tabId}}))
            .isEmpty());
}

void AgentNetworkTest::dropsTheRequestsWhenAllowAgentsGoesOff()
{
    Browser browser;
    const auto tabId = browser.agentTab();
    browser.control->recordNetworkRequest(tabId,
        began(QStringLiteral("a"), QStringLiteral("1"), QStringLiteral("https://a.example/kept")));
    browser.control->setAllowAgents(false);
    browser.control->setAllowAgents(true);
    QVERIFY(listedUrls(browser.ask({{QStringLiteral("verb"), QStringLiteral("network")},
                           {QStringLiteral("tab"), tabId}}))
            .isEmpty());
}

// A closed tab stops being an Agent tab at once, not at the next idle check,
// and its requests go with it.
void AgentNetworkTest::dropsTheRequestsWhenTheTabCloses()
{
    Browser browser;
    const auto tabId = browser.agentTab();
    const auto other = browser.agentTab(QStringLiteral("https://b.example/"));
    browser.control->recordNetworkRequest(tabId,
        began(QStringLiteral("a"), QStringLiteral("1"), QStringLiteral("https://a.example/kept")));
    QVERIFY(browser.control->agentTabIds().contains(tabId));
    QVERIFY(browser.browser->closeTabInSpace(tabId));
    QVERIFY(!browser.control->agentTabIds().contains(tabId));
    QVERIFY(browser.control->agentTabIds().contains(other));
}

void AgentNetworkTest::logsNoAddressHeaderOrBody()
{
    Browser browser;
    QTemporaryDir downloads;
    QVERIFY(browser.browser->setDownloadDirectory(downloads.path()));
    AgentActivityLog activity(QString {});
    browser.control->setActivityLog(&activity);
    connect(browser.control.get(), &AgentControl::pageRequested, this,
        [&browser](int requestId, const QVariantMap &) {
            browser.control->answerPage(requestId,
                {{QStringLiteral("ok"), true},
                    {QStringLiteral("body"),
                        QString::fromLatin1(QByteArray("body-text").toBase64())}});
        });
    const auto tabId = browser.agentTab();
    browser.control->recordNetworkRequest(
        tabId, credentialed(QStringLiteral("me"), QStringLiteral("1")));
    const auto id = browser.newestId();
    browser.ask(
        {{QStringLiteral("verb"), QStringLiteral("network")}, {QStringLiteral("failed"), true}});
    browser.ask({{QStringLiteral("verb"), QStringLiteral("request")}, {QStringLiteral("id"), id}});
    QJsonObject written;
    browser.control->handle({{QStringLiteral("verb"), QStringLiteral("request")},
                                {QStringLiteral("name"), QStringLiteral("agent")},
                                {QStringLiteral("id"), id}, {QStringLiteral("body"), true},
                                {QStringLiteral("output"), QStringLiteral("named.json")}},
        [&written](const QJsonObject &answer) { written = answer; });
    QTRY_VERIFY(!written.isEmpty());

    QStringList verbs;
    for (const auto &entry : activity.entries()) {
        verbs.append(entry.verb);
        if (entry.verb == u"request") {
            QCOMPARE(entry.target, QString::number(static_cast<quint64>(id)));
        }
        for (const auto &field : {entry.target, entry.outcome, entry.agent, entry.space}) {
            for (const auto *secret : {"api.example", "/me", "secret-token", "a=1", "s=1",
                     "application/json", "body-text", "named.json"}) {
                QVERIFY2(!field.contains(QLatin1StringView(secret)),
                    qPrintable(QStringLiteral("%1 logged %2").arg(entry.verb, field)));
            }
        }
    }
    QVERIFY(verbs.contains(QStringLiteral("network")));
    QCOMPARE(verbs.count(QStringLiteral("request")), 2);
}

Q_DECLARE_METATYPE(AgentNetwork::Header)

QTEST_GUILESS_MAIN(AgentNetworkTest)
#include "tst_agentnetwork.moc"
