#include "TabListModel.h"

namespace tanto {

TabListModel::TabListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TabListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tabs.size();
}

QVariant TabListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tabs.size()) {
        return {};
    }

    const auto &tab = m_tabs.at(index.row());
    switch (role) {
    case IdRole:
        return tab.id;
    case SpaceIdRole:
        return tab.spaceId;
    case UrlRole:
        return tab.url;
    case TitleRole:
        return tab.title;
    case PinnedRole:
        return tab.pinned;
    case ActiveRole:
        return tab.active;
    case LoadingRole:
        return tab.loading;
    default:
        return {};
    }
}

QHash<int, QByteArray> TabListModel::roleNames() const
{
    return {
        {IdRole, "tabId"},
        {SpaceIdRole, "spaceId"},
        {UrlRole, "tabUrl"},
        {TitleRole, "tabTitle"},
        {PinnedRole, "pinned"},
        {ActiveRole, "active"},
        {LoadingRole, "loading"},
    };
}

const QVector<TabState> &TabListModel::items() const
{
    return m_tabs;
}

const TabState *TabListModel::find(const QString &id) const
{
    for (const auto &tab : m_tabs) {
        if (tab.id == id) {
            return &tab;
        }
    }
    return nullptr;
}

TabState *TabListModel::find(const QString &id)
{
    for (auto &tab : m_tabs) {
        if (tab.id == id) {
            return &tab;
        }
    }
    return nullptr;
}

void TabListModel::reset(QVector<TabState> tabs)
{
    beginResetModel();
    m_tabs = std::move(tabs);
    endResetModel();
}

void TabListModel::append(TabState tab)
{
    const auto row = m_tabs.size();
    beginInsertRows({}, row, row);
    m_tabs.append(std::move(tab));
    endInsertRows();
}

bool TabListModel::remove(const QString &id)
{
    for (qsizetype row = 0; row < m_tabs.size(); ++row) {
        if (m_tabs.at(row).id != id) {
            continue;
        }
        beginRemoveRows({}, row, row);
        m_tabs.removeAt(row);
        endRemoveRows();
        return true;
    }
    return false;
}

void TabListModel::notifyChanged(const QString &id, const QList<int> &roles)
{
    for (qsizetype row = 0; row < m_tabs.size(); ++row) {
        if (m_tabs.at(row).id == id) {
            emit dataChanged(index(row), index(row), roles);
            return;
        }
    }
}

} // namespace tanto
