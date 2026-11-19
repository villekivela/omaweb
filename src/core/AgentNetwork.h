#pragma once

#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace omaweb {

// The requests the pages of Agent tabs have made, kept for `network` and
// `request` (ADR 0051).
//
// A tab's requests belong to the document that made them, so a new document
// starts the tab's buffer again, as the console does. Each tab keeps its most
// recent requests only. A request is numbered once, when it is first seen, by
// a count of every request the browser has kept since it started: the number
// is the id `request` takes and the cursor `--since` reads, so `--since`
// answers with the requests that began after the last call. What the engine
// learns of a request later, its response, failure or size, is added to it
// under the same number. Requests live in memory and nowhere else, and their
// bodies stay in the engine until one is asked for.
class AgentNetwork final {
public:
    static constexpr qsizetype maximumRequests = 500;
    // The most of a body written for `request --body`. A longer one is cut
    // there, and the answer says so.
    static constexpr qsizetype maximumBodyBytes = 10 * 1000 * 1000;

    struct Header {
        QString name;
        QString value;

        bool operator==(const Header &) const = default;
    };

    struct Request {
        quint64 id = 0;
        // The adapter's own name for the request, one per redirect hop, which
        // later news of it comes under.
        QString key;
        // What the adapter needs to fetch the request's bodies. The core
        // never reads it.
        QVariantMap body;
        QString method;
        QString url;
        // The engine's word for what was asked for: Document, Script, XHR,
        // WebSocket and so on.
        QString type;
        // 0 until a response arrives.
        int status = 0;
        QString statusText;
        // Why it failed, in the engine's words, when it did.
        QString failure;
        // The bytes it took on the wire, or -1 until it is known.
        qint64 size = -1;
        QVector<Header> requestHeaders;
        QVector<Header> responseHeaders;
        // Whether the page sent a body with it.
        bool sentBody = false;
    };

    struct Reading {
        // In the order they began.
        QVector<Request> requests;
        // What to pass as `since` next time: the newest request the tab has
        // kept, whether or not it was listed.
        quint64 cursor = 0;
        // Requests newer than `since` that were pushed out of the buffer
        // before this call could read them.
        bool truncated = false;
    };

    // What the adapter reports of a request, as a map: `key` and `document`,
    // and whichever of `body`, `method`, `url`, `type`, `status`,
    // `statusText`, `failure`, `size`, `requestHeaders`, `responseHeaders`
    // and `sentBody` it has learnt. Headers are a list of `[name, value]`
    // pairs. A key not seen before in the document is a new request. Anything
    // left out keeps what was known.
    void record(const QString &tabId, const QVariantMap &report);
    // Requests newer than `since`, all of them or only those that failed or
    // were answered with an error.
    Reading read(const QString &tabId, bool failedOnly, quint64 since) const;
    std::optional<Request> find(const QString &tabId, quint64 id) const;
    void forget(const QString &tabId);
    void forgetAll();

    // Whether a request failed or was answered with an error status.
    static bool failed(const Request &request);
    // The headers as a granted Space shows them: the value of each credential
    // header replaced by how many values it held, such as
    // `<redacted, 3 values>`.
    static QVector<Header> redacted(const QVector<Header> &headers);

private:
    struct Buffer {
        QString document;
        QVector<Request> requests;
        // The newest id pushed out, for telling a reader it missed some.
        quint64 evictedThrough = 0;
    };

    QHash<QString, Buffer> m_buffers;
    quint64 m_nextId = 0;
};

} // namespace omaweb
