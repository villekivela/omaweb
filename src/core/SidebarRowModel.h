#pragma once

#include "FolderState.h"
#include "TabListModel.h"

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

namespace omaweb {

// The ordinary list as the sidebar stands it: each Folder's row with its tabs
// under it, then the hairline, then the tabs in no Folder. One list, so a row
// moving in or out of a Folder is the same row moved and never built again,
// and collapsing a Folder only hides rows. A tab's row answers everything the
// tab model does; a Folder's answers the roles below.
class SidebarRowModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum class Kind { Tab, Folder, Divider };

    enum Role {
        // A name for the delegate chooser: `tab`, `folder` or `divider`.
        RowKindRole = TabListModel::FolderHiddenRole + 1,
        // How many rows the Folder holds, a split's being one.
        FolderMemberCountRole,
        FolderRowIdRole,
        FolderNameRole,
        FolderCollapsedRole,
    };
    Q_ENUM(Role)

    // The Folders are read through the reference, so a window that replaces
    // them before it replaces the tabs is read once, whole.
    SidebarRowModel(TabListModel &tabs, const QVector<FolderState> &folders);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Brings the rows to what the Folders and the tabs say now, moving the rows
    // that stay rather than building them again.
    void refresh();
    // A Space at rest lists no ordinary row, so it has no hairline to draw.
    void setAtRest(bool atRest);

private:
    struct Row {
        Kind kind = Kind::Tab;
        QString id {};

        bool operator==(const Row &) const = default;
    };

    QVector<Row> wantedRows() const;
    const FolderState *folder(const QString &id) const;
    void noteTabRows();

    TabListModel &m_tabs;
    const QVector<FolderState> &m_folders;
    QVector<Row> m_rows;
    QHash<QString, int> m_rowOfTab;
    bool m_atRest = false;
};

} // namespace omaweb
