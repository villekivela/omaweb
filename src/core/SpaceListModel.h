#pragma once

#include <QAbstractListModel>
#include <QVector>

namespace tanto {

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

private:
    QVector<SpaceState> m_spaces;
};

} // namespace tanto
