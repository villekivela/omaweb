#pragma once

#include <QImage>
#include <QQuickItem>

namespace omaweb {

// Draws what its part of the window showed when take() was last called, until
// release(). The picture fits only the size and scale it was taken at, so a
// resize or a change of scale lets it go, and `held` turns false.
class PageSnapshot : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(bool held READ held NOTIFY heldChanged)

public:
    explicit PageSnapshot(QQuickItem *parent = nullptr);

    bool held() const;
    // Takes the picture at once, from what the window shows now. It renders
    // the whole window to do so, which costs a few milliseconds.
    Q_INVOKABLE void take();
    Q_INVOKABLE void release();

signals:
    void heldChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    QImage m_picture;
    bool m_pictureChanged = false;
};

void registerPageSnapshot();

} // namespace omaweb
