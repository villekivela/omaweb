#include "HistorySearch.h"

#include "HistoryQuery.h"
#include "SessionStore.h"

#include <QFileInfo>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

namespace omaweb {

HistorySearch::HistorySearch(QString dataRoot, QObject *parent)
    : QObject(parent)
    , m_dataRoot(std::move(dataRoot))
    , m_connectionPrefix(QStringLiteral("omaweb-history-%1")
              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

HistorySearch::~HistorySearch()
{
    const auto spaceIds = m_databases.keys();
    for (auto connection : m_databases) {
        connection.close();
    }
    m_databases.clear();
    for (const auto &spaceId : spaceIds) {
        QSqlDatabase::removeDatabase(QStringLiteral("%1-%2").arg(m_connectionPrefix, spaceId));
    }
}

void HistorySearch::search(
    const QString &spaceId, const QString &text, int limit, quint64 generation)
{
    if (m_delayMilliseconds > 0) {
        QThread::msleep(static_cast<unsigned long>(m_delayMilliseconds));
    }
    const auto connection = database(spaceId);
    if (!connection.isOpen()) {
        emit resultsReady(spaceId, {}, generation);
        return;
    }
    emit resultsReady(spaceId, history::suggestions(connection, text, limit), generation);
}

void HistorySearch::forgetSpace(const QString &spaceId)
{
    auto connection = m_databases.take(spaceId);
    if (!connection.isValid()) {
        return;
    }
    connection.close();
    connection = {};
    QSqlDatabase::removeDatabase(QStringLiteral("%1-%2").arg(m_connectionPrefix, spaceId));
}

void HistorySearch::setDelayForTests(int milliseconds) { m_delayMilliseconds = milliseconds; }

QSqlDatabase HistorySearch::database(const QString &spaceId)
{
    if (const auto it = m_databases.constFind(spaceId); it != m_databases.cend()) {
        return it.value();
    }

    const auto path = SessionStore::spaceDatabasePath(m_dataRoot, spaceId);
    // Opening would create an empty database, which for a Space that has been
    // deleted would put its directory back. A Space with nothing written yet
    // has no History to suggest either way.
    if (!QFileInfo::exists(path)) {
        return {};
    }
    auto connection = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"), QStringLiteral("%1-%2").arg(m_connectionPrefix, spaceId));
    connection.setDatabaseName(path);
    if (!connection.open()) {
        return {};
    }
    QSqlQuery pragma(connection);
    // A reader that cannot write cannot block the session's writes or damage
    // the Space if this thread is ever given something else to run.
    pragma.exec(QStringLiteral("PRAGMA query_only = 1"));
    m_databases.insert(spaceId, connection);
    return connection;
}

} // namespace omaweb
