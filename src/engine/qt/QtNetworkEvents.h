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
// every report says which document it belongs to.
class QtNetworkEvents final {
public:
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
        bool begun = false;
        bool receivedFromService = false;
        int receivedStatus = 0;
    };

    QVariantMap report(const QString &id, const Tracked &tracked) const;

    QString m_mainFrame;
    int m_document = 0;
    QHash<QString, Tracked> m_requests;
};

} // namespace omaweb
