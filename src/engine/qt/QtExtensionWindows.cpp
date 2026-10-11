#include "QtExtensionWindows.h"

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickWebEngineProfile>
#include <QWebEngineExtensionManager>

namespace omaweb {

// Wayland gives a client no say over where its window is and does not tell it
// either, so a position Qt reports there is only what was last asked for.
QtExtensionWindows::Positions QtExtensionWindows::positionsOn(const QString &platformName)
{
    return platformName.startsWith(QLatin1String("wayland")) ? Positions::Unknown
                                                             : Positions::FromPlatform;
}

QtExtensionWindows::QtExtensionWindows(QObject *parent)
    : QtExtensionWindows(positionsOn(QGuiApplication::platformName()), parent)
{
}

QtExtensionWindows::QtExtensionWindows(Positions positions, QObject *parent)
    : QObject(parent)
    , m_positions(positions)
{
}

void QtExtensionWindows::setMainWindow(QWindow *window) { m_mainWindow = window; }

QRect QtExtensionWindows::geometryOf(const QObject *page) const
{
    // The page is the tab's QtWebEngine view, which a Tab window borrows by
    // drawing it in its own content, so the window it is drawn in is the one
    // the reader sees it in.
    const auto *view = qobject_cast<const QQuickItem *>(page);
    const QWindow *window = view && view->window() ? view->window() : m_mainWindow.data();
    if (!window) {
        return {};
    }
    QRect geometry = window->frameGeometry();
    if (m_positions == Positions::Unknown) {
        geometry.moveTopLeft(QPoint(0, 0));
    }
    return geometry;
}

void QtExtensionWindows::attachToProfile(QObject *profile)
{
#if OMAWEB_EXTENSION_WINDOW_GEOMETRY
    auto *quickProfile = qobject_cast<QQuickWebEngineProfile *>(profile);
    auto *extensions = quickProfile ? quickProfile->extensionManager() : nullptr;
    if (!extensions) {
        return;
    }
    // An invalid answer is the engine's cue to report the view's own size.
    extensions->setWindowGeometryProvider(
        [self = QPointer<const QtExtensionWindows>(this)](
            const QObject *page) { return self ? self->geometryOf(page) : QRect(); });
#else
    // An engine without the patch reports no position or size, which an
    // extension that places a window of its own cannot use.
    Q_UNUSED(profile);
#endif
}

} // namespace omaweb
