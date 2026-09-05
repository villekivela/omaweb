#include "WindowManager.h"

#include "BrowserController.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTimer>

namespace omaweb {

WindowManager::WindowManager(QString engineName, QObject *parent)
    : WindowManager(std::move(engineName), true, parent)
{
}

WindowManager::WindowManager(QString engineName, bool privateWindowsAvailable, QObject *parent)
    : WindowManager(std::move(engineName), {}, privateWindowsAvailable, parent)
{
}

WindowManager::WindowManager(QString engineName, QString configRoot,
    bool privateWindowsAvailable, QObject *parent)
    : QObject(parent)
    , m_engineName(std::move(engineName))
    , m_configRoot(std::move(configRoot))
    , m_privateWindowsAvailable(privateWindowsAvailable)
{
}

WindowManager::~WindowManager() = default;

BrowserController *WindowManager::createPrivateWindow()
{
    if (!m_privateWindowsAvailable || !ensurePrivateSession()) {
        return nullptr;
    }

    auto *controller = new BrowserController({}, m_engineName, true,
        m_privatePermissionDecisions, m_privateSiteState, m_configRoot, this);
    if (!controller->ready()) {
        controller->deleteLater();
        return nullptr;
    }

    m_privateWindows.insert(controller);
    emit privateWindowCountChanged();
    return controller;
}

void WindowManager::openPrivateWindow()
{
    if (auto *controller = createPrivateWindow()) {
        emit privateWindowRequested(controller, privateProfilePath());
    }
}

void WindowManager::releasePrivateWindow(QObject *controller)
{
    auto *browser = qobject_cast<BrowserController *>(controller);
    if (!browser || !m_privateWindows.remove(browser)) {
        return;
    }

    delete browser;
    emit privateWindowCountChanged();
    if (m_privateWindows.isEmpty()) {
        emit privateSessionEnding();
        QTimer::singleShot(0, this, [this] {
            if (!m_privateWindows.isEmpty()) {
                return;
            }
            m_privateRoot.reset();
            m_privatePermissionDecisions.reset();
            m_privateSiteState.reset();
            emit privateSessionChanged();
        });
    }
}

int WindowManager::privateWindowCount() const
{
    return m_privateWindows.size();
}

QString WindowManager::privateProfilePath() const
{
    if (!m_privateRoot) {
        return {};
    }
    return QDir(m_privateRoot->path()).filePath(QStringLiteral("engine-profile"));
}

bool WindowManager::privateWindowsAvailable() const
{
    return m_privateWindowsAvailable;
}

bool WindowManager::ensurePrivateSession()
{
    if (m_privateRoot) {
        return true;
    }

    auto root = std::make_unique<QTemporaryDir>(
        QDir::tempPath() + QStringLiteral("/omaweb-private-XXXXXX"));
    if (!root->isValid()) {
        return false;
    }
    if (!QDir().mkpath(QDir(root->path()).filePath(QStringLiteral("engine-profile")))) {
        return false;
    }

    m_privateRoot = std::move(root);
    m_privatePermissionDecisions = QSharedPointer<QHash<QString, int>>::create();
    m_privateSiteState = QSharedPointer<SessionSiteState>::create();
    emit privateSessionChanged();
    return true;
}

} // namespace omaweb
