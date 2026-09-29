#pragma once

#include <QHash>
#include <QString>
#include <QVector>

namespace omaweb {

// What the pages of Agent tabs have said to their console, kept for `console`
// (ADR 0051).
//
// A tab's messages belong to the document that logged them, so a new document
// starts the tab's buffer again, the way a page's own console does. Each tab
// keeps its most recent messages only, each cut to a length an Agent can read,
// and a cursor numbers every message the browser has kept since it started,
// so `--since` answers with what arrived after the last call. Messages live in
// memory and nowhere else.
class AgentConsole final {
public:
    // The engine's own order, so a level can be carried as the number it
    // reports.
    enum Level {
        Info = 0,
        Warning = 1,
        Error = 2,
    };

    static constexpr qsizetype maximumMessages = 500;
    static constexpr qsizetype maximumMessageLength = 2000;

    struct Message {
        quint64 cursor = 0;
        Level level = Info;
        QString text;
        QString source;
        int line = 0;
    };

    struct Reading {
        // In the order they were logged.
        QVector<Message> messages;
        // What to pass as `since` next time: the newest message the tab has
        // kept, whether or not it was at this level.
        quint64 cursor = 0;
        // Messages newer than `since` that were pushed out of the buffer
        // before this call could read them.
        bool truncated = false;
    };

    // `document` is anything that changes when the tab's page loads another
    // document, including when the tab's page is built again. A level the
    // engine names that is not one of the three is read as Info.
    void record(const QString &tabId, const QString &document, int level, const QString &text,
        const QString &source, int line);
    // Messages at `threshold` or more serious, newer than `since`.
    Reading read(const QString &tabId, Level threshold, quint64 since) const;
    void forget(const QString &tabId);
    void forgetAll();

    // The name a request gives a level, and the name an answer gives one.
    static bool parseThreshold(const QString &name, Level *level);
    static QString levelName(Level level);

private:
    struct Buffer {
        QString document;
        QVector<Message> messages;
        // The newest cursor pushed out, for telling a reader it missed some.
        quint64 evictedThrough = 0;
    };

    QHash<QString, Buffer> m_buffers;
    quint64 m_nextCursor = 0;
};

} // namespace omaweb
