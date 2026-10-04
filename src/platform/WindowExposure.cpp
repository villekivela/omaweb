#include "WindowExposure.h"

#include <QEvent>
#include <QQmlEngine>

namespace omaweb {

WindowExposure::WindowExposure(QObject *parent)
    : QObject(parent)
{
}

QWindow *WindowExposure::window() const { return m_window; }

void WindowExposure::setWindow(QWindow *window)
{
    if (m_window == window) {
        return;
    }
    if (m_window) {
        m_window->removeEventFilter(this);
    }
    m_window = window;
    m_hasBeenExposed = window && window->isExposed();
    m_unexposedSince = false;
    if (m_window) {
        m_window->installEventFilter(this);
    }
    emit windowChanged();
}

bool WindowExposure::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::Expose) {
        // An expose event with an empty region is how Qt says the window went
        // away, so the state is read from the window rather than the event.
        if (m_window->isExposed()) {
            const bool returned = m_hasBeenExposed && m_unexposedSince;
            m_hasBeenExposed = true;
            m_unexposedSince = false;
            if (returned) {
                emit exposedAgain();
            }
        } else if (m_hasBeenExposed) {
            m_unexposedSince = true;
        }
    }
    return false;
}

void registerWindowExposure() { qmlRegisterType<WindowExposure>("Omaweb", 1, 0, "WindowExposure"); }

} // namespace omaweb
