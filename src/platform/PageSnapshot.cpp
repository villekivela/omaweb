#include "PageSnapshot.h"

#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGImageNode>

namespace omaweb {

PageSnapshot::PageSnapshot(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

bool PageSnapshot::held() const { return !m_picture.isNull(); }

// The page's texture is black from a return of its window until the page
// makes a new frame, and covering it in the page's own colour shows a page
// whose ground is dark and whose content is light as a dark frame or two
// (#702). Told that the window is going away, ahead of the window's own
// handling, the window still renders the page as the reader left it: a grab
// from the scene 60 ms later is already black.
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

// Stretched, a picture puts the page's parts where they no longer are.
void PageSnapshot::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        release();
    }
}

void PageSnapshot::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if (change == ItemDevicePixelRatioHasChanged) {
        release();
    }
}

void registerPageSnapshot() { qmlRegisterType<PageSnapshot>("Omaweb", 1, 0, "PageSnapshot"); }

} // namespace omaweb
