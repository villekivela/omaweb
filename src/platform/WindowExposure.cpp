#include "WindowExposure.h"

#include <QEvent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGImageNode>

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
    m_wasAway = false;
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
            const bool returned = m_hasBeenExposed && m_wasAway;
            m_hasBeenExposed = true;
            m_wasAway = false;
            if (returned) {
                emit exposedAgain();
            }
        } else if (m_hasBeenExposed && !m_wasAway) {
            m_wasAway = true;
            emit goingAway();
        }
    }
    return false;
}

PageSnapshot::PageSnapshot(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

bool PageSnapshot::held() const { return !m_picture.isNull(); }

void PageSnapshot::take()
{
    auto *shown = window();
    if (!shown || width() <= 0 || height() <= 0) {
        return;
    }
    const auto ratio = shown->effectiveDevicePixelRatio();
    const auto area = mapRectToScene(boundingRect());
    const QRectF pixels(area.topLeft() * ratio, area.size() * ratio);
    auto picture = shown->grabWindow().copy(pixels.toAlignedRect());
    if (picture.isNull()) {
        return;
    }
    picture.setDevicePixelRatio(ratio);
    const bool wasHeld = held();
    m_picture = std::move(picture);
    m_pictureChanged = true;
    update();
    if (!wasHeld) {
        emit heldChanged();
    }
}

void PageSnapshot::release()
{
    if (!held()) {
        return;
    }
    m_picture = {};
    m_pictureChanged = true;
    update();
    emit heldChanged();
}

QSGNode *PageSnapshot::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
{
    if (!held()) {
        delete node;
        return nullptr;
    }
    auto *image = static_cast<QSGImageNode *>(node);
    if (!image) {
        image = window()->createImageNode();
        image->setOwnsTexture(true);
        m_pictureChanged = true;
    }
    if (m_pictureChanged) {
        image->setTexture(window()->createTextureFromImage(m_picture));
        m_pictureChanged = false;
    }
    image->setRect(boundingRect());
    return image;
}

void PageSnapshot::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        release();
    }
}

void registerWindowExposure()
{
    qmlRegisterType<WindowExposure>("Omaweb", 1, 0, "WindowExposure");
    qmlRegisterType<PageSnapshot>("Omaweb", 1, 0, "PageSnapshot");
}

} // namespace omaweb
