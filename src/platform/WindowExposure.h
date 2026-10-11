#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QWindow>

namespace omaweb {

// Says when a window is exposed again after it was not, which is what a
// compositor does to it when the reader switches workspace away and back.
//
// Qt gives a Quick item nothing to hear this with: the window stays visible
// and its items stay visible, and only the platform's expose event changes.
// A page's frames stop reaching the scene graph across that, and a page that
// is not drawing makes no new ones until it is told it is shown again (#517),
// so the view listens here and does that.
class WindowExposure : public QObject {
    Q_OBJECT
    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)

public:
    explicit WindowExposure(QObject *parent = nullptr);

    QWindow *window() const;
    void setWindow(QWindow *window);

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void windowChanged();
    // The window was exposed, then not, then exposed again. Its first exposure
    // is not one, because nothing has been lost yet. It is sent from the
    // filter, ahead of the window's own handling of the same event, so what
    // the listener does is in place before the window draws.
    void exposedAgain();
    // The window is going from exposed to not, said once per going. It is sent
    // from the filter too, while the window still holds its last picture:
    // once the window has handled the event, the page's frames are gone.
    void goingAway();

private:
    QPointer<QWindow> m_window;
    bool m_hasBeenExposed = false;
    bool m_wasAway = false;
};

// What its part of the window showed when it was last asked, drawn in that
// part until let go. A page's texture is black from a return of the window
// until the page makes a new frame, and covering it in the page's own colour
// shows a page whose ground is dark and whose content is light as a dark
// frame or two (#702). The window still holds the page as the reader left it
// while it is being told it is going away, so that is when this is taken.
//
// The picture fits only the size it was taken at: a part that is resized
// lets it go, and the caller falls back on the page's colour.
class PageSnapshot : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(bool held READ held NOTIFY heldChanged)

public:
    explicit PageSnapshot(QQuickItem *parent = nullptr);

    bool held() const;
    // Takes the picture at once, from what the window shows now. It renders
    // the whole window, which costs a few milliseconds.
    Q_INVOKABLE void take();
    Q_INVOKABLE void release();

signals:
    void heldChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    QImage m_picture;
    bool m_pictureChanged = false;
};

// Registers WindowExposure and PageSnapshot, which a page's return uses together.
void registerWindowExposure();

} // namespace omaweb
