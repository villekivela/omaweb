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

    class SpecNormalizer final {
    public:
        QString normalize(SessionSpec &spec)
        {
            if (spec.spaces.isEmpty()) {
                return QStringLiteral("spaces: at least one Space is required");
            }
            for (qsizetype index = 0; index < spec.spaces.size(); ++index) {
                if (const auto error = normalizeSpace(spec.spaces[index], index);
                    !error.isEmpty()) {
                    return error;
                }
            }
            return normalizeActiveSpace(spec);
        }

    private:
        QString normalizeSpace(SpaceSpec &space, qsizetype index)
        {
            const auto path = QStringLiteral("spaces[%1]").arg(index);
            if (space.id.trimmed().isEmpty()) {
                return path + QStringLiteral(".id: id is required");
            }
            if (m_spaceIds.contains(space.id)) {
                return path + QStringLiteral(".id: duplicate Space id '%1'").arg(space.id);
            }
            m_spaceIds.insert(space.id);

            space.name = space.name.trimmed();
            if (space.name.isEmpty()) {
                return path + QStringLiteral(".name: name is required");
            }
            if (space.color.isEmpty()) {
                return path + QStringLiteral(".color: color is required");
            }
            if (space.tabs.isEmpty()) {
                return path + QStringLiteral(".tabs: at least one open tab is required");
            }
            if (const auto error = normalizeOpenTabs(space.tabs, path); !error.isEmpty()) {
                return error;
            }
            if (const auto error = normalizeActiveTab(space, path); !error.isEmpty()) {
                return error;
            }
            if (const auto error = normalizeRecentCloses(space.recentCloses, path);
                !error.isEmpty()) {
                return error;
            }
            return {};
        }

        QString normalizeOpenTabs(QVector<TabSpec> &tabs, const QString &spacePath)
        {
            bool sawOrdinaryTab = false;
            qsizetype ordinaryTabs = 0;
            qsizetype blankTabs = 0;
            for (qsizetype index = 0; index < tabs.size(); ++index) {
                auto &tab = tabs[index];
                const auto path = spacePath + QStringLiteral(".tabs[%1]").arg(index);
                if (tab.id.trimmed().isEmpty()) {
                    return path + QStringLiteral(".id: id is required");
                }
                if (m_tabIds.contains(tab.id)) {
                    return path + QStringLiteral(".id: duplicate open-tab id '%1'").arg(tab.id);
                }
                m_tabIds.insert(tab.id);
                if (tab.pinned && sawOrdinaryTab) {
                    return path + QStringLiteral(".pinned: Pinned tabs must come first");
                }
                if (!tab.pinned) {
                    sawOrdinaryTab = true;
                    ++ordinaryTabs;
                }

                const auto blank = isBlank(tab.url);
                if (const auto error = normalizeTab(tab, path, true); !error.isEmpty()) {
                    return error;
                }
                blankTabs += blank ? 1 : 0;
            }
            if (blankTabs > 0 && ordinaryTabs != 1) {
                return spacePath
                    + QStringLiteral(".tabs: a blank tab must be the only ordinary tab");
            }
            return {};
        }

        static QString normalizeRecentCloses(
            QVector<TabSpec> &recentCloses, const QString &spacePath)
        {
            for (qsizetype index = 0; index < recentCloses.size(); ++index) {
                auto &close = recentCloses[index];
                const auto path = spacePath + QStringLiteral(".recentCloses[%1]").arg(index);
                if (!close.id.isEmpty()) {
                    return path + QStringLiteral(".id: recent closes do not declare an id");
                }
                if (const auto error = normalizeTab(close, path, false); !error.isEmpty()) {
                    return error;
                }
            }
            return {};
        }

        static QString normalizeTab(TabSpec &tab, const QString &path, bool blankAllowed)
        {
            if (tab.keepActive && !tab.pinned) {
                return path + QStringLiteral(".keepActive: requires a Pinned tab");
            }
            if (invalidZoom(tab.zoom)) {
                return path + QStringLiteral(".zoom: must be positive and finite");
            }
            if (!isBlank(tab.url)) {
                if (tab.title.isEmpty()) {
                    tab.title = tab.url.toString();
                }
                return {};
            }
            if (!blankAllowed) {
                return path + QStringLiteral(".url: address is required");
            }
            if (tab.pinned) {
                return path + QStringLiteral(".url: a Pinned tab cannot be blank");
            }
            tab.url = QUrl(QStringLiteral("about:blank"));
            tab.title = QStringLiteral("New tab");
            return {};
        }

        static QString normalizeActiveTab(SpaceSpec &space, const QString &path)
        {
            if (space.activeTabId.isEmpty()) {
                if (space.tabs.size() != 1) {
                    return path
                        + QStringLiteral(".activeTabId: required when several tabs are declared");
                }
                space.activeTabId = space.tabs.constFirst().id;
                return {};
            }
            for (const auto &tab : space.tabs) {
                if (tab.id == space.activeTabId) {
                    return {};
                }
            }
            return path
                + QStringLiteral(".activeTabId: unknown tab id '%1'").arg(space.activeTabId);
        }

        QString normalizeActiveSpace(SessionSpec &spec) const
        {
            if (spec.activeSpaceId.isEmpty()) {
                if (spec.spaces.size() != 1) {
                    return QStringLiteral(
                        "activeSpaceId: required when several Spaces are declared");
                }
                spec.activeSpaceId = spec.spaces.constFirst().id;
                return {};
            }
            if (!m_spaceIds.contains(spec.activeSpaceId)) {
                return QStringLiteral("activeSpaceId: unknown Space id '%1'")
                    .arg(spec.activeSpaceId);
            }
            return {};
        }

        QSet<QString> m_spaceIds;
        QSet<QString> m_tabIds;
    };

    enum class TabIdSource { Declared, Generated };

    QVector<TabState> tabStates(
        const QVector<TabSpec> &specs, const QString &spaceId, TabIdSource idSource)
    {
        QVector<TabState> tabs;
        tabs.reserve(specs.size());
        for (const auto &spec : specs) {
            tabs.append(TabState {
                .id = idSource == TabIdSource::Declared
                    ? spec.id
                    : QUuid::createUuid().toString(QUuid::WithoutBraces),
                .spaceId = spaceId,
                .url = spec.url,
                .title = spec.title,
                .pinned = spec.pinned,
                .muted = spec.muted,
                .zoom = spec.zoom,
                .keepActive = spec.keepActive,
            });
        }
        return tabs;
    }

    QString writeSpace(
        SqliteSessionStore &store, const SpaceSpec &spec, const QString &activeSpaceId)
    {
        if (!store.saveSpace({spec.id, spec.name, spec.color, spec.id == activeSpaceId})) {
            return QStringLiteral("spaces[%1]: could not save Space").arg(spec.id);
        }
        const auto tabs = tabStates(spec.tabs, spec.id, TabIdSource::Declared);
        if (!store.saveTabs(spec.id, tabs, spec.activeTabId)) {
            return QStringLiteral("spaces[%1].tabs: could not save tabs").arg(spec.id);
        }
        const auto recentCloses = tabStates(spec.recentCloses, spec.id, TabIdSource::Generated);
        if (!store.recordClosedTabs(spec.id, recentCloses)) {
            return QStringLiteral("spaces[%1].recentCloses: could not save recent closes")
                .arg(spec.id);
        }
        return {};
    }

    QString writeSession(const SessionSpec &spec, const QString &dataRoot)
    {
        SqliteSessionStore store(dataRoot);
        QString error;
        if (!store.open(&error)) {
            return error;
        }
        for (const auto &space : spec.spaces) {
            error = writeSpace(store, space, spec.activeSpaceId);
            if (!error.isEmpty()) {
                return error;
            }
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
    SpecNormalizer normalizer;
    m_errorMessage = normalizer.normalize(spec);
    if (m_errorMessage.isEmpty()) {
        m_errorMessage = writeSession(spec, m_dataRoot.path());
    }
    m_ready = m_errorMessage.isEmpty();
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
