#include "WindowManager.h"

#include "PrivateSessionStore.h"

#include "BrowserController.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTimer>

namespace omaweb {

WindowManager::WindowManager(QObject *parent)
    : WindowManager(true, parent)
{
}

WindowManager::WindowManager(bool privateWindowsAvailable, QObject *parent)
    : WindowManager({}, privateWindowsAvailable, parent)
{
}

WindowManager::WindowManager(QString configRoot, bool privateWindowsAvailable, QObject *parent)
    : QObject(parent)
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

    // A store per window, over the session's decisions: what a window keeps
    // of its own, the favicons its pages showed, goes when the window does.
    auto *controller
        = new BrowserController(std::make_shared<PrivateSessionStore>(m_privatePermissionDecisions),
            true, m_privatePermissionDecisions, m_privateSiteState, m_configRoot, this);
    if (!controller->ready()) {
        controller->deleteLater();
        return nullptr;
    }

    // A new window is the one on show.
    m_privateWindows.prepend(controller);
    emit privateWindowCountChanged();
    return controller;
}

void WindowManager::openPrivateWindow() { openNewPrivateWindow({}); }

BrowserController *WindowManager::showPrivateWindow(const QUrl &url)
{
    if (m_privateWindows.isEmpty()) {
        return openNewPrivateWindow(url);
    }
    auto *shown = m_privateWindows.constFirst();
    if (url.isValid()) {
        shown->openLink(url, true);
    }
    return shown;
}

BrowserController *WindowManager::openNewPrivateWindow(const QUrl &url)
{
    auto *controller = createPrivateWindow();
    if (!controller) {
        return nullptr;
    }
    // A new window opens on a blank tab, which the address takes rather than
    // leaving it beside the page. Before the window is built, so it comes up
    // on the address.
    if (url.isValid()) {
        controller->openLink(url, false);
    }
    emit privateWindowRequested(controller, privateProfilePath());
    return controller;
}

void WindowManager::notePrivateWindowShown(QObject *controller)
{
    auto *browser = qobject_cast<BrowserController *>(controller);
    const auto index = m_privateWindows.indexOf(browser);
    if (index > 0) {
        m_privateWindows.move(index, 0);
    }
}

void WindowManager::releasePrivateWindow(QObject *controller)
{
    auto *browser = qobject_cast<BrowserController *>(controller);
    if (!browser || !m_privateWindows.removeOne(browser)) {
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

int WindowManager::privateWindowCount() const { return m_privateWindows.size(); }

QString WindowManager::privateProfilePath() const
{
    if (!m_privateRoot) {
        return {};
    }
    return QDir(m_privateRoot->path()).filePath(QStringLiteral("engine-profile"));
}

bool WindowManager::privateWindowsAvailable() const { return m_privateWindowsAvailable; }

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
