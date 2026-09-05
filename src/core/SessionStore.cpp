#include "SessionStore.h"

#include <QDir>
#include <QDirIterator>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>
#include <QUuid>

namespace omaweb {
namespace {

    constexpr int retainedHistoryRows = 5000;

} // namespace

SessionStore::SessionStore(QString dataRoot)
    : m_dataRoot(std::move(dataRoot))
    , m_connectionName(
          QStringLiteral("omaweb-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
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
    // Session state is written whenever a page reports a new address or title,
    // so writes are frequent and small. A rollback journal creates and deletes
    // a file for each one; a write-ahead log appends instead.
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
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
    query.prepare(QStringLiteral("SELECT id, url, title, pinned, active, zoom, muted, keep_active "
                                 "FROM tabs ORDER BY pinned DESC, position"));
    query.exec();
    while (query.next()) {
        // Named rather than positional: a tab has state the store does not
        // keep — what it is playing, why its renderer stopped — and a field
        // added among those silently took the value of the one beside it.
        tabs.append(TabState {
            .id = query.value(0).toString(),
            .spaceId = spaceId,
            .url = QUrl(query.value(1).toString()),
            .title = query.value(2).toString(),
            .pinned = query.value(3).toBool(),
            .active = query.value(4).toBool(),
            .muted = query.value(6).toBool(),
            .zoom = query.value(5).toDouble(),
            .keepActive = query.value(7).toBool(),
        });
    }
    return tabs;
}

// The tabs a Space has lost, newest first, which is the order they come back
// in. Only what the session keeps about a tab is here: a reopened tab is a new
// tab at a saved address, not the page that was closed.
QVector<TabState> SessionStore::loadClosedTabs(const QString &spaceId) const
{
    QVector<TabState> tabs;
    QSqlQuery query(spaceDatabase(spaceId));
    query.prepare(QStringLiteral("SELECT id, url, title, pinned, zoom, muted, keep_active "
                                 "FROM closed_tabs ORDER BY position"));
    query.exec();
    while (query.next()) {
        tabs.append(TabState {
            .id = query.value(0).toString(),
            .spaceId = spaceId,
            .url = QUrl(query.value(1).toString()),
            .title = query.value(2).toString(),
            .pinned = query.value(3).toBool(),
            .muted = query.value(5).toBool(),
            .zoom = query.value(4).toDouble(),
            .keepActive = query.value(6).toBool(),
        });
    }
    return tabs;
}

// The whole stack is rewritten rather than pushed onto: it is at most a few
// dozen rows, and one statement per close would still have to trim the far end.
bool SessionStore::saveClosedTabs(const QString &spaceId, const QVector<TabState> &tabs)
{
    auto database = spaceDatabase(spaceId);
    if (!database.transaction()) {
        return false;
    }

    QSqlQuery removeExisting(database);
    if (!removeExisting.exec(QStringLiteral("DELETE FROM closed_tabs"))) {
        database.rollback();
        return false;
    }

    for (qsizetype position = 0; position < tabs.size(); ++position) {
        const auto &tab = tabs.at(position);
        QSqlQuery insert(database);
        insert.prepare(
            QStringLiteral("INSERT INTO closed_tabs"
                           "(id, url, title, pinned, zoom, muted, keep_active, position) "
                           "VALUES(?, ?, ?, ?, ?, ?, ?, ?)"));
        insert.addBindValue(tab.id);
        insert.addBindValue(tab.url.toString());
        insert.addBindValue(tab.title);
        insert.addBindValue(tab.pinned);
        insert.addBindValue(tab.zoom);
        insert.addBindValue(tab.muted);
        insert.addBindValue(tab.keepActive);
        insert.addBindValue(static_cast<int>(position));
        if (!insert.exec()) {
            database.rollback();
            return false;
        }
    }

    return database.commit();
}

bool SessionStore::saveSpace(const SpaceState &space)
{
    QSqlQuery query(m_database);
    query.prepare(
        QStringLiteral("INSERT INTO spaces(id, name, color, active, position) "
                       "VALUES(?, ?, ?, ?, COALESCE((SELECT MAX(position) + 1 FROM spaces), 0)) "
                       "ON CONFLICT(id) DO UPDATE SET name = excluded.name, color = "
                       "excluded.color, active = excluded.active"));
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
    if (!query.exec(
            QStringLiteral("SELECT (SELECT COUNT(*) FROM tabs "
                           "WHERE url != 'about:blank' OR pinned != 0 OR title != 'New tab') + "
                           "(SELECT COUNT(*) FROM history) + "
                           "(SELECT COUNT(*) FROM site_permissions)"))
        || !query.next()) {
        return true;
    }
    if (query.value(0).toInt() > 0) {
        return true;
    }

    const auto engineRoot
        = QDir(m_dataRoot).filePath(QStringLiteral("spaces/%1/engines").arg(spaceId));
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
        if (!clearActive.exec(QStringLiteral("UPDATE spaces SET active = 0")) || !setActive.exec()
            || setActive.numRowsAffected() != 1) {
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
    rememberCleanup.prepare(
        QStringLiteral("INSERT OR IGNORE INTO pending_space_deletions(space_id) VALUES(?)"));
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
        "INSERT INTO tabs"
        "(id, url, title, pinned, active, position, zoom, muted, keep_active) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET url = excluded.url, "
        "title = excluded.title, pinned = excluded.pinned, active = excluded.active, "
        "position = excluded.position, zoom = excluded.zoom, muted = excluded.muted, "
        "keep_active = excluded.keep_active"));
    query.addBindValue(tab.id);
    query.addBindValue(tab.url.toString());
    query.addBindValue(tab.title);
    query.addBindValue(tab.pinned);
    query.addBindValue(tab.active);
    query.addBindValue(position);
    query.addBindValue(tab.zoom);
    query.addBindValue(tab.muted);
    query.addBindValue(tab.keepActive);
    return query.exec();
}

bool SessionStore::saveTabs(
    const QString &spaceId, const QVector<TabState> &tabs, const QString &activeTabId)
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

bool SessionStore::saveSpaceMove(const QString &sourceSpaceId, const QVector<TabState> &sourceTabs,
    const QString &sourceActiveTabId, const QString &destinationSpaceId,
    const QVector<TabState> &destinationTabs, const QString &destinationActiveTabId)
{
    spaceDatabase(sourceSpaceId);
    spaceDatabase(destinationSpaceId);
    const auto sourcePath
        = QDir(m_dataRoot).filePath(QStringLiteral("spaces/%1/browser.sqlite").arg(sourceSpaceId));
    const auto destinationPath
        = QDir(m_dataRoot)
              .filePath(QStringLiteral("spaces/%1/browser.sqlite").arg(destinationSpaceId));

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

    const auto writeTabs
        = [this](const QString &schema, const QVector<TabState> &tabs, const QString &activeTabId) {
              QSqlQuery remove(m_database);
              if (!remove.exec(QStringLiteral("DELETE FROM %1.tabs").arg(schema))) {
                  return false;
              }
              for (qsizetype position = 0; position < tabs.size(); ++position) {
                  const auto &tab = tabs.at(position);
                  QSqlQuery insert(m_database);
                  insert.prepare(QStringLiteral("INSERT INTO %1.tabs"
                                                "(id, url, title, pinned, active, position, zoom, "
                                                "muted, keep_active) "
                                                "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)")
                          .arg(schema));
                  insert.addBindValue(tab.id);
                  insert.addBindValue(tab.url.toString());
                  insert.addBindValue(tab.title);
                  insert.addBindValue(tab.pinned);
                  insert.addBindValue(tab.id == activeTabId);
                  insert.addBindValue(position);
                  insert.addBindValue(tab.zoom);
                  insert.addBindValue(tab.muted);
                  insert.addBindValue(tab.keepActive);
                  if (!insert.exec()) {
                      return false;
                  }
              }
              return true;
          };

    bool saved = m_database.transaction();
    if (saved) {
        saved = writeTabs(QStringLiteral("move_source"), sourceTabs, sourceActiveTabId)
            && writeTabs(
                QStringLiteral("move_destination"), destinationTabs, destinationActiveTabId);
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

// Small, window-shaped settings the interface owns — how wide a panel was left,
// not what a Space contains. They are named rather than columned so a new one
// costs no migration.
QString SessionStore::preference(const QString &name, const QString &fallback) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT value FROM preferences WHERE name = ?"));
    query.addBindValue(name);
    if (!query.exec() || !query.next()) {
        return fallback;
    }
    return query.value(0).toString();
}

bool SessionStore::savePreference(const QString &name, const QString &value)
{
    if (name.isEmpty()) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO preferences(name, value) VALUES(?, ?) "
                                 "ON CONFLICT(name) DO UPDATE SET value = excluded.value"));
    query.addBindValue(name);
    query.addBindValue(value);
    return query.exec();
}

bool SessionStore::recordVisit(const QString &spaceId, const QUrl &url, const QString &title)
{
    QSqlQuery query(spaceDatabase(spaceId));
    query.prepare(QStringLiteral("INSERT INTO history(url, title, visited_at) VALUES(?, ?, ?)"));
    query.addBindValue(url.toString());
    query.addBindValue(title);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

QVariantList SessionStore::historySuggestions(
    const QString &spaceId, const QString &text, int limit) const
{
    QVariantList suggestions;
    QSqlQuery query(spaceDatabase(spaceId));
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

QVariantList SessionStore::history(const QString &spaceId, const QString &text, int limit) const
{
    QVariantList rows;
    QSqlQuery query(spaceDatabase(spaceId));
    const auto pattern = QStringLiteral("%%1%").arg(text);
    query.prepare(QStringLiteral("SELECT id, url, title, visited_at FROM history "
                                 "WHERE ? = '' OR url LIKE ? OR title LIKE ? "
                                 "ORDER BY visited_at DESC, id DESC LIMIT ?"));
    query.addBindValue(text);
    query.addBindValue(pattern);
    query.addBindValue(pattern);
    query.addBindValue(limit);
    if (!query.exec()) {
        return rows;
    }
    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), query.value(0).toLongLong());
        row.insert(QStringLiteral("url"), query.value(1).toUrl());
        row.insert(QStringLiteral("title"), query.value(2).toString());
        row.insert(QStringLiteral("visitedAt"), query.value(3).toLongLong());
        rows.append(row);
    }
    return rows;
}

bool SessionStore::deleteHistoryVisit(const QString &spaceId, qint64 id)
{
    QSqlQuery query(spaceDatabase(spaceId));
    query.prepare(QStringLiteral("DELETE FROM history WHERE id = ?"));
    query.addBindValue(id);
    return query.exec() && query.numRowsAffected() == 1;
}

bool SessionStore::deleteHistoryOrigin(const QString &spaceId, const QString &origin)
{
    auto database = spaceDatabase(spaceId);
    QSqlQuery select(database);
    if (!select.exec(QStringLiteral("SELECT id, url FROM history"))) {
        return false;
    }
    QList<qint64> ids;
    while (select.next()) {
        QUrl rowUrl(select.value(1).toString());
        QUrl rowOrigin;
        rowOrigin.setScheme(rowUrl.scheme().toLower());
        rowOrigin.setHost(rowUrl.host().toLower());
        const auto port = rowUrl.port(-1);
        const auto defaultPort
            = rowUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0 ? 443 : 80;
        if (port != -1 && port != defaultPort) {
            rowOrigin.setPort(port);
        }
        if (rowOrigin.toString(QUrl::FullyEncoded) == origin) {
            ids.append(select.value(0).toLongLong());
        }
    }
    if (!database.transaction()) {
        return false;
    }
    for (const auto id : ids) {
        QSqlQuery remove(database);
        remove.prepare(QStringLiteral("DELETE FROM history WHERE id = ?"));
        remove.addBindValue(id);
        if (!remove.exec()) {
            database.rollback();
            return false;
        }
    }
    return database.commit();
}

bool SessionStore::deleteHistorySince(const QString &spaceId, qint64 since)
{
    QSqlQuery query(spaceDatabase(spaceId));
    query.prepare(since > 0 ? QStringLiteral("DELETE FROM history WHERE visited_at >= ?")
                            : QStringLiteral("DELETE FROM history"));
    if (since > 0) {
        query.addBindValue(since);
    }
    return query.exec();
}

bool SessionStore::clearPermissionsSince(const QString &spaceId, qint64 since)
{
    QSqlQuery query(spaceDatabase(spaceId));
    query.prepare(since > 0 ? QStringLiteral("DELETE FROM site_permissions WHERE updated_at >= ?")
                            : QStringLiteral("DELETE FROM site_permissions"));
    if (since > 0) {
        query.addBindValue(since);
    }
    return query.exec();
}

int SessionStore::permissionDecision(
    const QString &spaceId, const QString &origin, const QString &permission) const
{
    QSqlQuery query(spaceDatabase(spaceId));
    query.prepare(QStringLiteral(
        "SELECT decision FROM site_permissions WHERE origin = ? AND permission = ?"));
    query.addBindValue(origin);
    query.addBindValue(permission);
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool SessionStore::savePermissionDecision(
    const QString &spaceId, const QString &origin, const QString &permission, int decision)
{
    QSqlQuery query(spaceDatabase(spaceId));
    query.prepare(QStringLiteral(
        "INSERT INTO site_permissions(origin, permission, decision, updated_at) VALUES(?, ?, ?, ?) "
        "ON CONFLICT(origin, permission) DO UPDATE SET decision = excluded.decision, "
        "updated_at = excluded.updated_at"));
    query.addBindValue(origin);
    query.addBindValue(permission);
    query.addBindValue(decision);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

QVariantList SessionStore::permissionsForOrigin(const QString &spaceId, const QString &origin) const
{
    QSqlQuery query(spaceDatabase(spaceId));
    query.prepare(QStringLiteral("SELECT permission, decision FROM site_permissions "
                                 "WHERE origin = ? ORDER BY permission"));
    query.addBindValue(origin);
    QVariantList permissions;
    if (!query.exec()) {
        return permissions;
    }
    while (query.next()) {
        permissions.append(QVariantMap {
            { QStringLiteral("permission"), query.value(0).toString() },
            { QStringLiteral("decision"), query.value(1).toInt() },
        });
    }
    return permissions;
}

bool SessionStore::clearPermissionsForOrigin(const QString &spaceId, const QString &origin)
{
    QSqlQuery query(spaceDatabase(spaceId));
    query.prepare(QStringLiteral("DELETE FROM site_permissions WHERE origin = ?"));
    query.addBindValue(origin);
    return query.exec();
}

bool SessionStore::recordDownload(const QString &id, const QUrl &url, const QString &path,
    const QString &state, qint64 receivedBytes, qint64 totalBytes)
{
    QSqlQuery query(m_database);
    query.prepare(
        QStringLiteral("INSERT INTO downloads(id, url, path, state, received_bytes, total_bytes, "
                       "error, created_at) "
                       "VALUES(?, ?, ?, ?, ?, ?, '', ?) "
                       "ON CONFLICT(id) DO UPDATE SET url = excluded.url, path = excluded.path, "
                       "state = excluded.state, received_bytes = excluded.received_bytes, "
                       "total_bytes = excluded.total_bytes"));
    query.addBindValue(id);
    query.addBindValue(url.toString());
    query.addBindValue(path);
    query.addBindValue(state);
    query.addBindValue(receivedBytes);
    query.addBindValue(totalBytes);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

bool SessionStore::updateDownload(const QString &id, const QString &state, qint64 receivedBytes,
    qint64 totalBytes, const QString &error)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE downloads SET state = ?, received_bytes = ?, total_bytes = ?, error = ? "
        "WHERE id = ?"));
    query.addBindValue(state);
    query.addBindValue(receivedBytes);
    query.addBindValue(totalBytes);
    query.addBindValue(error.isNull() ? QStringLiteral("") : error);
    query.addBindValue(id);
    return query.exec();
}

QVariantList SessionStore::downloadHistory() const
{
    QVariantList downloads;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "SELECT id, url, path, state, received_bytes, total_bytes, error, created_at "
            "FROM downloads ORDER BY created_at DESC"))) {
        return downloads;
    }
    while (query.next()) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), query.value(0).toString());
        item.insert(QStringLiteral("url"), query.value(1).toUrl());
        item.insert(QStringLiteral("path"), query.value(2).toString());
        item.insert(QStringLiteral("state"), query.value(3).toString());
        item.insert(QStringLiteral("receivedBytes"), query.value(4).toLongLong());
        item.insert(QStringLiteral("totalBytes"), query.value(5).toLongLong());
        item.insert(QStringLiteral("error"), query.value(6).toString());
        item.insert(QStringLiteral("createdAt"), query.value(7).toLongLong());
        downloads.append(item);
    }
    return downloads;
}

bool SessionStore::forgetDownload(const QString &id)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM downloads WHERE id = ?"));
    query.addBindValue(id);
    return query.exec() && query.numRowsAffected() > 0;
}

QString SessionStore::dataRoot() const { return m_dataRoot; }

QString SessionStore::engineProfilePath(const QString &spaceId, const QString &engineName) const
{
    const auto path
        = QDir(m_dataRoot)
              .filePath(QStringLiteral("spaces/%1/engines/%2").arg(spaceId, engineName));
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
        CREATE TABLE IF NOT EXISTS preferences (
            name TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS downloads (
            id TEXT PRIMARY KEY,
            url TEXT NOT NULL,
            path TEXT NOT NULL,
            state TEXT NOT NULL,
            received_bytes INTEGER NOT NULL DEFAULT 0,
            total_bytes INTEGER NOT NULL DEFAULT -1,
            error TEXT NOT NULL DEFAULT '',
            created_at INTEGER NOT NULL
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
    tableQuery.finish();

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
    if (!query.exec(QStringLiteral("SELECT id, space_id, url, title, pinned, active "
                                   "FROM tabs ORDER BY space_id, pinned DESC, position"))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    while (query.next()) {
        TabState tab {
            .id = query.value(0).toString(),
            .spaceId = query.value(1).toString(),
            .url = QUrl(query.value(2).toString()),
            .title = query.value(3).toString(),
            .pinned = query.value(4).toBool(),
            .active = query.value(5).toBool(),
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
    if (!dropLegacy.exec(QStringLiteral("DROP TABLE tabs")) || !finishMigration.exec()
        || !m_database.commit()) {
        m_database.rollback();
        if (errorMessage) {
            *errorMessage = dropLegacy.lastError().isValid() ? dropLegacy.lastError().text()
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
        const auto spacePath = QDir(m_dataRoot).filePath(QStringLiteral("spaces/%1").arg(spaceId));
        const QDir directory(spacePath);
        if (!directory.exists() || QDir(spacePath).removeRecursively()) {
            removedSpaceIds.append(spaceId);
        }
    }

    for (const auto &spaceId : removedSpaceIds) {
        QSqlQuery clearPending(m_database);
        clearPending.prepare(
            QStringLiteral("DELETE FROM pending_space_deletions WHERE space_id = ?"));
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
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    QSqlQuery schema(database);
    schema.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS tabs ("
                               "id TEXT PRIMARY KEY, "
                               "url TEXT NOT NULL, "
                               "title TEXT NOT NULL, "
                               "pinned INTEGER NOT NULL DEFAULT 0, "
                               "active INTEGER NOT NULL DEFAULT 0, "
                               "position INTEGER NOT NULL DEFAULT 0, "
                               "zoom REAL NOT NULL DEFAULT 1.0, "
                               "muted INTEGER NOT NULL DEFAULT 0, "
                               "keep_active INTEGER NOT NULL DEFAULT 0)"));
    // A Space whose store predates per-tab zoom keeps its tabs; the column is
    // added beside them, at the size every tab was drawn at before it existed.
    // Adding a column that is already there fails, and that failure is the
    // answer rather than a fault.
    schema.exec(QStringLiteral("ALTER TABLE tabs ADD COLUMN zoom REAL NOT NULL DEFAULT 1.0"));
    // Muting and Keep active arrived after zoom and are added the same way: a
    // Space whose store predates them keeps its tabs, unmuted and suspending
    // with their Space, which is where every tab was before the columns existed.
    schema.exec(QStringLiteral("ALTER TABLE tabs ADD COLUMN muted INTEGER NOT NULL DEFAULT 0"));
    schema.exec(
        QStringLiteral("ALTER TABLE tabs ADD COLUMN keep_active INTEGER NOT NULL DEFAULT 0"));
    // The tabs this Space has lost, so Reopen closed tab answers after a
    // restart as well as within a session. Position 0 is the newest.
    schema.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS closed_tabs ("
                               "id TEXT PRIMARY KEY, "
                               "url TEXT NOT NULL, "
                               "title TEXT NOT NULL, "
                               "pinned INTEGER NOT NULL DEFAULT 0, "
                               "zoom REAL NOT NULL DEFAULT 1.0, "
                               "muted INTEGER NOT NULL DEFAULT 0, "
                               "keep_active INTEGER NOT NULL DEFAULT 0, "
                               "position INTEGER NOT NULL DEFAULT 0)"));
    schema.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS history ("
                               "id INTEGER PRIMARY KEY, "
                               "url TEXT NOT NULL, "
                               "title TEXT NOT NULL, "
                               "visited_at INTEGER NOT NULL)"));
    // The Omnibar reads history on every keystroke, ordered by recency.
    schema.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS history_visited_at ON history(visited_at DESC)"));
    schema.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS history_url ON history(url)"));
    // One row per page load, kept forever, would make that read slower every
    // day the browser is used. Only the recent past is ever suggested.
    schema.exec(QStringLiteral("DELETE FROM history WHERE id NOT IN ("
                               "SELECT id FROM history ORDER BY visited_at DESC LIMIT %1)")
            .arg(retainedHistoryRows));
    schema.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS site_permissions ("
                               "origin TEXT NOT NULL, "
                               "permission TEXT NOT NULL, "
                               "decision INTEGER NOT NULL, "
                               "updated_at INTEGER NOT NULL DEFAULT 0, "
                               "PRIMARY KEY(origin, permission))"));
    schema.exec(QStringLiteral(
        "ALTER TABLE site_permissions ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 0"));
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

} // namespace omaweb
