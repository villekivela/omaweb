#include "SidebarRowModel.h"

#include <algorithm>

namespace omaweb {

SidebarRowModel::SidebarRowModel(TabListModel &tabs, const QVector<FolderState> &folders)
    : m_tabs(tabs)
    , m_folders(folders)
{
    connect(&m_tabs, &QAbstractItemModel::rowsInserted, this, &SidebarRowModel::refresh);
    connect(&m_tabs, &QAbstractItemModel::rowsRemoved, this, &SidebarRowModel::refresh);
    connect(&m_tabs, &QAbstractItemModel::rowsMoved, this, &SidebarRowModel::refresh);
    connect(&m_tabs, &QAbstractItemModel::modelReset, this, [this] {
        beginResetModel();
        m_rows = wantedRows();
        noteTabRows();
        endResetModel();
    });
    connect(&m_tabs, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex &from, const QModelIndex &to, const QList<int> &roles) {
            // A tab pinned or unpinned stands in another list.
            if (roles.contains(TabListModel::PinnedRole)) {
                refresh();
            }
            for (auto source = from.row(); source <= to.row(); ++source) {
                const auto row = m_rowOfTab.constFind(m_tabs.items().at(source).id);
                if (row != m_rowOfTab.constEnd()) {
                    const auto changed = createIndex(*row, 0);
                    emit dataChanged(changed, changed, roles);
                }
            }
        });
}

int SidebarRowModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant SidebarRowModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const auto &row = m_rows.at(index.row());
    if (role == RowKindRole) {
        return row.kind == Kind::Tab   ? QStringLiteral("tab")
            : row.kind == Kind::Folder ? QStringLiteral("folder")
                                       : QStringLiteral("divider");
    }
    if (row.kind == Kind::Tab) {
        const auto &tabs = m_tabs.items();
        const auto found = std::ranges::find(tabs, row.id, &TabState::id);
        return found == tabs.end() ? QVariant {}
                                   : m_tabs.data(m_tabs.index(found - tabs.begin(), 0), role);
    }
    const auto *held = row.kind == Kind::Folder ? folder(row.id) : nullptr;
    if (!held) {
        return {};
    }
    switch (role) {
    case FolderRowIdRole:
        return held->id;
    case FolderNameRole:
        return held->name;
    case FolderCollapsedRole:
        return held->collapsed;
    case FolderMemberCountRole: {
        const auto halves = std::ranges::count_if(held->tabIds, [this](const QString &id) {
            const auto *tab = m_tabs.find(id);
            return tab && !tab->splitPartnerId.isEmpty();
        });
        return static_cast<int>(held->tabIds.size() - halves / 2);
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> SidebarRowModel::roleNames() const
{
    auto names = m_tabs.roleNames();
    names.insert(RowKindRole, "rowKind");
    names.insert(FolderMemberCountRole, "folderMemberCount");
    names.insert(FolderRowIdRole, "folderRowId");
    names.insert(FolderNameRole, "folderName");
    names.insert(FolderCollapsedRole, "folderCollapsed");
    return names;
}

QVector<SidebarRowModel::Row> SidebarRowModel::wantedRows() const
{
    QVector<Row> rows;
    QSet<QString> held;
    for (const auto &folder : m_folders) {
        rows.append({Kind::Folder, folder.id});
        for (const auto &id : folder.tabIds) {
            if (m_tabs.find(id)) {
                rows.append({Kind::Tab, id});
                held.insert(id);
            }
        }
    }
    QVector<Row> loose;
    for (const auto &tab : m_tabs.items()) {
        if (!tab.pinned && !held.contains(tab.id)) {
            loose.append({Kind::Tab, tab.id});
        }
    }
    const auto listed = std::ranges::any_of(
        loose, [this](const Row &row) { return !m_atRest || m_tabs.find(row.id)->poppedOut; });
    if (!m_folders.isEmpty() && listed) {
        rows.append({Kind::Divider, {}});
    }
    return rows + loose;
}

const FolderState *SidebarRowModel::folder(const QString &id) const
{
    const auto found = std::ranges::find(m_folders, id, &FolderState::id);
    return found == m_folders.end() ? nullptr : &*found;
}

void SidebarRowModel::noteTabRows()
{
    m_rowOfTab.clear();
    for (qsizetype row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).kind == Kind::Tab) {
            m_rowOfTab.insert(m_rows.at(row).id, static_cast<int>(row));
        }
    }
}

void SidebarRowModel::refresh()
{
    const auto wanted = wantedRows();
    for (auto row = m_rows.size() - 1; row >= 0; --row) {
        if (!wanted.contains(m_rows.at(row))) {
            beginRemoveRows({}, static_cast<int>(row), static_cast<int>(row));
            m_rows.removeAt(row);
            endRemoveRows();
        }
    }
    for (qsizetype row = 0; row < wanted.size(); ++row) {
        if (row < m_rows.size() && m_rows.at(row) == wanted.at(row)) {
            continue;
        }
        const auto from = m_rows.indexOf(wanted.at(row), row);
        if (from < 0) {
            beginInsertRows({}, static_cast<int>(row), static_cast<int>(row));
            m_rows.insert(row, wanted.at(row));
            endInsertRows();
        } else {
            beginMoveRows(
                {}, static_cast<int>(from), static_cast<int>(from), {}, static_cast<int>(row));
            m_rows.move(from, row);
            endMoveRows();
        }
    }
    noteTabRows();
    for (qsizetype row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).kind == Kind::Folder) {
            const auto changed = createIndex(static_cast<int>(row), 0);
            emit dataChanged(
                changed, changed, {FolderNameRole, FolderCollapsedRole, FolderMemberCountRole});
        }
    }
}

void SidebarRowModel::setAtRest(bool atRest)
{
    if (atRest != m_atRest) {
        m_atRest = atRest;
        refresh();
    }
}

} // namespace omaweb
