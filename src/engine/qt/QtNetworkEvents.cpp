#include "QtNetworkEvents.h"

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
    if (!method.startsWith(u"Network.")) {
        return {};
    }
    const auto session = message.value(QStringLiteral("sessionId")).toString();
    const auto params = message.value(QStringLiteral("params")).toObject();
    const auto requestId = params.value(QStringLiteral("requestId")).toString();
    if (requestId.isEmpty()) {
        return {};
    }
    // The engine's ids are its own per session, and a child session's are
    // kept apart by its id.
    const auto id = session + u'/' + requestId;
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
            ended.insert(QStringLiteral("responseHeaders"),
                headerPairs(redirect.value(QStringLiteral("headers"))));
            if (redirect.contains(QStringLiteral("encodedDataLength"))) {
                ended.insert(QStringLiteral("size"),
                    redirect.value(QStringLiteral("encodedDataLength")).toInteger());
            }
            ended.insert(QStringLiteral("body"), QVariantMap {});
            reports.append(ended);
            ++tracked->hop;
            tracked->sentHeaders.clear();
            tracked->receivedHeaders.clear();
            tracked->receivedFromService = false;
        } else if (session.isEmpty() && redirect.isEmpty()
            && params.value(QStringLiteral("type")).toString() == u"Document"
            && (m_mainFrame.isEmpty()
                || params.value(QStringLiteral("frameId")).toString() == m_mainFrame)) {
            ++m_document;
            // The last document's requests are not reported again, so they
            // are not tracked either.
            const auto current = *tracked;
            m_requests.clear();
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
        tracked->sentHeaders = headers;
        if (!tracked->begun) {
            return {};
        }
        auto update = report(id, *tracked);
        update.insert(QStringLiteral("requestHeaders"), headers);
        return {update};
    }

    if (method == u"Network.responseReceivedExtraInfo") {
        tracked->receivedHeaders = headerPairs(params.value(QStringLiteral("headers")));
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
