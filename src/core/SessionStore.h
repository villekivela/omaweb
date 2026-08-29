#pragma once

#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QSqlDatabase>

namespace tanto {

class SessionStore final {
public:
    explicit SessionStore(QString dataRoot);
    ~SessionStore();

    SessionStore(const SessionStore &) = delete;
    SessionStore &operator=(const SessionStore &) = delete;

    bool open(QString *errorMessage = nullptr);
    QVector<SpaceState> loadSpaces() const;
    QVector<TabState> loadTabs(const QString &spaceId) const;
    bool saveSpace(const SpaceState &space);
    bool saveTab(const TabState &tab, int position);
    bool removeTab(const QString &id);
    bool setActiveTab(const QString &spaceId, const QString &tabId);

    QString dataRoot() const;
    QString engineProfilePath(const QString &spaceId, const QString &engineName) const;

private:
    bool executeSchema(QString *errorMessage);

    QString m_dataRoot;
    QString m_connectionName;
    QSqlDatabase m_database;
};

} // namespace tanto
