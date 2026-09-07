#pragma once

#include <QHash>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>

namespace omaweb {

// The Omnibar's History search, run away from the interface. It lives on a
// thread of its own and reads each Space database through a connection of its
// own: a QSqlDatabase belongs to the thread that opened it, so nothing here is
// shared with the connections the session writes through.
//
// A search never writes. It answers with the generation it was given, and
// deciding which answer is still wanted is the caller's.
class HistorySearch final : public QObject {
    Q_OBJECT

public:
    explicit HistorySearch(QString dataRoot, QObject *parent = nullptr);
    ~HistorySearch() override;

    HistorySearch(const HistorySearch &) = delete;
    HistorySearch &operator=(const HistorySearch &) = delete;

public slots:
    void search(const QString &spaceId, const QString &text, int limit, quint64 generation);
    // Releases the connection to one Space, for a Space that is being deleted.
    void forgetSpace(const QString &spaceId);
    // Holds each search open for this long. Only a test sets it, to have input
    // arrive while a search is still running.
    void setDelayForTests(int milliseconds);

signals:
    void resultsReady(const QString &spaceId, const QVariantList &suggestions, quint64 generation);

private:
    QSqlDatabase database(const QString &spaceId);

    QString m_dataRoot;
    QString m_connectionPrefix;
    QHash<QString, QSqlDatabase> m_databases;
    int m_delayMilliseconds = 0;
};

} // namespace omaweb
