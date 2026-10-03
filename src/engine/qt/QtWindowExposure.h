#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>

namespace omaweb {

// Says when a window is exposed again after it was not, which is what a
// compositor does to it when the reader switches workspace away and back.
//
// Qt gives a Quick item nothing to hear this with: the window stays visible
// and its items stay visible, and only the platform's expose event changes.
// A page's frames stop reaching the scene graph across that (#517), so the
// view listens here and attaches Chromium to the compositor again.
class QtWindowExposure : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged)

public:
    explicit QtWindowExposure(QObject *parent = nullptr);

    QQuickWindow *window() const;
    void setWindow(QQuickWindow *window);

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void windowChanged();
    // The window was exposed, then not, then exposed again. Its first exposure
    // is not one, because nothing has been lost yet.
    void exposedAgain();

private:
    QPointer<QQuickWindow> m_window;
    bool m_hasBeenExposed = false;
    bool m_unexposedSince = false;
};

void registerQtWindowExposure();

} // namespace omaweb
