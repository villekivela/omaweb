#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace omaweb {

// What Agents did in the browser, one line per verb, kept for a week
// (ADR 0051).
//
// A line says who asked, where and what: the time, the connection's name, the
// Space and tab, the verb and its target, which is an address, a hint label or
// a name the Agent gave. It never holds what a page says, a value an Agent
// filled or the source it evaluated, so the log is not a second store of page
// content or of what the reader types.
//
// The file is JSON lines in the application data directory, readable by the
// reader alone. Lines older than the retention go when the log is opened, which
// is at every start, and while it runs.
class AgentActivityLog final : public QObject {
    Q_OBJECT

public:
    static constexpr qint64 retentionMs = 7LL * 24 * 60 * 60 * 1000;
    // A bound on a week of lines, so an Agent looping on a verb cannot fill the
    // disk. The oldest go first.
    static constexpr qsizetype maximumEntries = 100000;

    struct Entry {
        // Milliseconds since the epoch.
        qint64 time = 0;
        QString agent;
        QString spaceId;
        // The Space's name when the verb ran, so a line still reads after the
        // Space is renamed or deleted.
        QString space;
        QString tabId;
        // The tab's address when the verb ran.
        QString address;
        QString verb;
        QString target;
        // `ok`, or the code the verb was refused with.
        QString outcome;
    };

    // Opens the log in `directory` and removes what is past the retention.
    // An empty directory keeps the log in memory only.
    explicit AgentActivityLog(QString directory, QObject *parent = nullptr);

    QString path() const;
    void record(Entry entry);
    // Oldest first.
    const QList<Entry> &entries() const;

    // The lines the page shows, newest first: `time`, `agent`, `spaceId`,
    // `space`, `tabId`, `address`, `verb`, `target` and `outcome`. An empty
    // `agent` or `spaceId` filters nothing.
    Q_INVOKABLE QVariantList rows(const QString &agent = {}, const QString &spaceId = {}) const;
    // Every connection name in the log, sorted.
    Q_INVOKABLE QStringList agents() const;
    // Every Space in the log as `{id, name}`, by the name it last had there.
    Q_INVOKABLE QVariantList spaces() const;

signals:
    void recorded();

private:
    void load();
    // Drops what is past the retention or over the bound, and writes the file
    // again when anything went or `rewriteAnyway` asks.
    void prune(bool rewriteAnyway);
    void rewrite() const;
    void append(const Entry &entry) const;

    QString m_path;
    QList<Entry> m_entries;
};

} // namespace omaweb
