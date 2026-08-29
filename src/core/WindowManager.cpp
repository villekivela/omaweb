#include "WindowManager.h"

#include "BrowserController.h"

#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

namespace tanto {

WindowManager::WindowManager(QString engineName, QObject *parent)
    : QObject(parent)
    , m_engineName(std::move(engineName))
{
}

WindowManager::~WindowManager() = default;

BrowserController *WindowManager::createPrivateWindow()
{
    if (!ensurePrivateSession()) {
        return nullptr;
    }

    auto *controller = new BrowserController({}, m_engineName, true, this);
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

QString WindowManager::privateDownloadDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

bool WindowManager::acceptPrivateDownloads() const
{
    return true;
}

bool WindowManager::recordPrivateDownloads() const
{
    return false;
}

bool WindowManager::ensurePrivateSession()
{
    if (m_privateRoot) {
        return true;
    }

    auto root = std::make_unique<QTemporaryDir>(
        QDir::tempPath() + QStringLiteral("/tanto-private-XXXXXX"));
    if (!root->isValid()) {
        return false;
    }
    if (!QDir().mkpath(QDir(root->path()).filePath(QStringLiteral("engine-profile")))) {
        return false;
    }

    m_privateRoot = std::move(root);
    emit privateSessionChanged();
    return true;
}

} // namespace tanto
