#pragma once

#include <QAbstractListModel>
#include <QVector>

namespace omaweb {

struct SpaceState {
    QString id;
    QString name;
    QString color;
    bool active = false;
};

class SpaceListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ColorRole,
        ActiveRole,
    };
    Q_ENUM(Role)

    explicit SpaceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    const QVector<SpaceState> &items() const;
    void reset(QVector<SpaceState> spaces);
    // Where one Space sits in the list, or -1 for an id the list does not
    // hold. Asked by a command that has to work out where a Space is going
    // before it can ask for the move.
    qsizetype rowOf(const QString &id) const;
    // The order Spaces are listed in is the reader's, so a move reports itself
    // as a move rather than as a new list: a sidebar and a Settings row that
    // are watching this model keep their delegates and their focus across it.
    bool move(const QString &id, qsizetype destinationRow);

private:
    QVector<SpaceState> m_spaces;
};

} // namespace omaweb
