#pragma once

#include <QString>
#include <QVariantList>

class QSqlDatabase;

namespace omaweb::history {

// The History rows one Space keeps, and how many visits may arrive between two
// cleanups. A Space therefore never holds more than their sum: the cleanup is
// amortized over the visits rather than run on every one of them.
constexpr int retainedRows = 5000;
constexpr int cleanupBatch = 256;

// The Omnibar's suggestion search: substring matching over address and title,
// one row per address, most recently visited first. The interface reads it
// through a connection of the searching thread's own, and the session writes
// through its own, so the query lives here rather than beside either.
QVariantList suggestions(const QSqlDatabase &database, const QString &text, int limit);

// Drops every visit outside the retained window, newest kept.
bool trim(const QSqlDatabase &database);

} // namespace omaweb::history
