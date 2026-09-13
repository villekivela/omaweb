#include "SpaceListModel.h"

#include <algorithm>

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

const QVector<SpaceState> &SpaceListModel::items() const { return m_spaces; }

void SpaceListModel::reset(QVector<SpaceState> spaces)
{
    const auto sameIdentity = spaces.size() == m_spaces.size()
        && std::ranges::equal(spaces, m_spaces,
            [](const SpaceState &left, const SpaceState &right) { return left.id == right.id; });
    if (sameIdentity) {
        const auto previous = m_spaces;
        m_spaces = std::move(spaces);
        for (qsizetype row = 0; row < m_spaces.size(); ++row) {
            QList<int> roles;
            if (previous.at(row).name != m_spaces.at(row).name) {
                roles.append(NameRole);
            }
            if (previous.at(row).color != m_spaces.at(row).color) {
                roles.append(ColorRole);
            }
            if (previous.at(row).active != m_spaces.at(row).active) {
                roles.append(ActiveRole);
            }
            if (!roles.isEmpty()) {
                emit dataChanged(index(row), index(row), roles);
            }
        }
        return;
    }
    beginResetModel();
    m_spaces = std::move(spaces);
    endResetModel();
}

} // namespace omaweb
