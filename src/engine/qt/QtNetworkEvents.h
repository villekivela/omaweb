#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace omaweb {

// Reads the DevTools protocol's Network events from a page's debugging
// session into the reports `AgentControl::recordNetworkRequest` takes, for
// `network` and `request` (ADR 0051).
//
// A request is reported when it begins and again as its response, its
// failure and its size arrive, each time with what changed. A redirect ends
// one hop and begins the next under a key of its own, since the engine keeps
// one id for every hop. The headers the network service actually sent and
// received, which carry the cookies, are reported over the ones the page
// asked for. A WebSocket is reported as its opening request and the
// response to it. Events from a frame in another process come over a child
// session and are kept apart by its id.
//
// The page's main frame loading a document starts the next document, and
// every report says which document it belongs to. A document the main frame
// shows without a request, as one from the back-forward cache, is reported as
// a report with an empty key that names the document alone. That needs the
// session's Page events.
class QtNetworkEvents final {
public:
    // The requests of one document followed at once. Past it the oldest is
    // forgotten, and what more is heard of it is not reported.
    static constexpr int maximumTracked = 1000;
    // What the engine keeps of each body for the session to read, whole or
    // not at all, and of a page's bodies together.
    static constexpr int keptBodyBytes = 100 * 1000 * 1000;
    static constexpr int keptBodiesBytes = 200 * 1000 * 1000;

    // The answer to the session's `Network.getResponseBody`, or
    // `getRequestPostData` for `part` "sent", as `fetchBody` gives it: the
    // body's first `limit` bytes as base64 and its whole `size`, or a
    // refusal. `size` is what the request's report said it carried.
    static QVariantMap bodyAnswer(
        const QJsonObject &answer, const QString &part, qint64 limit, qint64 size);

    // The main frame's id, from `Page.getFrameTree`. Until it is known, a
    // document request from the page's own session starts a document.
    void setMainFrame(const QString &frameId);
    // One message the session sent. Anything that is not a Network event
    // answers nothing.
    QList<QVariantMap> read(const QJsonObject &message);

private:
    struct Tracked {
        int hop = 0;
        // The headers the network service reported before the event they
        // belong to, kept until it arrives.
        QVariantList sentHeaders;
        QVariantList receivedHeaders;
        // The next hop's, which the service can report before the redirect
        // that starts it.
        QVariantList nextSentHeaders;
        bool begun = false;
        bool sentFromService = false;
        bool receivedFromService = false;
        // The redirect that ended the hop before, whose response the service
        // can report after the next hop has begun.
        int redirectStatus = 0;
        bool redirectFromService = true;
    };

    QVariantMap report(const QString &id, const Tracked &tracked) const;

    QString m_mainFrame;
    int m_document = 0;
    // The loader of the main frame's document request, whose commit starts
    // nothing more.
    QString m_documentLoader;
    QHash<QString, Tracked> m_requests;
    // The keys of `m_requests`, oldest first.
    QList<QString> m_order;
};

} // namespace omaweb
