#include "QtNetworkEvents.h"

#include <utility>

namespace omaweb {
namespace {

    // The protocol's headers are an object of name to value, with a header
    // sent more than once joined by line breaks. Its order is the order they
    // were sent in.
    QVariantList headerPairs(const QJsonValue &headers)
    {
        QVariantList pairs;
        const auto object = headers.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            pairs.append(QVariant(QVariantList {it.key(), it.value().toString()}));
        }
        return pairs;
    }

    QVariantMap refusal(const QString &code, const QString &error)
    {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("code"), code},
            {QStringLiteral("error"), error}};
    }

    QString failureOf(const QJsonObject &params)
    {
        const auto blocked = params.value(QStringLiteral("blockedReason")).toString();
        if (!blocked.isEmpty()) {
            return QStringLiteral("blocked: ") + blocked;
        }
        if (params.value(QStringLiteral("canceled")).toBool()) {
            return QStringLiteral("canceled");
        }
        return params.value(QStringLiteral("errorText")).toString();
    }

} // namespace

void QtNetworkEvents::setMainFrame(const QString &frameId) { m_mainFrame = frameId; }

QVariantMap QtNetworkEvents::bodyAnswer(
    const QJsonObject &answer, const QString &part, qint64 limit, qint64 size)
{
    const auto result = answer.value(QStringLiteral("result")).toObject();
    if (answer.contains(QStringLiteral("error")) || result.isEmpty()) {
        if (size > keptBodyBytes) {
            return refusal(QStringLiteral("too-large"),
                QStringLiteral("The body is %1 bytes. The engine keeps none of a body over 100 MB, "
                               "so there is nothing of it to write.")
                    .arg(size));
        }
        return refusal(QStringLiteral("not-found"),
            QStringLiteral("The engine no longer holds this body. It keeps a body while its "
                           "document is on show, up to 100 MB."));
    }
    const auto text
        = result.value(part == u"sent" ? QStringLiteral("postData") : QStringLiteral("body"))
              .toString();
    auto body = result.value(QStringLiteral("base64Encoded")).toBool()
        ? QByteArray::fromBase64(text.toLatin1())
        : text.toUtf8();
    const auto whole = body.size();
    // Cut here, so what is not written never travels further.
    if (body.size() > limit) {
        body.truncate(limit);
    }
    return {{QStringLiteral("ok"), true},
        {QStringLiteral("body"), QString::fromLatin1(body.toBase64())},
        {QStringLiteral("size"), whole}};
}

QVariantMap QtNetworkEvents::report(const QString &id, const Tracked &tracked) const
{
    return {
        {QStringLiteral("key"), QStringLiteral("%1#%2").arg(id).arg(tracked.hop)},
        {QStringLiteral("document"), QString::number(m_document)},
    };
}

QList<QVariantMap> QtNetworkEvents::read(const QJsonObject &message)
{
    const auto method = message.value(QStringLiteral("method")).toString();
    const auto session = message.value(QStringLiteral("sessionId")).toString();
    const auto params = message.value(QStringLiteral("params")).toObject();
    if (method == u"Page.frameNavigated" && session.isEmpty()) {
        const auto frame = params.value(QStringLiteral("frame")).toObject();
        const auto loader = frame.value(QStringLiteral("loaderId")).toString();
        if (frame.contains(QStringLiteral("parentId")) || loader == m_documentLoader
            || (!m_mainFrame.isEmpty()
                && frame.value(QStringLiteral("id")).toString() != m_mainFrame)) {
            return {};
        }
        ++m_document;
        m_documentLoader = loader;
        m_requests.clear();
        m_order.clear();
        return {{{QStringLiteral("key"), QString {}},
            {QStringLiteral("document"), QString::number(m_document)}}};
    }
    if (!method.startsWith(u"Network.")) {
        return {};
    }
    const auto requestId = params.value(QStringLiteral("requestId")).toString();
    if (requestId.isEmpty()) {
        return {};
    }
    // The engine's ids are its own per session, and a child session's are
    // kept apart by its id.
    const auto id = session + u'/' + requestId;
    if (!m_requests.contains(id)) {
        // Only the start of a request, or the headers the network service
        // reports ahead of it, is a request to follow. News of one that is
        // not followed is not reported.
        if (method != u"Network.requestWillBeSent" && method != u"Network.webSocketCreated"
            && !method.endsWith(u"ExtraInfo")
            && method != u"Network.webSocketWillSendHandshakeRequest") {
            return {};
        }
        // The oldest goes before the new one is made, as removing from the
        // hash can move what it holds.
        if (m_order.size() >= maximumTracked) {
            m_requests.remove(m_order.takeFirst());
        }
        m_order.append(id);
    }
    auto *tracked = &m_requests[id];
    const auto bodyOf = [&session, &requestId] {
        QVariantMap body {{QStringLiteral("requestId"), requestId}};
        if (!session.isEmpty()) {
            body.insert(QStringLiteral("sessionId"), session);
        }
        return body;
    };

    if (method == u"Network.requestWillBeSent" || method == u"Network.webSocketCreated") {
        QList<QVariantMap> reports;
        const auto request = params.value(QStringLiteral("request")).toObject();
        const auto redirect = params.value(QStringLiteral("redirectResponse")).toObject();
        if (tracked->begun && !redirect.isEmpty()) {
            // The hop before ends with the redirect's response, and the
            // engine keeps no body for it.
            auto ended = report(id, *tracked);
            ended.insert(
                QStringLiteral("status"), redirect.value(QStringLiteral("status")).toInt());
            ended.insert(QStringLiteral("statusText"),
                redirect.value(QStringLiteral("statusText")).toString());
            if (!tracked->receivedFromService) {
                ended.insert(QStringLiteral("responseHeaders"),
                    headerPairs(redirect.value(QStringLiteral("headers"))));
            }
            if (redirect.contains(QStringLiteral("encodedDataLength"))) {
                ended.insert(QStringLiteral("size"),
                    redirect.value(QStringLiteral("encodedDataLength")).toInteger());
            }
            ended.insert(QStringLiteral("body"), QVariantMap {});
            reports.append(ended);
            tracked->redirectStatus = redirect.value(QStringLiteral("status")).toInt();
            tracked->redirectFromService = tracked->receivedFromService;
            ++tracked->hop;
            tracked->sentHeaders = std::exchange(tracked->nextSentHeaders, {});
            tracked->sentFromService = !tracked->sentHeaders.isEmpty();
            tracked->receivedHeaders.clear();
            tracked->receivedFromService = false;
        } else if (session.isEmpty() && redirect.isEmpty()
            && params.value(QStringLiteral("type")).toString() == u"Document"
            && (m_mainFrame.isEmpty()
                || params.value(QStringLiteral("frameId")).toString() == m_mainFrame)) {
            ++m_document;
            m_documentLoader = params.value(QStringLiteral("loaderId")).toString();
            // The last document's requests are not reported again, so they
            // are not tracked either.
            const auto current = *tracked;
            m_requests.clear();
            m_order = {id};
            tracked = &m_requests.insert(id, current).value();
        }
        tracked->begun = true;
        auto begun = report(id, *tracked);
        const auto socket = method == u"Network.webSocketCreated";
        begun.insert(QStringLiteral("url"),
            socket ? params.value(QStringLiteral("url")).toString()
                   : request.value(QStringLiteral("url")).toString());
        begun.insert(QStringLiteral("method"),
            socket ? QStringLiteral("GET") : request.value(QStringLiteral("method")).toString());
        begun.insert(QStringLiteral("type"),
            socket ? QStringLiteral("WebSocket") : params.value(QStringLiteral("type")).toString());
        begun.insert(
            QStringLiteral("sentBody"), request.value(QStringLiteral("hasPostData")).toBool());
        begun.insert(QStringLiteral("requestHeaders"),
            tracked->sentHeaders.isEmpty() ? headerPairs(request.value(QStringLiteral("headers")))
                                           : tracked->sentHeaders);
        // A socket's messages are not kept, so it has no body to read.
        begun.insert(QStringLiteral("body"), socket ? QVariantMap {} : bodyOf());
        if (!tracked->receivedHeaders.isEmpty()) {
            begun.insert(QStringLiteral("responseHeaders"), tracked->receivedHeaders);
        }
        reports.append(begun);
        return reports;
    }

    if (method == u"Network.requestWillBeSentExtraInfo"
        || method == u"Network.webSocketWillSendHandshakeRequest") {
        const auto headers = headerPairs(method == u"Network.webSocketWillSendHandshakeRequest"
                ? params.value(QStringLiteral("request"))
                      .toObject()
                      .value(QStringLiteral("headers"))
                : params.value(QStringLiteral("headers")));
        // A hop's are reported once, so a second account before the hop ends
        // is the next hop's, sent before the redirect that starts it.
        if (tracked->begun && tracked->sentFromService) {
            tracked->nextSentHeaders = headers;
            return {};
        }
        tracked->sentHeaders = headers;
        tracked->sentFromService = true;
        if (!tracked->begun) {
            return {};
        }
        auto update = report(id, *tracked);
        update.insert(QStringLiteral("requestHeaders"), headers);
        return {update};
    }

    if (method == u"Network.responseReceivedExtraInfo") {
        const auto headers = headerPairs(params.value(QStringLiteral("headers")));
        // The redirect's own response, reported after the hop it ended.
        if (tracked->begun && tracked->hop > 0 && !tracked->redirectFromService
            && !tracked->receivedFromService
            && params.value(QStringLiteral("statusCode")).toInt() == tracked->redirectStatus) {
            tracked->redirectFromService = true;
            Tracked ended = *tracked;
            --ended.hop;
            auto update = report(id, ended);
            update.insert(QStringLiteral("responseHeaders"), headers);
            return {update};
        }
        tracked->receivedHeaders = headers;
        tracked->receivedFromService = true;
        if (!tracked->begun) {
            return {};
        }
        auto update = report(id, *tracked);
        update.insert(QStringLiteral("responseHeaders"), tracked->receivedHeaders);
        return {update};
    }

    if (!tracked->begun) {
        m_requests.remove(id);
        m_order.removeOne(id);
        return {};
    }
    auto update = report(id, *tracked);
    if (method == u"Network.responseReceived"
        || method == u"Network.webSocketHandshakeResponseReceived") {
        const auto response = params.value(QStringLiteral("response")).toObject();
        update.insert(QStringLiteral("status"), response.value(QStringLiteral("status")).toInt());
        update.insert(
            QStringLiteral("statusText"), response.value(QStringLiteral("statusText")).toString());
        if (!tracked->receivedFromService) {
            update.insert(QStringLiteral("responseHeaders"),
                headerPairs(response.value(QStringLiteral("headers"))));
        }
        return {update};
    }
    if (method == u"Network.loadingFinished") {
        update.insert(
            QStringLiteral("size"), params.value(QStringLiteral("encodedDataLength")).toInteger());
        return {update};
    }
    if (method == u"Network.loadingFailed") {
        update.insert(QStringLiteral("failure"), failureOf(params));
        return {update};
    }
    return {};
}

} // namespace omaweb
