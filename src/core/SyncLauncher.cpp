#include "SyncLauncher.h"

#include "BrowserStateExchange.h"
#include "SyncFeature.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>

#include <utility>

namespace omaweb {

SyncLauncher::SyncLauncher(BrowserController *browser, ContentBlocker *blocker,
    KeyboardNavigation *keyboardNavigation, QString dataRoot, QString configRoot,
    QString modulePath, QObject *parent)
    : QObject(parent)
    , m_browser(browser)
    , m_blocker(blocker)
    , m_keyboardNavigation(keyboardNavigation)
    , m_dataRoot(std::move(dataRoot))
    , m_configRoot(std::move(configRoot))
    , m_modulePath(std::move(modulePath))
{
    QFile marker(QDir(m_configRoot).filePath(QStringLiteral("sync.json")));
    const auto markerObject = marker.open(QIODevice::ReadOnly)
        ? QJsonDocument::fromJson(marker.readAll()).object()
        : QJsonObject {};
    m_configured = markerObject.value(QStringLiteral("provider")).isString();
    if (markerObject.value(QStringLiteral("enabled")).toBool()) {
        load();
    }
}

SyncLauncher::~SyncLauncher() = default;

QObject *SyncLauncher::controller() const { return m_controller; }

QString SyncLauncher::errorMessage() const { return m_errorMessage; }

bool SyncLauncher::configured() const { return m_configured; }

bool SyncLauncher::load()
{
    if (m_controller) {
        return true;
    }
    if (m_modulePath.isEmpty() || !QFileInfo::exists(m_modulePath)) {
        m_errorMessage = QStringLiteral("The Sync Feature module is not installed");
        emit errorMessageChanged();
        return false;
    }
    m_loader = std::make_unique<QPluginLoader>(m_modulePath);
    auto *plugin = qobject_cast<SyncFeature *>(m_loader->instance());
    if (!plugin || plugin->contractVersion() != 2) {
        m_errorMessage = m_loader->errorString().isEmpty()
            ? QStringLiteral("The installed Sync module is not compatible with this browser")
            : m_loader->errorString();
        m_loader.reset();
        emit errorMessageChanged();
        return false;
    }
    m_stateExchange = std::make_unique<BrowserStateExchangeAdapter>(
        m_browser, m_blocker, m_keyboardNavigation, m_dataRoot, m_configRoot);
    m_controller = plugin->createController(m_stateExchange.get(), m_dataRoot, m_configRoot, this);
    if (!m_controller) {
        m_errorMessage = QStringLiteral("The Sync module could not start");
        m_stateExchange.reset();
        m_loader.reset();
        emit errorMessageChanged();
        return false;
    }
    m_errorMessage.clear();
    emit errorMessageChanged();
    emit controllerChanged();
    return true;
}

} // namespace omaweb
