#pragma once

#include <QObject>
#include <QPointer>
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

private:
    QPointer<QWindow> m_window;
    bool m_hasBeenExposed = false;
    bool m_unexposedSince = false;
};

void registerWindowExposure();

} // namespace omaweb
