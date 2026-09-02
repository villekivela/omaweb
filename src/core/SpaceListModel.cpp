#include "SpaceListModel.h"

namespace omaweb {

SpaceListModel::SpaceListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SpaceListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_spaces.size();
}

QVariant SpaceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_spaces.size()) {
        return {};
    }

    const auto &space = m_spaces.at(index.row());
    switch (role) {
    case IdRole:
        return space.id;
    case NameRole:
        return space.name;
    case ColorRole:
        return space.color;
    case ActiveRole:
        return space.active;
    default:
        return {};
    }
}

QHash<int, QByteArray> SpaceListModel::roleNames() const
{
    return {
        {IdRole, "spaceId"},
        {NameRole, "spaceName"},
        {ColorRole, "spaceColor"},
        {ActiveRole, "active"},
    };
}

const QVector<SpaceState> &SpaceListModel::items() const
{
    return m_spaces;
}

void SpaceListModel::reset(QVector<SpaceState> spaces)
{
    beginResetModel();
    m_spaces = std::move(spaces);
    endResetModel();
}

} // namespace omaweb
