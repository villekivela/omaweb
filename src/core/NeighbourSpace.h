#pragma once

#include "TabListModel.h"

#include <QObject>
#include <QSortFilterProxyModel>

namespace omaweb {

// One of the Spaces either side of the Space on show in footer order, as the
// sidebar draws it while a swipe brings it in: its pins and its ordinary rows,
// read-only. The window fills it from the store off the interface thread, so
// it is ready before a swipe starts. Empty, with no `spaceId`, when there is
// no Space to swipe to.
class NeighbourSpace final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString spaceId READ spaceId NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel *pinnedTabs READ pinnedTabs CONSTANT)
    Q_PROPERTY(QAbstractItemModel *unpinnedTabs READ unpinnedTabs CONSTANT)

public:
    explicit NeighbourSpace(QObject *parent = nullptr);

    QString spaceId() const;
    // Whether the rows are the Space's, read since it became this neighbour.
    bool ready() const;
    QAbstractItemModel *pinnedTabs();
    QAbstractItemModel *unpinnedTabs();

    // Names the Space this neighbour is. A different Space empties the rows
    // until `show` hands over its tabs.
    void name(const QString &spaceId);
    // The Space's tabs as the store keeps them. A Space resting on a blank tab
    // lists no ordinary row, as the Space on show does, except a tab shown in
    // a Tab window.
    void show(QVector<TabState> tabs);

signals:
    void changed();

private:
    QString m_spaceId;
    bool m_ready = false;
    TabListModel m_tabs;
    QSortFilterProxyModel m_pinnedTabs;
    QSortFilterProxyModel m_unpinnedTabs;
};

} // namespace omaweb
