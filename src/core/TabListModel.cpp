#include "TabListModel.h"

namespace omaweb {

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
    case IconUrlRole:
        return tab.iconUrl;
    case PinnedRole:
        return tab.pinned;
    case ActiveRole:
        return tab.active;
    case LoadingRole:
        return tab.loading;
    case AudibleRole:
        return tab.audible;
    case MutedRole:
        return tab.muted;
    case ZoomRole:
        return tab.zoom;
    case KeepActiveRole:
        return tab.keepActive;
    case SoundSuppressedRole:
        return tab.soundSuppressed;
    default:
        return {};
    }
}

QHash<int, QByteArray> TabListModel::roleNames() const
{
    return {
        { IdRole, "tabId" },
        { SpaceIdRole, "spaceId" },
        { UrlRole, "tabUrl" },
        { TitleRole, "tabTitle" },
        { IconUrlRole, "tabIconUrl" },
        { PinnedRole, "pinned" },
        { ActiveRole, "active" },
        { LoadingRole, "loading" },
        { AudibleRole, "tabAudible" },
        { MutedRole, "tabMuted" },
        { ZoomRole, "tabZoom" },
        { KeepActiveRole, "tabKeepActive" },
        { SoundSuppressedRole, "tabSoundSuppressed" },
    };
}

const QVector<TabState> &TabListModel::items() const { return m_tabs; }

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

void TabListModel::insert(TabState tab, qsizetype row)
{
    const auto destination = qBound(qsizetype { 0 }, row, m_tabs.size());
    beginInsertRows({}, destination, destination);
    m_tabs.insert(destination, std::move(tab));
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

bool TabListModel::move(const QString &id, qsizetype destinationRow)
{
    if (destinationRow < 0 || destinationRow >= m_tabs.size()) {
        return false;
    }
    for (qsizetype sourceRow = 0; sourceRow < m_tabs.size(); ++sourceRow) {
        if (m_tabs.at(sourceRow).id != id) {
            continue;
        }
        if (sourceRow == destinationRow) {
            return true;
        }
        const auto destinationChild
            = destinationRow > sourceRow ? destinationRow + 1 : destinationRow;
        beginMoveRows({}, sourceRow, sourceRow, {}, destinationChild);
        m_tabs.move(sourceRow, destinationRow);
        endMoveRows();
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

} // namespace omaweb
