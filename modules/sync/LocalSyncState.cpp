#include "LocalSyncState.h"

#include "SqliteSessionStore.h"
#include "SyncModule.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <sodium.h>

#include <algorithm>
#include <utility>

namespace omaweb {

namespace {

    QByteArray digest(const QJsonValue &value)
    {
        const auto contents = value.isArray()
            ? QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact)
            : QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
        return QCryptographicHash::hash(contents, QCryptographicHash::Sha256);
    }

    QJsonObject projectedTab(const TabState &tab, qsizetype position)
    {
        return {{QStringLiteral("id"), tab.id}, {QStringLiteral("spaceId"), tab.spaceId},
            {QStringLiteral("url"), tab.url.toString(QUrl::FullyEncoded)},
            {QStringLiteral("title"), tab.title}, {QStringLiteral("pinned"), tab.pinned},
            {QStringLiteral("position"), position}, {QStringLiteral("muted"), tab.muted},
            {QStringLiteral("zoom"), tab.zoom}, {QStringLiteral("keepActive"), tab.keepActive}};
    }

    bool isRestingPlaceholder(const TabState &tab, const QVector<TabState> &tabs)
    {
        if (tab.pinned || (!tab.url.isEmpty() && tab.url != QUrl(QStringLiteral("about:blank")))) {
            return false;
        }
        return std::ranges::count_if(tabs, [](const TabState &candidate) {
            return !candidate.pinned;
        }) == 1;
    }

} // namespace

LocalSyncState::LocalSyncState(
    BrowserStateExchange &exchange, QString dataRoot, QString configRoot, QObject *parent)
    : QObject(parent)
    , m_exchange(exchange)
    , m_dataRoot(std::move(dataRoot))
    , m_configRoot(std::move(configRoot))
{
    m_projectionCheck.setSingleShot(true);
    connect(&m_projectionCheck, &QTimer::timeout, this, &LocalSyncState::checkProjection);
    connect(&m_exchange, &BrowserStateExchange::possiblyChanged, this,
        &LocalSyncState::scheduleProjectionCheck);
    if (m_configWatcher.addPath(m_configRoot)) {
        connect(&m_configWatcher, &QFileSystemWatcher::directoryChanged, this,
            &LocalSyncState::scheduleProjectionCheck);
    }
    if (eligible()) {
        m_fingerprints = fingerprints(m_exchange.capture(selection()));
    }
}

bool LocalSyncState::eligible() const { return m_exchange.eligible(); }

LocalSyncRevision LocalSyncState::checkpoint() const
{
    if (!eligible()) {
        return {};
    }
    const auto image = m_exchange.capture(selection());
    return {.generation = m_generation,
        .protectedTabId = image.activeTabId,
        .pristine = image.pristine};
}

LocalSyncApplyResult LocalSyncState::applyRemoteState(LocalSyncApplyRequest request)
{
    const auto discardRecoveryKey = [&request] {
        sodium_memzero(request.recoveryKey.data(), static_cast<size_t>(request.recoveryKey.size()));
    };
    if (!eligible()) {
        discardRecoveryKey();
        return {.status = LocalSyncApplyStatus::Refused,
            .failure = LocalSyncFailureCode::PrivateStateRefused,
            .errorMessage = QStringLiteral("Sync is unavailable in a Private window")};
    }
    if (request.expectedGeneration != m_generation) {
        discardRecoveryKey();
        return {.status = LocalSyncApplyStatus::Stale,
            .failure = LocalSyncFailureCode::LocalStateChanged,
            .errorMessage = QStringLiteral("Local browser state changed during Sync")};
    }

    const auto before = fingerprints(m_exchange.capture(selection()));
    QString errorMessage;
    SqliteSessionStore store(m_dataRoot);
    SyncModule sync(store,
        {.dataRoot = m_dataRoot,
            .configRoot = m_configRoot,
            .remoteUrl = std::move(request.remoteUrl),
            .machineId = std::move(request.machineId),
            .recoveryKey = std::move(request.recoveryKey),
            .protectedTabId = std::move(request.protectedTabId),
            .initialRemoteRestore = request.initialRemoteRestore,
            .discardPristineLocalState = request.discardPristineLocalState,
            .replaceLocalState = request.replaceLocalState});
    if (!store.open(&errorMessage) || !sync.open(&errorMessage)
        || !sync.applyRemoteState(&errorMessage)) {
        return {.status = LocalSyncApplyStatus::Failed,
            .failure = LocalSyncFailureCode::StorageFailed,
            .errorMessage = errorMessage};
    }

    const auto after = fingerprints(m_exchange.capture(selection()));
    BrowserStateSections changed;
    if (before.browser != after.browser) {
        changed |= BrowserStateSection::Browser;
    }
    if (before.keybindings != after.keybindings) {
        changed |= BrowserStateSection::Keybindings;
    }
    if (before.filterSubscriptions != after.filterSubscriptions) {
        changed |= BrowserStateSection::FilterSubscriptions;
    }
    m_applying = true;
    m_exchange.refresh(changed);
    m_applying = false;
    m_fingerprints = after;
    return {.status = changed == BrowserStateSections {} ? LocalSyncApplyStatus::NoChange
                                                         : LocalSyncApplyStatus::Applied,
        .failure = LocalSyncFailureCode::None,
        .errorMessage = {}};
}

BrowserStateSelection LocalSyncState::selection()
{
    return {.preferenceNames = {QStringLiteral("floating-controls"), QStringLiteral("ease-sidebar"),
                QStringLiteral("use-favicons"), QStringLiteral("tint-favicons")},
        .keybindings = true,
        .filterSubscriptions = true};
}

LocalSyncState::Fingerprints LocalSyncState::fingerprints(const BrowserStateImage &image)
{
    QJsonArray spaces;
    QJsonArray tabs;
    for (qsizetype spacePosition = 0; spacePosition < image.spaces.size(); ++spacePosition) {
        const auto &space = image.spaces.at(spacePosition);
        spaces.append(QJsonObject {{QStringLiteral("id"), space.id},
            {QStringLiteral("name"), space.name}, {QStringLiteral("color"), space.color},
            {QStringLiteral("position"), spacePosition}});
        const auto localTabs = image.tabsBySpace.value(space.id);
        for (qsizetype tabPosition = 0; tabPosition < localTabs.size(); ++tabPosition) {
            if (isRestingPlaceholder(localTabs.at(tabPosition), localTabs)) {
                continue;
            }
            tabs.append(projectedTab(localTabs.at(tabPosition), tabPosition));
        }
    }
    QStringList preferenceNames = image.preferences.keys();
    preferenceNames.sort();
    QJsonObject preferences;
    for (const auto &name : preferenceNames) {
        preferences.insert(name, image.preferences.value(name));
    }
    return {.browser = digest(QJsonObject {{QStringLiteral("spaces"), spaces},
                {QStringLiteral("tabs"), tabs}, {QStringLiteral("preferences"), preferences}}),
        .keybindings = QCryptographicHash::hash(image.keybindings, QCryptographicHash::Sha256),
        .filterSubscriptions = digest(image.filterSubscriptions)};
}

void LocalSyncState::scheduleProjectionCheck()
{
    if (!m_applying) {
        m_projectionCheck.start(0);
    }
}

void LocalSyncState::checkProjection()
{
    if (m_applying || !eligible()) {
        return;
    }
    const auto next = fingerprints(m_exchange.capture(selection()));
    if (next == m_fingerprints) {
        return;
    }
    m_fingerprints = next;
    ++m_generation;
    emit meaningfulChange(m_generation);
}

} // namespace omaweb
