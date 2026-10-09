#include "WindowAppId.h"

#include <QPlatformSurfaceEvent>
#include <QQmlEngine>

namespace omaweb {

WindowAppId::WindowAppId(QObject *parent)
    : QObject(parent)
{
}

QWindow *WindowAppId::window() const { return m_window; }

void WindowAppId::setWindow(QWindow *window)
{
    if (m_window == window) {
        return;
    }
    if (m_window) {
        m_window->removeEventFilter(this);
    }
    m_window = window;
    if (m_window) {
        m_window->installEventFilter(this);
    }
    emit windowChanged();
    watchSurface();
}

QString WindowAppId::appId() const { return m_appId; }

void WindowAppId::setAppId(const QString &appId)
{
    if (m_appId == appId) {
        return;
    }
    m_appId = appId;
    emit appIdChanged();
    watchSurface();
}

bool WindowAppId::applied() const { return m_applied; }

// A window's platform surface is made when it is first shown, and the name
// has to reach it before the compositor maps it, so a window not yet shown is
// watched until it has one.
bool WindowAppId::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::PlatformSurface
        && static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType()
            == QPlatformSurfaceEvent::SurfaceCreated) {
        watchSurface();
    }
    return false;
}

void WindowAppId::watchSurface()
{
    if (!m_window || !m_window->handle() || m_appId.isEmpty() || m_watched == m_window->handle()) {
        return;
    }
    m_watched = m_window->handle();
    QPointer<WindowAppId> self(this);
    watchWindowRole(m_window, m_appId, this, [self] {
        if (self) {
            self->setApplied(true);
        }
    });
}

void WindowAppId::setApplied(bool applied)
{
    if (m_applied == applied) {
        return;
    }
    m_applied = applied;
    emit appliedChanged();
}

void registerWindowAppId() { qmlRegisterType<WindowAppId>("Omaweb", 1, 0, "WindowAppId"); }

} // namespace omaweb
