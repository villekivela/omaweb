#include "BrowserStateExchange.h"

#include "BrowserController.h"
#include "ContentBlocker.h"
#include "KeyboardNavigation.h"
#include "SessionStore.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace omaweb {

BrowserStateExchangeAdapter::BrowserStateExchangeAdapter(BrowserController *browser,
    ContentBlocker *blocker, KeyboardNavigation *keyboardNavigation, QString dataRoot,
    QString configRoot, QObject *parent)
    : BrowserStateExchange(parent)
    , m_browser(browser)
    , m_blocker(blocker)
    , m_keyboardNavigation(keyboardNavigation)
    , m_dataRoot(std::move(dataRoot))
    , m_configRoot(std::move(configRoot))
{
    const auto reportPossibleChange = [this] { emit possiblyChanged(); };
    connect(browser->spaces(), &QAbstractItemModel::dataChanged, this, reportPossibleChange);
    connect(browser->spaces(), &QAbstractItemModel::rowsInserted, this, reportPossibleChange);
    connect(browser->spaces(), &QAbstractItemModel::rowsRemoved, this, reportPossibleChange);
    connect(browser->spaces(), &QAbstractItemModel::rowsMoved, this, reportPossibleChange);
    connect(browser->spaces(), &QAbstractItemModel::modelReset, this, reportPossibleChange);
    connect(browser->tabs(), &QAbstractItemModel::dataChanged, this, reportPossibleChange);
    connect(browser->tabs(), &QAbstractItemModel::rowsInserted, this, reportPossibleChange);
    connect(browser->tabs(), &QAbstractItemModel::rowsRemoved, this, reportPossibleChange);
    connect(browser->tabs(), &QAbstractItemModel::rowsMoved, this, reportPossibleChange);
    connect(browser->tabs(), &QAbstractItemModel::modelReset, this, reportPossibleChange);
    connect(browser, &BrowserController::preferenceChanged, this, reportPossibleChange);
    if (blocker) {
        connect(blocker, &ContentBlocker::subscriptionsChanged, this, reportPossibleChange);
    }
}

bool BrowserStateExchangeAdapter::eligible() const
{
    return m_browser && !m_browser->privateBrowsing() && m_browser->sessionStore()
        && m_browser->sessionStore()->recordsState();
}

BrowserStateImage BrowserStateExchangeAdapter::capture(const BrowserStateSelection &selection) const
{
    if (!eligible()) {
        return {};
    }
    BrowserStateImage image;
    auto *store = m_browser->sessionStore();
    image.spaces = store->loadSpaces();
    for (auto &space : image.spaces) {
        space.active = false;
        auto tabs = store->loadTabs(space.id);
        for (auto &tab : tabs) {
            if (tab.active) {
                image.activeTabIds.insert(space.id, tab.id);
            }
            tab.active = false;
            tab.loading = false;
            tab.iconUrl.clear();
            tab.audible = false;
            tab.soundSuppressed = false;
            tab.rendererFailureReason.clear();
        }
        image.tabsBySpace.insert(space.id, std::move(tabs));
    }
    image.activeSpaceId = m_browser->activeSpaceId();
    image.activeTabId = m_browser->activeTabId();
    image.pristine = m_browser->startedWithEmptyState();
    for (const auto &name : selection.preferenceNames) {
        const auto missing = QString(QChar(0));
        const auto value = store->preference(name, missing);
        if (value != missing) {
            image.preferences.insert(name, value);
        }
    }
    if (selection.keybindings) {
        QFile keybindings(QDir(m_configRoot).filePath(QStringLiteral("keybindings.json")));
        if (keybindings.open(QIODevice::ReadOnly)) {
            image.keybindings = keybindings.readAll();
        }
    }
    if (selection.filterSubscriptions) {
        QFile settings(QDir(m_dataRoot).filePath(QStringLiteral("content-blocking/settings.json")));
        const auto subscriptions = settings.open(QIODevice::ReadOnly)
            ? QJsonDocument::fromJson(settings.readAll())
                  .object()
                  .value(QStringLiteral("subscriptions"))
                  .toArray()
            : QJsonArray {};
        for (const auto &value : subscriptions) {
            const auto local = value.toObject();
            QJsonObject subscription;
            for (const auto &name : {QStringLiteral("id"), QStringLiteral("title"),
                     QStringLiteral("source"), QStringLiteral("license"),
                     QStringLiteral("updateAddress"), QStringLiteral("enabled")}) {
                if (local.contains(name)) {
                    subscription.insert(name, local.value(name));
                }
            }
            image.filterSubscriptions.append(subscription);
        }
    }
    return image;
}

void BrowserStateExchangeAdapter::refresh(BrowserStateSections sections)
{
    if (sections.testFlag(BrowserStateSection::Browser)) {
        m_browser->reloadSyncedState();
    }
    if (sections.testFlag(BrowserStateSection::Keybindings) && m_keyboardNavigation) {
        m_keyboardNavigation->reload();
    }
    if (sections.testFlag(BrowserStateSection::FilterSubscriptions) && m_blocker) {
        m_blocker->reloadSyncedConfiguration();
    }
}

} // namespace omaweb
