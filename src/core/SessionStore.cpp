#include "SessionStore.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace tanto {

SessionStore::SessionStore(QString dataRoot)
    : m_dataRoot(std::move(dataRoot))
    , m_connectionName(QStringLiteral("tanto-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

SessionStore::~SessionStore()
{
    if (m_database.isValid()) {
        m_database.close();
        m_database = {};
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool SessionStore::open(QString *errorMessage)
{
    if (!QDir().mkpath(m_dataRoot)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create data directory: %1").arg(m_dataRoot);
        }
        return false;
    }

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(QDir(m_dataRoot).filePath(QStringLiteral("state.sqlite")));
    if (!m_database.open()) {
        if (errorMessage) {
            *errorMessage = m_database.lastError().text();
        }
        return false;
    }

    QSqlQuery pragma(m_database);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    return executeSchema(errorMessage);
}

QVector<SpaceState> SessionStore::loadSpaces() const
{
    QVector<SpaceState> spaces;
    QSqlQuery query(m_database);
    query.exec(QStringLiteral("SELECT id, name, color, active FROM spaces ORDER BY position"));
    while (query.next()) {
        spaces.append({
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toBool(),
        });
    }
    return spaces;
}

QVector<TabState> SessionStore::loadTabs(const QString &spaceId) const
{
    QVector<TabState> tabs;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, space_id, url, title, pinned, active "
        "FROM tabs WHERE space_id = ? ORDER BY pinned DESC, position"));
    query.addBindValue(spaceId);
    query.exec();
    while (query.next()) {
        tabs.append({
            query.value(0).toString(),
            query.value(1).toString(),
            QUrl(query.value(2).toString()),
            query.value(3).toString(),
            query.value(4).toBool(),
            query.value(5).toBool(),
            false,
            {},
        });
    }
    return tabs;
}

bool SessionStore::saveSpace(const SpaceState &space)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO spaces(id, name, color, active, position) VALUES(?, ?, ?, ?, 0) "
        "ON CONFLICT(id) DO UPDATE SET name = excluded.name, color = excluded.color, active = excluded.active"));
    query.addBindValue(space.id);
    query.addBindValue(space.name);
    query.addBindValue(space.color);
    query.addBindValue(space.active);
    return query.exec();
}

bool SessionStore::saveTab(const TabState &tab, int position)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO tabs(id, space_id, url, title, pinned, active, position) VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET space_id = excluded.space_id, url = excluded.url, "
        "title = excluded.title, pinned = excluded.pinned, active = excluded.active, position = excluded.position"));
    query.addBindValue(tab.id);
    query.addBindValue(tab.spaceId);
    query.addBindValue(tab.url.toString());
    query.addBindValue(tab.title);
    query.addBindValue(tab.pinned);
    query.addBindValue(tab.active);
    query.addBindValue(position);
    return query.exec();
}

bool SessionStore::saveTabs(const QString &spaceId, const QVector<TabState> &tabs,
    const QString &activeTabId)
{
    if (!m_database.transaction()) {
        return false;
    }

    QSqlQuery removeExisting(m_database);
    removeExisting.prepare(QStringLiteral("DELETE FROM tabs WHERE space_id = ?"));
    removeExisting.addBindValue(spaceId);
    if (!removeExisting.exec()) {
        m_database.rollback();
        return false;
    }

    for (qsizetype index = 0; index < tabs.size(); ++index) {
        auto tab = tabs.at(index);
        tab.active = tab.id == activeTabId;
        if (!saveTab(tab, static_cast<int>(index))) {
            m_database.rollback();
            return false;
        }
    }

    return m_database.commit();
}

QString SessionStore::dataRoot() const
{
    return m_dataRoot;
}

QString SessionStore::engineProfilePath(const QString &spaceId, const QString &engineName) const
{
    const auto path = QDir(m_dataRoot).filePath(
        QStringLiteral("spaces/%1/engines/%2").arg(spaceId, engineName));
    QDir().mkpath(path);
    return path;
}

bool SessionStore::executeSchema(QString *errorMessage)
{
    static constexpr auto schema = R"SQL(
        CREATE TABLE IF NOT EXISTS spaces (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            color TEXT NOT NULL,
            active INTEGER NOT NULL DEFAULT 0,
            position INTEGER NOT NULL DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS tabs (
            id TEXT PRIMARY KEY,
            space_id TEXT NOT NULL REFERENCES spaces(id) ON DELETE CASCADE,
            url TEXT NOT NULL,
            title TEXT NOT NULL,
            pinned INTEGER NOT NULL DEFAULT 0,
            active INTEGER NOT NULL DEFAULT 0,
            position INTEGER NOT NULL DEFAULT 0
        );
    )SQL";

    const auto statements = QString::fromUtf8(schema).split(';', Qt::SkipEmptyParts);
    for (const auto &statement : statements) {
        const auto sql = statement.trimmed();
        if (sql.isEmpty()) {
            continue;
        }
        QSqlQuery query(m_database);
        if (!query.exec(sql)) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
    }
    return true;
}

} // namespace tanto
