#include "AgentConsole.h"

#include <utility>

namespace omaweb {

void AgentConsole::record(const QString &tabId, const QString &document, int level,
    const QString &text, const QString &source, int line)
{
    auto &buffer = m_buffers[tabId];
    if (buffer.messages.isEmpty() || buffer.document != document) {
        buffer = {};
        buffer.document = document;
    }
    Message message;
    message.cursor = ++m_nextCursor;
    message.level = level == Warning ? Warning : level == Error ? Error : Info;
    message.text = text.left(maximumMessageLength);
    message.source = source.left(maximumMessageLength);
    message.line = line;
    buffer.messages.append(std::move(message));
    while (buffer.messages.size() > maximumMessages) {
        buffer.evictedThrough = buffer.messages.constFirst().cursor;
        buffer.messages.removeFirst();
    }
}

void AgentConsole::start(const QString &tabId, const QString &document)
{
    const auto found = m_buffers.find(tabId);
    if (found != m_buffers.end() && found->document != document) {
        *found = {};
        found->document = document;
    }
}

AgentConsole::Reading AgentConsole::read(const QString &tabId, Level threshold, quint64 since) const
{
    Reading reading;
    reading.cursor = since;
    const auto found = m_buffers.constFind(tabId);
    if (found == m_buffers.cend()) {
        return reading;
    }
    for (const auto &message : found->messages) {
        if (message.cursor <= since) {
            continue;
        }
        reading.cursor = message.cursor;
        if (message.level >= threshold) {
            reading.messages.append(message);
        }
    }
    reading.truncated = found->evictedThrough > since;
    return reading;
}

void AgentConsole::forget(const QString &tabId) { m_buffers.remove(tabId); }

void AgentConsole::forgetAll() { m_buffers.clear(); }

bool AgentConsole::parseThreshold(const QString &name, Level *level)
{
    if (name.isEmpty() || name == u"all") {
        *level = Info;
    } else if (name == u"warning") {
        *level = Warning;
    } else if (name == u"error") {
        *level = Error;
    } else {
        return false;
    }
    return true;
}

QString AgentConsole::levelName(Level level)
{
    switch (level) {
    case Warning:
        return QStringLiteral("warning");
    case Error:
        return QStringLiteral("error");
    case Info:
        break;
    }
    return QStringLiteral("info");
}

} // namespace omaweb
