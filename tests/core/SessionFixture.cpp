#include "SessionFixture.h"

#include "SpaceStorage.h"
#include "SqliteSessionStore.h"

#include <QSet>
#include <QUuid>

#include <cmath>
#include <utility>

namespace omaweb::test {

namespace {

    bool isBlank(const QUrl &url)
    {
        return url.isEmpty() || url == QUrl(QStringLiteral("about:blank"));
    }

    bool invalidZoom(double zoom) { return !std::isfinite(zoom) || zoom <= 0.0; }

    QString validateAndNormalizeTab(TabSpec &tab, const QString &path, bool blankAllowed)
    {
        if (tab.keepActive && !tab.pinned) {
            return path + QStringLiteral(".keepActive: requires a Pinned tab");
        }
        if (invalidZoom(tab.zoom)) {
            return path + QStringLiteral(".zoom: must be positive and finite");
        }
        if (isBlank(tab.url)) {
            if (!blankAllowed) {
                return path + QStringLiteral(".url: address is required");
            }
            if (tab.pinned) {
                return path + QStringLiteral(".url: a Pinned tab cannot be blank");
            }
            tab.url = QUrl(QStringLiteral("about:blank"));
            tab.title = QStringLiteral("New tab");
        } else if (tab.title.isEmpty()) {
            tab.title = tab.url.toString();
        }
        return {};
    }

    TabState tabState(const TabSpec &spec, const QString &spaceId, QString id)
    {
        return TabState {
            .id = std::move(id),
            .spaceId = spaceId,
            .url = spec.url,
            .title = spec.title,
            .pinned = spec.pinned,
            .muted = spec.muted,
            .zoom = spec.zoom,
            .keepActive = spec.keepActive,
        };
    }

    QString validateAndNormalize(SessionSpec &spec)
    {
        if (spec.spaces.isEmpty()) {
            return QStringLiteral("spaces: at least one Space is required");
        }

        QSet<QString> spaceIds;
        QSet<QString> tabIds;
        for (qsizetype spaceIndex = 0; spaceIndex < spec.spaces.size(); ++spaceIndex) {
            auto &space = spec.spaces[spaceIndex];
            const auto spacePath = QStringLiteral("spaces[%1]").arg(spaceIndex);
            if (space.id.trimmed().isEmpty()) {
                return spacePath + QStringLiteral(".id: id is required");
            }
            if (spaceIds.contains(space.id)) {
                return spacePath + QStringLiteral(".id: duplicate Space id '%1'").arg(space.id);
            }
            spaceIds.insert(space.id);
            space.name = space.name.trimmed();
            if (space.name.isEmpty()) {
                return spacePath + QStringLiteral(".name: name is required");
            }
            if (space.color.isEmpty()) {
                return spacePath + QStringLiteral(".color: color is required");
            }
            if (space.tabs.isEmpty()) {
                return spacePath + QStringLiteral(".tabs: at least one open tab is required");
            }

            bool sawOrdinaryTab = false;
            qsizetype ordinaryTabs = 0;
            qsizetype blankOrdinaryTabs = 0;
            for (qsizetype tabIndex = 0; tabIndex < space.tabs.size(); ++tabIndex) {
                auto &tab = space.tabs[tabIndex];
                const auto tabPath = spacePath + QStringLiteral(".tabs[%1]").arg(tabIndex);
                if (tab.id.trimmed().isEmpty()) {
                    return tabPath + QStringLiteral(".id: id is required");
                }
                if (tabIds.contains(tab.id)) {
                    return tabPath + QStringLiteral(".id: duplicate open-tab id '%1'").arg(tab.id);
                }
                tabIds.insert(tab.id);
                if (tab.pinned && sawOrdinaryTab) {
                    return tabPath + QStringLiteral(".pinned: Pinned tabs must come first");
                }
                if (!tab.pinned) {
                    sawOrdinaryTab = true;
                    ++ordinaryTabs;
                }
                const auto blank = isBlank(tab.url);
                if (const auto error = validateAndNormalizeTab(tab, tabPath, true);
                    !error.isEmpty()) {
                    return error;
                }
                if (blank) {
                    ++blankOrdinaryTabs;
                }
            }
            if (blankOrdinaryTabs > 0 && ordinaryTabs != 1) {
                return spacePath
                    + QStringLiteral(".tabs: a blank tab must be the only ordinary tab");
            }
            if (space.activeTabId.isEmpty()) {
                if (space.tabs.size() != 1) {
                    return spacePath
                        + QStringLiteral(".activeTabId: required when several tabs are declared");
                }
                space.activeTabId = space.tabs.constFirst().id;
            } else {
                bool foundActiveTab = false;
                for (const auto &tab : space.tabs) {
                    foundActiveTab = foundActiveTab || tab.id == space.activeTabId;
                }
                if (!foundActiveTab) {
                    return spacePath
                        + QStringLiteral(".activeTabId: unknown tab id '%1'")
                              .arg(space.activeTabId);
                }
            }

            for (qsizetype closeIndex = 0; closeIndex < space.recentCloses.size(); ++closeIndex) {
                auto &close = space.recentCloses[closeIndex];
                const auto closePath
                    = spacePath + QStringLiteral(".recentCloses[%1]").arg(closeIndex);
                if (!close.id.isEmpty()) {
                    return closePath + QStringLiteral(".id: recent closes do not declare an id");
                }
                if (const auto error = validateAndNormalizeTab(close, closePath, false);
                    !error.isEmpty()) {
                    return error;
                }
            }
        }

        if (spec.activeSpaceId.isEmpty()) {
            if (spec.spaces.size() != 1) {
                return QStringLiteral("activeSpaceId: required when several Spaces are declared");
            }
            spec.activeSpaceId = spec.spaces.constFirst().id;
        } else if (!spaceIds.contains(spec.activeSpaceId)) {
            return QStringLiteral("activeSpaceId: unknown Space id '%1'").arg(spec.activeSpaceId);
        }
        return {};
    }

} // namespace

SessionFixture::SessionFixture(SessionSpec spec, QString configRoot)
    : m_configRoot(std::move(configRoot))
{
    if (!m_dataRoot.isValid()) {
        m_errorMessage = QStringLiteral("dataRoot: could not create a temporary directory");
        return;
    }
    m_errorMessage = validateAndNormalize(spec);
    if (!m_errorMessage.isEmpty()) {
        return;
    }

    SqliteSessionStore store(m_dataRoot.path());
    if (!store.open(&m_errorMessage)) {
        return;
    }
    for (const auto &spaceSpec : spec.spaces) {
        SpaceState space {
            .id = spaceSpec.id,
            .name = spaceSpec.name,
            .color = spaceSpec.color,
            .active = spaceSpec.id == spec.activeSpaceId,
        };
        if (!store.saveSpace(space)) {
            m_errorMessage = QStringLiteral("spaces[%1]: could not save Space").arg(spaceSpec.id);
            return;
        }

        QVector<TabState> tabs;
        tabs.reserve(spaceSpec.tabs.size());
        for (const auto &tabSpec : spaceSpec.tabs) {
            tabs.append(tabState(tabSpec, spaceSpec.id, tabSpec.id));
        }
        if (!store.saveTabs(spaceSpec.id, tabs, spaceSpec.activeTabId)) {
            m_errorMessage
                = QStringLiteral("spaces[%1].tabs: could not save tabs").arg(spaceSpec.id);
            return;
        }

        QVector<TabState> recentCloses;
        recentCloses.reserve(spaceSpec.recentCloses.size());
        for (const auto &closeSpec : spaceSpec.recentCloses) {
            recentCloses.append(tabState(
                closeSpec, spaceSpec.id, QUuid::createUuid().toString(QUuid::WithoutBraces)));
        }
        if (!store.saveClosedTabs(spaceSpec.id, recentCloses)) {
            m_errorMessage = QStringLiteral("spaces[%1].recentCloses: could not save recent closes")
                                 .arg(spaceSpec.id);
            return;
        }
    }
    m_ready = true;
}

bool SessionFixture::ready() const { return m_ready; }

QString SessionFixture::errorMessage() const { return m_errorMessage; }

QString SessionFixture::dataRoot() const { return m_dataRoot.path(); }

std::unique_ptr<BrowserController> SessionFixture::createController() const
{
    if (!m_ready) {
        return {};
    }
    return std::make_unique<BrowserController>(
        SpaceStorage(m_dataRoot.path(), QStringLiteral("test")), m_configRoot);
}

} // namespace omaweb::test
