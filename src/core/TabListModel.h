#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QVector>

namespace tanto {

struct TabState {
    QString id;
    QString spaceId;
    QUrl url;
    QString title;
    QUrl iconUrl;
    bool pinned = false;
    bool active = false;
    bool loading = false;
    // A page's own doing, and a decision about it the reader made. Neither
    // outlives the session: a tab restored from the store is silent until its
    // page plays something, and asking to mute is asking to mute what is
    // playing now.
    bool audible = false;
    bool muted = false;
    QString rendererFailureReason;
};

class TabListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        SpaceIdRole,
        UrlRole,
        TitleRole,
        PinnedRole,
        ActiveRole,
        LoadingRole,
        // QML reads these positionally as Qt.UserRole + n, so new roles go on
        // the end. Inserting one renumbers every role after it.
        IconUrlRole,
        AudibleRole,
        MutedRole,
    };
    Q_ENUM(Role)

    explicit TabListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QVector<TabState> &items() const;
    const TabState *find(const QString &id) const;
    TabState *find(const QString &id);
    void reset(QVector<TabState> tabs);
    void append(TabState tab);
    bool remove(const QString &id);
    bool move(const QString &id, qsizetype destinationRow);
    void notifyChanged(const QString &id, const QList<int> &roles);

private:
    QVector<TabState> m_tabs;
};

} // namespace tanto
