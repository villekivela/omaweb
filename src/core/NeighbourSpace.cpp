#include "NeighbourSpace.h"

#include <QRegularExpression>

#include <algorithm>

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
    if (!m_tabs.items().isEmpty()) {
        emit rowsAboutToChange();
        m_tabs.reset({});
        emit rowsChanged();
    }
    emit changed();
}

namespace {

    // Whether two tabs draw as the same row.
    bool drawnAlike(const TabState &left, const TabState &right)
    {
        return left.id == right.id && left.url == right.url && left.title == right.title
            && left.iconUrl == right.iconUrl && left.pinned == right.pinned
            && left.active == right.active && left.audible == right.audible
            && left.muted == right.muted && left.soundSuppressed == right.soundSuppressed
            && left.keepActive == right.keepActive && left.splitPartnerId == right.splitPartnerId
            && left.poppedOut == right.poppedOut;
    }

} // namespace

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
    const bool unchanged = std::ranges::equal(tabs, m_tabs.items(), drawnAlike);
    if (!unchanged) {
        emit rowsAboutToChange();
        m_tabs.reset(std::move(tabs));
        emit rowsChanged();
    }
    if (!m_ready) {
        m_ready = true;
        emit changed();
    }
}

} // namespace omaweb
