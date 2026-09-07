#include "HistoryQuery.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUrl>
#include <QVariantMap>

namespace omaweb::history {

QVariantList suggestions(const QSqlDatabase &database, const QString &text, int limit)
{
    QVariantList suggestions;
    QSqlQuery query(database);
    const auto pattern = QStringLiteral("%%1%").arg(text);
    query.prepare(QStringLiteral("SELECT url, MAX(title), MAX(visited_at) FROM history "
                                 "WHERE ? = '' OR url LIKE ? OR title LIKE ? "
                                 "GROUP BY url ORDER BY MAX(visited_at) DESC LIMIT ?"));
    query.addBindValue(text);
    query.addBindValue(pattern);
    query.addBindValue(pattern);
    query.addBindValue(limit);
    if (!query.exec()) {
        return suggestions;
    }
    while (query.next()) {
        QVariantMap item;
        item.insert(QStringLiteral("url"), query.value(0).toUrl());
        item.insert(QStringLiteral("title"), query.value(1).toString());
        item.insert(QStringLiteral("visitedAt"), query.value(2).toLongLong());
        suggestions.append(item);
    }
    return suggestions;
}

bool trim(const QSqlDatabase &database)
{
    // The oldest visit worth keeping, by the same order the suggestions use.
    // Timestamps repeat -- a redirect chain lands several visits in one
    // millisecond -- so the row is named by its id as well, or a boundary
    // shared by more visits than a batch holds would keep more than the bound.
    QSqlQuery boundary(database);
    boundary.prepare(QStringLiteral("SELECT visited_at, id FROM history "
                                    "ORDER BY visited_at DESC, id DESC LIMIT 1 OFFSET ?"));
    boundary.addBindValue(retainedRows - 1);
    if (!boundary.exec()) {
        return false;
    }
    if (!boundary.next()) {
        // Fewer visits than the bound, so there is nothing outside it.
        return true;
    }
    const auto oldestKeptVisit = boundary.value(0);
    const auto oldestKeptId = boundary.value(1);

    // Both halves come off the recency index: everything older than that
    // millisecond, then the visits inside it that the boundary is ahead of.
    // Asking for the rows outside the window by id instead would scan the
    // whole table on every batch.
    QSqlQuery cleanup(database);
    cleanup.prepare(QStringLiteral("DELETE FROM history WHERE visited_at < ? "
                                   "OR (visited_at = ? AND id < ?)"));
    cleanup.addBindValue(oldestKeptVisit);
    cleanup.addBindValue(oldestKeptVisit);
    cleanup.addBindValue(oldestKeptId);
    return cleanup.exec();
}

} // namespace omaweb::history
