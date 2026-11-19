#include "AgentNetwork.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace omaweb {
namespace {

    QVector<AgentNetwork::Header> headersFrom(const QVariant &value)
    {
        QVector<AgentNetwork::Header> headers;
        for (const auto &entry : value.toList()) {
            const auto pair = entry.toList();
            if (pair.size() == 2) {
                headers.append({pair.at(0).toString(), pair.at(1).toString()});
            }
        }
        return headers;
    }

    // The header names whose values are the reader's credentials, lowercase.
    const QSet<QString> &credentialHeaders()
    {
        static const QSet<QString> names {
            QStringLiteral("cookie"),
            QStringLiteral("set-cookie"),
            QStringLiteral("authorization"),
            QStringLiteral("proxy-authorization"),
        };
        return names;
    }

    // A request carries its cookies in one header, separated by semicolons.
    // Every other header holds one value to a line, which is how an engine
    // joins a header sent more than once.
    qsizetype valueCount(const QString &name, const QString &value)
    {
        const auto separator = name.compare(QStringLiteral("cookie"), Qt::CaseInsensitive) == 0
            ? QLatin1Char(';')
            : QLatin1Char('\n');
        const auto parts = value.split(separator);
        return std::count_if(parts.cbegin(), parts.cend(),
            [](const QString &part) { return !part.trimmed().isEmpty(); });
    }

} // namespace

void AgentNetwork::record(const QString &tabId, const QVariantMap &report)
{
    const auto key = report.value(QStringLiteral("key")).toString();
    const auto document = report.value(QStringLiteral("document")).toString();
    if (key.isEmpty()) {
        // A document that asked for nothing on its way in, as one from the
        // back-forward cache, still leaves the last one's requests behind.
        const auto found = m_buffers.find(tabId);
        if (found != m_buffers.end() && found->document != document) {
            *found = {};
            found->document = document;
        }
        return;
    }
    auto &buffer = m_buffers[tabId];
    if (buffer.document != document) {
        // News of a request of a document that has gone is not the new one's.
        const auto known = std::any_of(buffer.requests.cbegin(), buffer.requests.cend(),
            [&key](const Request &request) { return request.key == key; });
        if (known || !report.contains(QStringLiteral("url"))) {
            return;
        }
        buffer = {};
        buffer.document = document;
    }
    auto found = std::find_if(buffer.requests.begin(), buffer.requests.end(),
        [&key](const Request &request) { return request.key == key; });
    if (found == buffer.requests.end()) {
        if (!report.contains(QStringLiteral("url"))) {
            // A request heard of only after it was pushed out stays out.
            return;
        }
        Request request;
        request.id = ++m_nextId;
        request.key = key;
        buffer.requests.append(std::move(request));
        while (buffer.requests.size() > maximumRequests) {
            buffer.evictedThrough = buffer.requests.constFirst().id;
            buffer.requests.removeFirst();
        }
        found = std::prev(buffer.requests.end());
    }
    auto &request = *found;
    const auto text = [&report](const QString &field, QString &into) {
        if (report.contains(field)) {
            into = report.value(field).toString();
        }
    };
    if (report.contains(QStringLiteral("body"))) {
        request.body = report.value(QStringLiteral("body")).toMap();
    }
    text(QStringLiteral("method"), request.method);
    text(QStringLiteral("url"), request.url);
    text(QStringLiteral("type"), request.type);
    text(QStringLiteral("statusText"), request.statusText);
    text(QStringLiteral("failure"), request.failure);
    if (report.contains(QStringLiteral("status"))) {
        request.status = report.value(QStringLiteral("status")).toInt();
    }
    if (report.contains(QStringLiteral("size"))) {
        request.size = report.value(QStringLiteral("size")).toLongLong();
    }
    if (report.contains(QStringLiteral("requestHeaders"))) {
        request.requestHeaders = headersFrom(report.value(QStringLiteral("requestHeaders")));
    }
    if (report.contains(QStringLiteral("responseHeaders"))) {
        request.responseHeaders = headersFrom(report.value(QStringLiteral("responseHeaders")));
    }
    if (report.contains(QStringLiteral("sentBody"))) {
        request.sentBody = report.value(QStringLiteral("sentBody")).toBool();
    }
}

AgentNetwork::Reading AgentNetwork::read(const QString &tabId, bool failedOnly, quint64 since) const
{
    Reading reading;
    reading.cursor = since;
    const auto found = m_buffers.constFind(tabId);
    if (found == m_buffers.cend()) {
        return reading;
    }
    for (const auto &request : found->requests) {
        if (request.id <= since) {
            continue;
        }
        reading.cursor = request.id;
        if (!failedOnly || failed(request)) {
            reading.requests.append(request);
        }
    }
    reading.truncated = found->evictedThrough > since;
    return reading;
}

std::optional<AgentNetwork::Request> AgentNetwork::find(const QString &tabId, quint64 id) const
{
    const auto found = m_buffers.constFind(tabId);
    if (found == m_buffers.cend()) {
        return std::nullopt;
    }
    for (const auto &request : found->requests) {
        if (request.id == id) {
            return request;
        }
    }
    return std::nullopt;
}

void AgentNetwork::forget(const QString &tabId) { m_buffers.remove(tabId); }

void AgentNetwork::forgetAll() { m_buffers.clear(); }

bool AgentNetwork::failed(const Request &request)
{
    return !request.failure.isEmpty() || request.status >= 400;
}

QVector<AgentNetwork::Header> AgentNetwork::redacted(const QVector<Header> &headers)
{
    auto shown = headers;
    for (auto &header : shown) {
        if (!credentialHeaders().contains(header.name.toLower())) {
            continue;
        }
        const auto count = valueCount(header.name, header.value);
        header.value = count == 1 ? QStringLiteral("<redacted, 1 value>")
                                  : QStringLiteral("<redacted, %1 values>").arg(count);
    }
    return shown;
}

} // namespace omaweb
