#include "QtNetworkEvents.h"

#include <QJsonArray>
#include <QTest>

using omaweb::QtNetworkEvents;

namespace {

QJsonObject protocolEvent(
    const QString &method, const QJsonObject &params, const QString &session = {})
{
    QJsonObject message {{QStringLiteral("method"), method}, {QStringLiteral("params"), params}};
    if (!session.isEmpty()) {
        message.insert(QStringLiteral("sessionId"), session);
    }
    return message;
}

QJsonObject willBeSent(const QString &requestId, const QString &url, const QString &type,
    const QJsonObject &extra = {}, const QString &frame = QStringLiteral("MAIN"))
{
    QJsonObject params {
        {QStringLiteral("requestId"), requestId},
        {QStringLiteral("loaderId"), requestId},
        {QStringLiteral("frameId"), frame},
        {QStringLiteral("type"), type},
        {QStringLiteral("request"),
            QJsonObject {
                {QStringLiteral("url"), url},
                {QStringLiteral("method"), QStringLiteral("GET")},
                {QStringLiteral("headers"),
                    QJsonObject {{QStringLiteral("Accept"), QStringLiteral("*/*")}}},
            }},
    };
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
        params.insert(it.key(), it.value());
    }
    return protocolEvent(QStringLiteral("Network.requestWillBeSent"), params);
}

QVariantList pairs(std::initializer_list<std::pair<QString, QString>> headers)
{
    QVariantList list;
    for (const auto &[name, value] : headers) {
        list.append(QVariant(QVariantList {name, value}));
    }
    return list;
}

// Every report merged by key, as the core keeps them.
QHash<QString, QVariantMap> merged(const QList<QVariantMap> &reports)
{
    QHash<QString, QVariantMap> requests;
    for (const auto &report : reports) {
        auto &request = requests[report.value(QStringLiteral("key")).toString()];
        request.insert(report);
    }
    return requests;
}

} // namespace

class QtNetworkEventsTest final : public QObject {
    Q_OBJECT

private slots:
    void reportsARequestAndWhatFollows();
    void reportsHowARequestFailed();
    void prefersTheHeadersTheNetworkCarried();
    void endsAHopAtEachRedirect();
    void startsADocumentWithTheMainFrame();
    void startsADocumentThatMadeNoRequest();
    void keepsEachHopsHeadersInEitherOrder();
    void reportsAWebSocketAsItsOpeningRequest();
    void keepsAChildSessionsRequestsApart();
    void forgetsTheOldestRequestsPastTheLimit();
    void cutsABodyBeforeAnsweringWithIt();
    void saysABodyWasTooLargeToKeep();
    void readsWhatTheMainWorldCameTo();
    void readsWhatTheMainWorldThrew();
    void readsNothingElse();
};

void QtNetworkEventsTest::reportsARequestAndWhatFollows()
{
    QtNetworkEvents events;
    auto sent = willBeSent(
        QStringLiteral("7"), QStringLiteral("https://a.example/api"), QStringLiteral("Fetch"));
    auto params = sent.value(QStringLiteral("params")).toObject();
    auto request = params.value(QStringLiteral("request")).toObject();
    request.insert(QStringLiteral("method"), QStringLiteral("POST"));
    request.insert(QStringLiteral("hasPostData"), true);
    params.insert(QStringLiteral("request"), request);
    sent.insert(QStringLiteral("params"), params);

    QList<QVariantMap> reports = events.read(sent);
    QCOMPARE(reports.size(), 1);
    const auto begun = reports.constFirst();
    QCOMPARE(begun.value(QStringLiteral("method")).toString(), QStringLiteral("POST"));
    QCOMPARE(
        begun.value(QStringLiteral("url")).toString(), QStringLiteral("https://a.example/api"));
    QCOMPARE(begun.value(QStringLiteral("type")).toString(), QStringLiteral("Fetch"));
    QCOMPARE(begun.value(QStringLiteral("sentBody")).toBool(), true);
    QCOMPARE(begun.value(QStringLiteral("requestHeaders")).toList(),
        pairs({{QStringLiteral("Accept"), QStringLiteral("*/*")}}));
    QCOMPARE(begun.value(QStringLiteral("body")).toMap().value(QStringLiteral("requestId")),
        QVariant(QStringLiteral("7")));

    reports += events.read(protocolEvent(QStringLiteral("Network.responseReceived"),
        {{QStringLiteral("requestId"), QStringLiteral("7")},
            {QStringLiteral("type"), QStringLiteral("Fetch")},
            {QStringLiteral("response"),
                QJsonObject {{QStringLiteral("url"), QStringLiteral("https://a.example/api")},
                    {QStringLiteral("status"), 201},
                    {QStringLiteral("statusText"), QStringLiteral("Created")},
                    {QStringLiteral("headers"),
                        QJsonObject {
                            {QStringLiteral("Content-Type"), QStringLiteral("application/json")},
                            {QStringLiteral("Vary"), QStringLiteral("Accept\nOrigin")}}}}}}));
    reports += events.read(protocolEvent(QStringLiteral("Network.loadingFinished"),
        {{QStringLiteral("requestId"), QStringLiteral("7")},
            {QStringLiteral("encodedDataLength"), 812}}));

    const auto requests = merged(reports);
    QCOMPARE(requests.size(), 1);
    const auto whole = requests.constBegin().value();
    QCOMPARE(whole.value(QStringLiteral("status")).toInt(), 201);
    QCOMPARE(whole.value(QStringLiteral("statusText")).toString(), QStringLiteral("Created"));
    // A header sent twice is one value to a line, as the engine joins it.
    QCOMPARE(whole.value(QStringLiteral("responseHeaders")).toList(),
        pairs({{QStringLiteral("Content-Type"), QStringLiteral("application/json")},
            {QStringLiteral("Vary"), QStringLiteral("Accept\nOrigin")}}));
    QCOMPARE(whole.value(QStringLiteral("size")).toLongLong(), 812);
    QVERIFY(whole.value(QStringLiteral("failure")).toString().isEmpty());
    // Each report names the document it belongs to.
    for (const auto &report : std::as_const(reports)) {
        QCOMPARE(report.value(QStringLiteral("document")), begun.value(QStringLiteral("document")));
    }
}

void QtNetworkEventsTest::reportsHowARequestFailed()
{
    QtNetworkEvents events;
    QList<QVariantMap> reports;
    for (const auto *id : {"1", "2", "3"}) {
        reports += events.read(willBeSent(QLatin1StringView(id),
            QStringLiteral("https://a.example/%1").arg(QLatin1StringView(id)),
            QStringLiteral("Script")));
    }
    reports += events.read(protocolEvent(QStringLiteral("Network.loadingFailed"),
        {{QStringLiteral("requestId"), QStringLiteral("1")},
            {QStringLiteral("errorText"), QStringLiteral("net::ERR_NAME_NOT_RESOLVED")},
            {QStringLiteral("canceled"), false}}));
    reports += events.read(protocolEvent(QStringLiteral("Network.loadingFailed"),
        {{QStringLiteral("requestId"), QStringLiteral("2")},
            {QStringLiteral("errorText"), QStringLiteral("net::ERR_ABORTED")},
            {QStringLiteral("canceled"), true}}));
    reports += events.read(protocolEvent(QStringLiteral("Network.loadingFailed"),
        {{QStringLiteral("requestId"), QStringLiteral("3")},
            {QStringLiteral("errorText"), QStringLiteral("net::ERR_BLOCKED_BY_CLIENT")},
            {QStringLiteral("blockedReason"), QStringLiteral("inspector")}}));
    QStringList failures;
    for (const auto &request : merged(reports)) {
        failures.append(request.value(QStringLiteral("failure")).toString());
    }
    failures.sort();
    QCOMPARE(failures,
        QStringList({QStringLiteral("blocked: inspector"), QStringLiteral("canceled"),
            QStringLiteral("net::ERR_NAME_NOT_RESOLVED")}));
}

// The network service reports the headers it put on the wire, cookies
// included, in events of their own that may come before or after the page's.
void QtNetworkEventsTest::prefersTheHeadersTheNetworkCarried()
{
    QtNetworkEvents events;
    QList<QVariantMap> reports;
    reports += events.read(protocolEvent(QStringLiteral("Network.requestWillBeSentExtraInfo"),
        {{QStringLiteral("requestId"), QStringLiteral("9")},
            {QStringLiteral("headers"),
                QJsonObject {{QStringLiteral("Accept"), QStringLiteral("*/*")},
                    {QStringLiteral("Cookie"), QStringLiteral("a=1; b=2")}}}}));
    reports += events.read(protocolEvent(QStringLiteral("Network.responseReceivedExtraInfo"),
        {{QStringLiteral("requestId"), QStringLiteral("9")}, {QStringLiteral("statusCode"), 200},
            {QStringLiteral("headers"),
                QJsonObject {{QStringLiteral("Set-Cookie"), QStringLiteral("s=1\nt=2")}}}}));
    reports += events.read(willBeSent(
        QStringLiteral("9"), QStringLiteral("https://a.example/"), QStringLiteral("XHR")));
    reports += events.read(protocolEvent(QStringLiteral("Network.responseReceived"),
        {{QStringLiteral("requestId"), QStringLiteral("9")},
            {QStringLiteral("response"),
                QJsonObject {{QStringLiteral("status"), 200},
                    {QStringLiteral("headers"),
                        QJsonObject {
                            {QStringLiteral("Content-Type"), QStringLiteral("text/plain")}}}}}}));

    const auto requests = merged(reports);
    QCOMPARE(requests.size(), 1);
    const auto request = requests.constBegin().value();
    QCOMPARE(request.value(QStringLiteral("requestHeaders")).toList(),
        pairs({{QStringLiteral("Accept"), QStringLiteral("*/*")},
            {QStringLiteral("Cookie"), QStringLiteral("a=1; b=2")}}));
    QCOMPARE(request.value(QStringLiteral("responseHeaders")).toList(),
        pairs({{QStringLiteral("Set-Cookie"), QStringLiteral("s=1\nt=2")}}));
    // Nothing is reported of a request before it begins.
    QVERIFY(reports.constFirst().contains(QStringLiteral("url")));

    // And afterwards, for one already begun.
    reports = events.read(willBeSent(
        QStringLiteral("10"), QStringLiteral("https://a.example/2"), QStringLiteral("XHR")));
    reports += events.read(protocolEvent(QStringLiteral("Network.requestWillBeSentExtraInfo"),
        {{QStringLiteral("requestId"), QStringLiteral("10")},
            {QStringLiteral("headers"),
                QJsonObject {{QStringLiteral("Cookie"), QStringLiteral("c=3")}}}}));
    QCOMPARE(merged(reports).constBegin().value().value(QStringLiteral("requestHeaders")).toList(),
        pairs({{QStringLiteral("Cookie"), QStringLiteral("c=3")}}));
}

void QtNetworkEventsTest::endsAHopAtEachRedirect()
{
    QtNetworkEvents events;
    QList<QVariantMap> reports;
    reports += events.read(willBeSent(
        QStringLiteral("4"), QStringLiteral("http://a.example/"), QStringLiteral("Script")));
    reports += events.read(willBeSent(QStringLiteral("4"), QStringLiteral("https://a.example/"),
        QStringLiteral("Script"),
        {{QStringLiteral("redirectResponse"),
            QJsonObject {{QStringLiteral("status"), 301},
                {QStringLiteral("statusText"), QStringLiteral("Moved Permanently")},
                {QStringLiteral("headers"),
                    QJsonObject {
                        {QStringLiteral("Location"), QStringLiteral("https://a.example/")}}},
                {QStringLiteral("encodedDataLength"), 120}}}}));
    reports += events.read(protocolEvent(QStringLiteral("Network.responseReceived"),
        {{QStringLiteral("requestId"), QStringLiteral("4")},
            {QStringLiteral("response"), QJsonObject {{QStringLiteral("status"), 200}}}}));

    const auto requests = merged(reports);
    QCOMPARE(requests.size(), 2);
    QVariantMap first;
    QVariantMap second;
    for (const auto &request : requests) {
        (request.value(QStringLiteral("url")).toString().startsWith(u"http:") ? first : second)
            = request;
    }
    QCOMPARE(first.value(QStringLiteral("status")).toInt(), 301);
    QCOMPARE(first.value(QStringLiteral("responseHeaders")).toList(),
        pairs({{QStringLiteral("Location"), QStringLiteral("https://a.example/")}}));
    QCOMPARE(first.value(QStringLiteral("size")).toLongLong(), 120);
    // The engine keeps the last hop's body alone.
    QVERIFY(first.value(QStringLiteral("body")).toMap().isEmpty());
    QCOMPARE(second.value(QStringLiteral("status")).toInt(), 200);
    QCOMPARE(second.value(QStringLiteral("body")).toMap().value(QStringLiteral("requestId")),
        QVariant(QStringLiteral("4")));
}

void QtNetworkEventsTest::startsADocumentWithTheMainFrame()
{
    QtNetworkEvents events;
    events.setMainFrame(QStringLiteral("MAIN"));
    const auto documentOf = [](const QList<QVariantMap> &reports) {
        return reports.constFirst().value(QStringLiteral("document")).toString();
    };
    const auto first = documentOf(events.read(willBeSent(
        QStringLiteral("d1"), QStringLiteral("https://a.example/"), QStringLiteral("Document"))));
    const auto script = documentOf(events.read(willBeSent(
        QStringLiteral("s1"), QStringLiteral("https://a.example/a.js"), QStringLiteral("Script"))));
    QCOMPARE(script, first);
    // A frame's document is the page's.
    const auto frame = documentOf(
        events.read(willBeSent(QStringLiteral("f1"), QStringLiteral("https://a.example/frame"),
            QStringLiteral("Document"), {}, QStringLiteral("CHILD"))));
    QCOMPARE(frame, first);
    QJsonObject childDocument
        = willBeSent(QStringLiteral("c1"), QStringLiteral("https://b.example/"),
            QStringLiteral("Document"), {}, QStringLiteral("MAIN"));
    childDocument.insert(QStringLiteral("sessionId"), QStringLiteral("child"));
    QCOMPARE(documentOf(events.read(childDocument)), first);

    const auto second = documentOf(events.read(willBeSent(QStringLiteral("d2"),
        QStringLiteral("https://a.example/next"), QStringLiteral("Document"))));
    QVERIFY(second != first);
    // A redirect of the document is still the one document.
    const auto redirected = documentOf(events.read(willBeSent(QStringLiteral("d2"),
        QStringLiteral("https://a.example/landed"), QStringLiteral("Document"),
        {{QStringLiteral("redirectResponse"), QJsonObject {{QStringLiteral("status"), 302}}}})));
    QCOMPARE(redirected, second);
}

// A document the main frame shows without asking the network, as one from the
// back-forward cache is, starts a document all the same.
void QtNetworkEventsTest::startsADocumentThatMadeNoRequest()
{
    QtNetworkEvents events;
    events.setMainFrame(QStringLiteral("MAIN"));
    const auto first = events
                           .read(willBeSent(QStringLiteral("d1"),
                               QStringLiteral("https://a.example/"), QStringLiteral("Document")))
                           .constFirst()
                           .value(QStringLiteral("document"));
    const auto navigated = [](const QString &loader, const QString &parent = {}) {
        QJsonObject frame {{QStringLiteral("id"),
                               parent.isEmpty() ? QStringLiteral("MAIN") : QStringLiteral("CHILD")},
            {QStringLiteral("loaderId"), loader}};
        if (!parent.isEmpty()) {
            frame.insert(QStringLiteral("parentId"), parent);
        }
        return protocolEvent(
            QStringLiteral("Page.frameNavigated"), {{QStringLiteral("frame"), frame}});
    };
    // The document whose request was seen commits, and nothing starts.
    QVERIFY(events.read(navigated(QStringLiteral("d1"))).isEmpty());
    QVERIFY(events.read(navigated(QStringLiteral("f1"), QStringLiteral("MAIN"))).isEmpty());

    const auto restored = events.read(navigated(QStringLiteral("cached")));
    QCOMPARE(restored.size(), 1);
    QCOMPARE(restored.constFirst().value(QStringLiteral("key")).toString(), QString());
    QVERIFY(restored.constFirst().value(QStringLiteral("document")) != first);
    // What it asks for next is the restored document's.
    QCOMPARE(events
                 .read(willBeSent(QStringLiteral("s2"), QStringLiteral("https://a.example/a.js"),
                     QStringLiteral("Script")))
                 .constFirst()
                 .value(QStringLiteral("document")),
        restored.constFirst().value(QStringLiteral("document")));
}

// The network service's account of a hop can arrive after the page has moved
// on to the next, or before the hop it belongs to has begun.
void QtNetworkEventsTest::keepsEachHopsHeadersInEitherOrder()
{
    QtNetworkEvents events;
    QList<QVariantMap> reports;
    const auto sentExtra = [](const QString &cookie) {
        return protocolEvent(QStringLiteral("Network.requestWillBeSentExtraInfo"),
            {{QStringLiteral("requestId"), QStringLiteral("4")},
                {QStringLiteral("headers"), QJsonObject {{QStringLiteral("Cookie"), cookie}}}});
    };
    reports += events.read(willBeSent(
        QStringLiteral("4"), QStringLiteral("http://a.example/"), QStringLiteral("Script")));
    reports += events.read(sentExtra(QStringLiteral("a=1")));
    // The next hop's, before its request.
    reports += events.read(sentExtra(QStringLiteral("b=2")));
    reports += events.read(willBeSent(QStringLiteral("4"), QStringLiteral("https://a.example/"),
        QStringLiteral("Script"),
        {{QStringLiteral("redirectResponse"),
            QJsonObject {{QStringLiteral("status"), 301},
                {QStringLiteral("headers"),
                    QJsonObject {
                        {QStringLiteral("Location"), QStringLiteral("https://a.example/")}}}}}}));
    // The first hop's response, after the next hop began.
    reports += events.read(protocolEvent(QStringLiteral("Network.responseReceivedExtraInfo"),
        {{QStringLiteral("requestId"), QStringLiteral("4")}, {QStringLiteral("statusCode"), 301},
            {QStringLiteral("headers"),
                QJsonObject {{QStringLiteral("Location"), QStringLiteral("https://a.example/")},
                    {QStringLiteral("Set-Cookie"), QStringLiteral("s=1")}}}}));
    reports += events.read(protocolEvent(QStringLiteral("Network.responseReceivedExtraInfo"),
        {{QStringLiteral("requestId"), QStringLiteral("4")}, {QStringLiteral("statusCode"), 200},
            {QStringLiteral("headers"),
                QJsonObject {{QStringLiteral("Set-Cookie"), QStringLiteral("t=2")}}}}));

    QVariantMap first;
    QVariantMap second;
    for (const auto &request : merged(reports)) {
        (request.value(QStringLiteral("url")).toString().startsWith(u"http:") ? first : second)
            = request;
    }
    QCOMPARE(first.value(QStringLiteral("requestHeaders")).toList(),
        pairs({{QStringLiteral("Cookie"), QStringLiteral("a=1")}}));
    QCOMPARE(first.value(QStringLiteral("responseHeaders")).toList(),
        pairs({{QStringLiteral("Location"), QStringLiteral("https://a.example/")},
            {QStringLiteral("Set-Cookie"), QStringLiteral("s=1")}}));
    QCOMPARE(second.value(QStringLiteral("requestHeaders")).toList(),
        pairs({{QStringLiteral("Cookie"), QStringLiteral("b=2")}}));
    QCOMPARE(second.value(QStringLiteral("responseHeaders")).toList(),
        pairs({{QStringLiteral("Set-Cookie"), QStringLiteral("t=2")}}));
}

void QtNetworkEventsTest::reportsAWebSocketAsItsOpeningRequest()
{
    QtNetworkEvents events;
    QList<QVariantMap> reports;
    reports += events.read(protocolEvent(QStringLiteral("Network.webSocketCreated"),
        {{QStringLiteral("requestId"), QStringLiteral("w")},
            {QStringLiteral("url"), QStringLiteral("wss://a.example/live")}}));
    reports
        += events.read(protocolEvent(QStringLiteral("Network.webSocketWillSendHandshakeRequest"),
            {{QStringLiteral("requestId"), QStringLiteral("w")},
                {QStringLiteral("request"),
                    QJsonObject {{QStringLiteral("headers"),
                        QJsonObject {{QStringLiteral("Cookie"), QStringLiteral("a=1")}}}}}}));
    reports
        += events.read(protocolEvent(QStringLiteral("Network.webSocketHandshakeResponseReceived"),
            {{QStringLiteral("requestId"), QStringLiteral("w")},
                {QStringLiteral("response"),
                    QJsonObject {{QStringLiteral("status"), 101},
                        {QStringLiteral("statusText"), QStringLiteral("Switching Protocols")},
                        {QStringLiteral("headers"),
                            QJsonObject {
                                {QStringLiteral("Upgrade"), QStringLiteral("websocket")}}}}}}));
    // Its messages are not requests.
    reports += events.read(protocolEvent(QStringLiteral("Network.webSocketFrameReceived"),
        {{QStringLiteral("requestId"), QStringLiteral("w")},
            {QStringLiteral("response"),
                QJsonObject {{QStringLiteral("payloadData"), QStringLiteral("hi")}}}}));

    const auto requests = merged(reports);
    QCOMPARE(requests.size(), 1);
    const auto socket = requests.constBegin().value();
    QCOMPARE(socket.value(QStringLiteral("method")).toString(), QStringLiteral("GET"));
    QCOMPARE(socket.value(QStringLiteral("type")).toString(), QStringLiteral("WebSocket"));
    QCOMPARE(
        socket.value(QStringLiteral("url")).toString(), QStringLiteral("wss://a.example/live"));
    QCOMPARE(socket.value(QStringLiteral("status")).toInt(), 101);
    QCOMPARE(socket.value(QStringLiteral("requestHeaders")).toList(),
        pairs({{QStringLiteral("Cookie"), QStringLiteral("a=1")}}));
    QCOMPARE(socket.value(QStringLiteral("responseHeaders")).toList(),
        pairs({{QStringLiteral("Upgrade"), QStringLiteral("websocket")}}));
    QVERIFY(socket.value(QStringLiteral("body")).toMap().isEmpty());
}

void QtNetworkEventsTest::keepsAChildSessionsRequestsApart()
{
    QtNetworkEvents events;
    auto inChild = willBeSent(QStringLiteral("1"), QStringLiteral("https://b.example/frame.js"),
        QStringLiteral("Script"), {}, QStringLiteral("CHILD"));
    inChild.insert(QStringLiteral("sessionId"), QStringLiteral("child"));
    const auto reports = events.read(inChild)
        + events.read(willBeSent(QStringLiteral("1"), QStringLiteral("https://a.example/page.js"),
            QStringLiteral("Script")));
    const auto requests = merged(reports);
    QCOMPARE(requests.size(), 2);
    for (const auto &request : requests) {
        const auto body = request.value(QStringLiteral("body")).toMap();
        QCOMPARE(body.value(QStringLiteral("sessionId")).toString(),
            request.value(QStringLiteral("url")).toString().contains(u"b.example")
                ? QStringLiteral("child")
                : QString {});
    }
}

// A document that keeps asking, such as one polling, is followed for its
// newest requests alone, and news of an older one is not reported.
void QtNetworkEventsTest::forgetsTheOldestRequestsPastTheLimit()
{
    QtNetworkEvents events;
    events.setMainFrame(QStringLiteral("MAIN"));
    for (int index = 0; index <= QtNetworkEvents::maximumTracked; ++index) {
        events.read(willBeSent(QString::number(index), QStringLiteral("https://a.example/poll"),
            QStringLiteral("Fetch")));
    }
    const auto finished = [&events](int index) {
        return events.read(protocolEvent(QStringLiteral("Network.loadingFinished"),
            {{QStringLiteral("requestId"), QString::number(index)},
                {QStringLiteral("encodedDataLength"), 10}}));
    };
    QVERIFY(finished(0).isEmpty());
    QCOMPARE(finished(1).size(), 1);
    QCOMPARE(finished(QtNetworkEvents::maximumTracked).size(), 1);
}

void QtNetworkEventsTest::cutsABodyBeforeAnsweringWithIt()
{
    const auto bytes = [](const QVariantMap &answer) {
        return QByteArray::fromBase64(answer.value(QStringLiteral("body")).toString().toLatin1());
    };
    auto answer = QtNetworkEvents::bodyAnswer(
        {{QStringLiteral("result"),
            QJsonObject {{QStringLiteral("body"), QStringLiteral("abcdef")},
                {QStringLiteral("base64Encoded"), false}}}},
        QStringLiteral("response"), 4, 0);
    QVERIFY(answer.value(QStringLiteral("ok")).toBool());
    QCOMPARE(bytes(answer), QByteArray("abcd"));
    QCOMPARE(answer.value(QStringLiteral("size")).toLongLong(), 6);

    answer = QtNetworkEvents::bodyAnswer(
        {{QStringLiteral("result"),
            QJsonObject {{QStringLiteral("body"), QStringLiteral("AQID")},
                {QStringLiteral("base64Encoded"), true}}}},
        QStringLiteral("response"), 4, 0);
    QCOMPARE(bytes(answer), QByteArray("\x01\x02\x03"));
    QCOMPARE(answer.value(QStringLiteral("size")).toLongLong(), 3);

    answer = QtNetworkEvents::bodyAnswer(
        {{QStringLiteral("result"),
            QJsonObject {{QStringLiteral("postData"), QStringLiteral("a=1")}}}},
        QStringLiteral("sent"), 10, 0);
    QCOMPARE(bytes(answer), QByteArray("a=1"));
}

// The engine keeps a body whole or not at all, so one past what it keeps has
// nothing left to cut.
void QtNetworkEventsTest::saysABodyWasTooLargeToKeep()
{
    const QJsonObject gone {{QStringLiteral("error"),
        QJsonObject {{QStringLiteral("code"), -32000},
            {QStringLiteral("message"), QStringLiteral("No data found for resource")}}}};
    const auto large = QtNetworkEvents::bodyAnswer(
        gone, QStringLiteral("response"), 10, QtNetworkEvents::keptBodyBytes + 1);
    QVERIFY(!large.value(QStringLiteral("ok")).toBool());
    QCOMPARE(large.value(QStringLiteral("code")).toString(), QStringLiteral("too-large"));
    QCOMPARE(QtNetworkEvents::bodyAnswer(gone, QStringLiteral("response"), 10, 100)
                 .value(QStringLiteral("code"))
                 .toString(),
        QStringLiteral("not-found"));
}

namespace {

// The session's answer to `Runtime.evaluate`, whose result is the
// remote object `evaluationSource` settles to.
QJsonObject evaluatedTo(const QJsonObject &remote, const QJsonObject &exception = {})
{
    QJsonObject result {{QStringLiteral("result"), remote}};
    if (!exception.isEmpty()) {
        result.insert(QStringLiteral("exceptionDetails"), exception);
    }
    return {{QStringLiteral("id"), 7}, {QStringLiteral("result"), result}};
}

QJsonObject text(const QString &value)
{
    return {{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("value"), value}};
}

} // namespace

// The main world answers as the isolated one does: the value as JSON, read
// back, and a refusal for what JSON cannot carry or is too large to.
void QtNetworkEventsTest::readsWhatTheMainWorldCameTo()
{
    auto answer = QtNetworkEvents::evaluationAnswer(
        evaluatedTo(text(QStringLiteral(R"({"user":"ada","items":[1,2]})"))));
    QVERIFY(answer.value(QStringLiteral("ok")).toBool());
    QCOMPARE(answer.value(QStringLiteral("value")).toMap().value(QStringLiteral("user")).toString(),
        QStringLiteral("ada"));
    QCOMPARE(answer.value(QStringLiteral("value")).toMap().value(QStringLiteral("items")).toList(),
        (QVariantList {1, 2}));

    answer = QtNetworkEvents::evaluationAnswer(evaluatedTo(text(QStringLiteral("42"))));
    QCOMPARE(answer.value(QStringLiteral("value")).toInt(), 42);
    answer = QtNetworkEvents::evaluationAnswer(evaluatedTo(text(QStringLiteral(R"("plain")"))));
    QCOMPARE(answer.value(QStringLiteral("value")).toString(), QStringLiteral("plain"));
    answer = QtNetworkEvents::evaluationAnswer(evaluatedTo(text(QStringLiteral("null"))));
    QVERIFY(answer.value(QStringLiteral("ok")).toBool());
    QVERIFY(answer.value(QStringLiteral("value")).isNull());

    answer = QtNetworkEvents::evaluationAnswer(
        evaluatedTo({{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("value"),
                QJsonObject {{QStringLiteral("notJson"),
                    QStringLiteral("Converting circular structure to JSON")}}}}));
    QVERIFY(!answer.value(QStringLiteral("ok")).toBool());
    QCOMPARE(answer.value(QStringLiteral("code")).toString(), QStringLiteral("not-json"));
    QVERIFY(answer.value(QStringLiteral("error")).toString().contains(u"circular structure"));

    answer = QtNetworkEvents::evaluationAnswer(
        evaluatedTo(text(u'"' + QString(QtNetworkEvents::maximumValueCharacters, u'x') + u'"')));
    QCOMPARE(answer.value(QStringLiteral("code")).toString(), QStringLiteral("too-large"));

    // A session that ended answers nothing.
    answer = QtNetworkEvents::evaluationAnswer({});
    QVERIFY(!answer.value(QStringLiteral("ok")).toBool());
    QCOMPARE(answer.value(QStringLiteral("code")).toString(), QStringLiteral("failed"));
}

// An error by its message, as `eval` gives it, and anything else thrown as
// itself.
void QtNetworkEventsTest::readsWhatTheMainWorldThrew()
{
    auto answer = QtNetworkEvents::evaluationAnswer(
        evaluatedTo({{QStringLiteral("type"), QStringLiteral("object")}},
            {{QStringLiteral("text"), QStringLiteral("Uncaught")},
                {QStringLiteral("exception"),
                    QJsonObject {{QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("subtype"), QStringLiteral("error")},
                        {QStringLiteral("className"), QStringLiteral("ReferenceError")},
                        {QStringLiteral("description"),
                            QStringLiteral(
                                "ReferenceError: app is not defined\n    at <anonymous>:2:1")}}}}));
    QVERIFY(!answer.value(QStringLiteral("ok")).toBool());
    QCOMPARE(answer.value(QStringLiteral("code")).toString(), QStringLiteral("threw"));
    QCOMPARE(
        answer.value(QStringLiteral("error")).toString(), QStringLiteral("app is not defined"));

    answer = QtNetworkEvents::evaluationAnswer(
        evaluatedTo({{QStringLiteral("type"), QStringLiteral("string")}},
            {{QStringLiteral("text"), QStringLiteral("Uncaught (in promise)")},
                {QStringLiteral("exception"), text(QStringLiteral("no  session"))}}));
    QCOMPARE(answer.value(QStringLiteral("code")).toString(), QStringLiteral("threw"));
    QCOMPARE(answer.value(QStringLiteral("error")).toString(), QStringLiteral("no session"));

    // A promise that rejects is caught in the page, which answers with what
    // it was rejected with.
    answer = QtNetworkEvents::evaluationAnswer(
        evaluatedTo({{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("value"),
                QJsonObject {{QStringLiteral("threw"), QStringLiteral("later  on")}}}}));
    QCOMPARE(answer.value(QStringLiteral("code")).toString(), QStringLiteral("threw"));
    QCOMPARE(answer.value(QStringLiteral("error")).toString(), QStringLiteral("later on"));

    // A source that did not parse throws before it runs.
    answer = QtNetworkEvents::evaluationAnswer(
        evaluatedTo({{QStringLiteral("type"), QStringLiteral("object")}},
            {{QStringLiteral("text"), QStringLiteral("Uncaught")},
                {QStringLiteral("exception"),
                    QJsonObject {{QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("subtype"), QStringLiteral("error")},
                        {QStringLiteral("className"), QStringLiteral("SyntaxError")},
                        {QStringLiteral("description"),
                            QStringLiteral("SyntaxError: Unexpected token ')'")}}}}));
    QCOMPARE(
        answer.value(QStringLiteral("error")).toString(), QStringLiteral("Unexpected token ')'"));
}

void QtNetworkEventsTest::readsNothingElse()
{
    QtNetworkEvents events;
    QVERIFY(events.read({{QStringLiteral("id"), 3}, {QStringLiteral("result"), QJsonObject {}}})
            .isEmpty());
    QVERIFY(events.read(protocolEvent(QStringLiteral("Runtime.consoleAPICalled"), {})).isEmpty());
    // Nothing of a request that never began.
    QVERIFY(events
            .read(protocolEvent(QStringLiteral("Network.loadingFinished"),
                {{QStringLiteral("requestId"), QStringLiteral("x")}}))
            .isEmpty());
}

QTEST_GUILESS_MAIN(QtNetworkEventsTest)
#include "tst_qtnetworkevents.moc"
