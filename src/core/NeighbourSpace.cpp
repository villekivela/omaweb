#include "NeighbourSpace.h"

#include <QRegularExpression>

namespace omaweb {

NeighbourSpace::NeighbourSpace(QObject *parent)
    : QObject(parent)
{
    m_pinnedTabs.setSourceModel(&m_tabs);
    m_pinnedTabs.setFilterRole(TabListModel::PinnedRole);
    m_pinnedTabs.setFilterRegularExpression(QRegularExpression(QStringLiteral("^true$")));
    m_unpinnedTabs.setSourceModel(&m_tabs);
    m_unpinnedTabs.setFilterRole(TabListModel::PinnedRole);
    m_unpinnedTabs.setFilterRegularExpression(QRegularExpression(QStringLiteral("^false$")));
}

QString NeighbourSpace::spaceId() const { return m_spaceId; }

bool NeighbourSpace::ready() const { return m_ready; }

QAbstractItemModel *NeighbourSpace::pinnedTabs() { return &m_pinnedTabs; }

QAbstractItemModel *NeighbourSpace::unpinnedTabs() { return &m_unpinnedTabs; }

void NeighbourSpace::name(const QString &spaceId)
{
    if (spaceId == m_spaceId) {
        return;
    }
    m_spaceId = spaceId;
    m_ready = false;
    m_tabs.reset({});
    emit changed();
}

void NeighbourSpace::show(QVector<TabState> tabs)
{
    qsizetype ordinary = 0;
    const TabState *onlyOrdinary = nullptr;
    for (const auto &tab : tabs) {
        if (!tab.pinned && !tab.poppedOut) {
            ++ordinary;
            onlyOrdinary = &tab;
        }
    }
    if (ordinary == 1
        && (onlyOrdinary->url.isEmpty()
            || onlyOrdinary->url == QUrl(QStringLiteral("about:blank")))) {
        const auto blankId = onlyOrdinary->id;
        tabs.removeIf([&blankId](const TabState &tab) { return tab.id == blankId; });
    }
    m_tabs.reset(std::move(tabs));
    if (!m_ready) {
        m_ready = true;
        emit changed();
    }
}

} // namespace omaweb
