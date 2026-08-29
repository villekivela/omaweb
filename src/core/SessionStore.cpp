#include "SessionStore.h"

#include <QDir>
#include <QDirIterator>
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
    const auto spaceConnectionNames = m_spaceConnectionNames.values();
    for (auto database : m_spaceDatabases) {
        database.close();
    }
    m_spaceDatabases.clear();
    m_spaceConnectionNames.clear();
    for (const auto &connectionName : spaceConnectionNames) {
        QSqlDatabase::removeDatabase(connectionName);
    }
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
    pragma.exec(QStringLiteral("PRAGMA journal_mode = DELETE"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    if (!executeSchema(errorMessage) || !migrateLegacyTabs(errorMessage)) {
        return false;
    }
    cleanupPendingSpaceDeletions();
    return true;
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
    QSqlQuery query(spaceDatabase(spaceId));
    query.prepare(QStringLiteral(
        "SELECT id, url, title, pinned, active FROM tabs ORDER BY pinned DESC, position"));
    query.exec();
    while (query.next()) {
        tabs.append({
            query.value(0).toString(),
            spaceId,
            QUrl(query.value(1).toString()),
            query.value(2).toString(),
            query.value(3).toBool(),
            query.value(4).toBool(),
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
        "INSERT INTO spaces(id, name, color, active, position) "
        "VALUES(?, ?, ?, ?, COALESCE((SELECT MAX(position) + 1 FROM spaces), 0)) "
        "ON CONFLICT(id) DO UPDATE SET name = excluded.name, color = excluded.color, active = excluded.active"));
    query.addBindValue(space.id);
    query.addBindValue(space.name);
    query.addBindValue(space.color);
    query.addBindValue(space.active);
    return query.exec();
}

bool SessionStore::setActiveSpace(const QString &spaceId)
{
    if (!m_database.transaction()) {
        return false;
    }

    QSqlQuery clearActive(m_database);
    if (!clearActive.exec(QStringLiteral("UPDATE spaces SET active = 0"))) {
        m_database.rollback();
        return false;
    }

    QSqlQuery setActive(m_database);
    setActive.prepare(QStringLiteral("UPDATE spaces SET active = 1 WHERE id = ?"));
    setActive.addBindValue(spaceId);
    if (!setActive.exec() || setActive.numRowsAffected() != 1) {
        m_database.rollback();
        return false;
    }

    return m_database.commit();
}

bool SessionStore::spaceHasSavedContent(const QString &spaceId) const
{
    const auto database = spaceDatabase(spaceId);
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT (SELECT COUNT(*) FROM tabs "
            "WHERE url != 'about:blank' OR pinned != 0 OR title != 'New tab') + "
            "(SELECT COUNT(*) FROM history) + "
            "(SELECT COUNT(*) FROM site_permissions)"))
        || !query.next()) {
        return true;
    }
    if (query.value(0).toInt() > 0) {
        return true;
    }

    const auto engineRoot = QDir(m_dataRoot).filePath(
        QStringLiteral("spaces/%1/engines").arg(spaceId));
    QDirIterator files(engineRoot, QDir::Files, QDirIterator::Subdirectories);
    return files.hasNext();
}

bool SessionStore::deleteSpace(const QString &spaceId, const QString &replacementActiveSpaceId)
{
    if (!m_database.transaction()) {
        return false;
    }

    if (!replacementActiveSpaceId.isEmpty()) {
        QSqlQuery clearActive(m_database);
        QSqlQuery setActive(m_database);
        setActive.prepare(QStringLiteral("UPDATE spaces SET active = 1 WHERE id = ?"));
        setActive.addBindValue(replacementActiveSpaceId);
        if (!clearActive.exec(QStringLiteral("UPDATE spaces SET active = 0"))
            || !setActive.exec() || setActive.numRowsAffected() != 1) {
            m_database.rollback();
            return false;
        }
    }

    QSqlQuery removeSpace(m_database);
    removeSpace.prepare(QStringLiteral("DELETE FROM spaces WHERE id = ?"));
    removeSpace.addBindValue(spaceId);
    if (!removeSpace.exec() || removeSpace.numRowsAffected() != 1) {
        m_database.rollback();
        return false;
    }

    QSqlQuery rememberCleanup(m_database);
    rememberCleanup.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO pending_space_deletions(space_id) VALUES(?)"));
    rememberCleanup.addBindValue(spaceId);
    if (!rememberCleanup.exec() || !m_database.commit()) {
        m_database.rollback();
        return false;
    }

    closeSpaceDatabase(spaceId);
    cleanupPendingSpaceDeletions();
    return true;
}

bool SessionStore::saveTab(const TabState &tab, int position)
{
    QSqlQuery query(spaceDatabase(tab.spaceId));
    query.prepare(QStringLiteral(
        "INSERT INTO tabs(id, url, title, pinned, active, position) VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET url = excluded.url, "
        "title = excluded.title, pinned = excluded.pinned, active = excluded.active, position = excluded.position"));
    query.addBindValue(tab.id);
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
    auto database = spaceDatabase(spaceId);
    if (!database.transaction()) {
        return false;
    }

    QSqlQuery removeExisting(database);
    removeExisting.prepare(QStringLiteral("DELETE FROM tabs"));
    if (!removeExisting.exec()) {
        database.rollback();
        return false;
    }

    for (qsizetype index = 0; index < tabs.size(); ++index) {
        auto tab = tabs.at(index);
        tab.active = tab.id == activeTabId;
        if (!saveTab(tab, static_cast<int>(index))) {
            database.rollback();
            return false;
        }
    }

    return database.commit();
}

bool SessionStore::saveSpaceMove(const QString &sourceSpaceId,
    const QVector<TabState> &sourceTabs, const QString &sourceActiveTabId,
    const QString &destinationSpaceId, const QVector<TabState> &destinationTabs,
    const QString &destinationActiveTabId)
{
    spaceDatabase(sourceSpaceId);
    spaceDatabase(destinationSpaceId);
    const auto sourcePath = QDir(m_dataRoot).filePath(
        QStringLiteral("spaces/%1/browser.sqlite").arg(sourceSpaceId));
    const auto destinationPath = QDir(m_dataRoot).filePath(
        QStringLiteral("spaces/%1/browser.sqlite").arg(destinationSpaceId));

    QSqlQuery attachSource(m_database);
    attachSource.prepare(QStringLiteral("ATTACH DATABASE ? AS move_source"));
    attachSource.addBindValue(sourcePath);
    if (!attachSource.exec()) {
        return false;
    }
    QSqlQuery attachDestination(m_database);
    attachDestination.prepare(QStringLiteral("ATTACH DATABASE ? AS move_destination"));
    attachDestination.addBindValue(destinationPath);
    if (!attachDestination.exec()) {
        QSqlQuery(m_database).exec(QStringLiteral("DETACH DATABASE move_source"));
        return false;
    }

    const auto writeTabs = [this](const QString &schema, const QVector<TabState> &tabs,
                               const QString &activeTabId) {
        QSqlQuery remove(m_database);
        if (!remove.exec(QStringLiteral("DELETE FROM %1.tabs").arg(schema))) {
            return false;
        }
        for (qsizetype position = 0; position < tabs.size(); ++position) {
            const auto &tab = tabs.at(position);
            QSqlQuery insert(m_database);
            insert.prepare(QStringLiteral(
                               "INSERT INTO %1.tabs(id, url, title, pinned, active, position) "
                               "VALUES(?, ?, ?, ?, ?, ?)")
                               .arg(schema));
            insert.addBindValue(tab.id);
            insert.addBindValue(tab.url.toString());
            insert.addBindValue(tab.title);
            insert.addBindValue(tab.pinned);
            insert.addBindValue(tab.id == activeTabId);
            insert.addBindValue(position);
            if (!insert.exec()) {
                return false;
            }
        }
        return true;
    };

    bool saved = m_database.transaction();
    if (saved) {
        saved = writeTabs(QStringLiteral("move_source"), sourceTabs, sourceActiveTabId)
            && writeTabs(QStringLiteral("move_destination"), destinationTabs,
                destinationActiveTabId);
    }
    if (saved) {
        saved = m_database.commit();
    } else {
        m_database.rollback();
    }

    QSqlQuery detachDestination(m_database);
    detachDestination.exec(QStringLiteral("DETACH DATABASE move_destination"));
    QSqlQuery detachSource(m_database);
    detachSource.exec(QStringLiteral("DETACH DATABASE move_source"));
    return saved;
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
        CREATE TABLE IF NOT EXISTS pending_space_deletions (
            space_id TEXT PRIMARY KEY
        );
        CREATE TABLE IF NOT EXISTS schema_migrations (
            name TEXT PRIMARY KEY,
            state TEXT NOT NULL
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

bool SessionStore::migrateLegacyTabs(QString *errorMessage)
{
    QSqlQuery tableQuery(m_database);
    if (!tableQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'tabs'"))
        || !tableQuery.next() || tableQuery.value(0).toInt() == 0) {
        return true;
    }

    QSqlQuery recordMigration(m_database);
    recordMigration.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO schema_migrations(name, state) VALUES(?, 'copying')"));
    recordMigration.addBindValue(QStringLiteral("legacy-global-tabs-v1"));
    if (!recordMigration.exec()) {
        if (errorMessage) {
            *errorMessage = recordMigration.lastError().text();
        }
        return false;
    }

    QHash<QString, QVector<TabState>> tabsBySpace;
    QHash<QString, QString> activeTabBySpace;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "SELECT id, space_id, url, title, pinned, active "
            "FROM tabs ORDER BY space_id, pinned DESC, position"))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    while (query.next()) {
        TabState tab {
            query.value(0).toString(),
            query.value(1).toString(),
            QUrl(query.value(2).toString()),
            query.value(3).toString(),
            query.value(4).toBool(),
            query.value(5).toBool(),
            false,
            {},
        };
        tabsBySpace[tab.spaceId].append(tab);
        if (tab.active) {
            activeTabBySpace[tab.spaceId] = tab.id;
        }
    }

    for (auto it = tabsBySpace.cbegin(); it != tabsBySpace.cend(); ++it) {
        if (!saveTabs(it.key(), it.value(), activeTabBySpace.value(it.key()))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not migrate tabs for Space %1").arg(it.key());
            }
            return false;
        }
    }

    if (!m_database.transaction()) {
        return false;
    }
    QSqlQuery dropLegacy(m_database);
    QSqlQuery finishMigration(m_database);
    finishMigration.prepare(QStringLiteral("DELETE FROM schema_migrations WHERE name = ?"));
    finishMigration.addBindValue(QStringLiteral("legacy-global-tabs-v1"));
    if (!dropLegacy.exec(QStringLiteral("DROP TABLE tabs"))
        || !finishMigration.exec() || !m_database.commit()) {
        m_database.rollback();
        if (errorMessage) {
            *errorMessage = dropLegacy.lastError().isValid()
                ? dropLegacy.lastError().text()
                : finishMigration.lastError().text();
        }
        return false;
    }
    return true;
}

void SessionStore::cleanupPendingSpaceDeletions()
{
    QSqlQuery pending(m_database);
    if (!pending.exec(QStringLiteral("SELECT space_id FROM pending_space_deletions"))) {
        return;
    }

    QStringList removedSpaceIds;
    while (pending.next()) {
        const auto spaceId = pending.value(0).toString();
        closeSpaceDatabase(spaceId);
        const auto spacePath = QDir(m_dataRoot).filePath(
            QStringLiteral("spaces/%1").arg(spaceId));
        const QDir directory(spacePath);
        if (!directory.exists() || QDir(spacePath).removeRecursively()) {
            removedSpaceIds.append(spaceId);
        }
    }

    for (const auto &spaceId : removedSpaceIds) {
        QSqlQuery clearPending(m_database);
        clearPending.prepare(QStringLiteral(
            "DELETE FROM pending_space_deletions WHERE space_id = ?"));
        clearPending.addBindValue(spaceId);
        clearPending.exec();
    }
}

QSqlDatabase SessionStore::spaceDatabase(const QString &spaceId) const
{
    if (const auto it = m_spaceDatabases.constFind(spaceId); it != m_spaceDatabases.cend()) {
        return it.value();
    }

    const auto spaceRoot = QDir(m_dataRoot).filePath(QStringLiteral("spaces/%1").arg(spaceId));
    QDir().mkpath(spaceRoot);
    const auto connectionName = QStringLiteral("%1-space-%2").arg(m_connectionName, spaceId);
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(QDir(spaceRoot).filePath(QStringLiteral("browser.sqlite")));
    database.open();
    QSqlQuery pragma(database);
    pragma.exec(QStringLiteral("PRAGMA journal_mode = DELETE"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    QSqlQuery schema(database);
    schema.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tabs ("
        "id TEXT PRIMARY KEY, "
        "url TEXT NOT NULL, "
        "title TEXT NOT NULL, "
        "pinned INTEGER NOT NULL DEFAULT 0, "
        "active INTEGER NOT NULL DEFAULT 0, "
        "position INTEGER NOT NULL DEFAULT 0)"));
    schema.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS history ("
        "id INTEGER PRIMARY KEY, "
        "url TEXT NOT NULL, "
        "title TEXT NOT NULL, "
        "visited_at INTEGER NOT NULL)"));
    schema.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS site_permissions ("
        "origin TEXT NOT NULL, "
        "permission TEXT NOT NULL, "
        "decision INTEGER NOT NULL, "
        "PRIMARY KEY(origin, permission))"));
    m_spaceConnectionNames.insert(spaceId, connectionName);
    m_spaceDatabases.insert(spaceId, database);
    return database;
}

void SessionStore::closeSpaceDatabase(const QString &spaceId)
{
    const auto connectionName = m_spaceConnectionNames.take(spaceId);
    auto database = m_spaceDatabases.take(spaceId);
    if (database.isValid()) {
        database.close();
        database = {};
    }
    if (!connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(connectionName);
    }
}

} // namespace tanto
