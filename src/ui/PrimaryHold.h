#pragma once

#include <QObject>
#include <QTimer>

namespace omaweb {

// Whether the reader is holding Primary on its own, long enough to be asking
// what it does rather than typing a chord. The key reaches whatever holds the
// keyboard, a page as often as the chrome, so it is read where every key
// passes: the application, before any item is handed it. Another key joining
// Primary, Primary coming up, or the window losing the keyboard ends the hold.
class PrimaryHold final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool held READ held NOTIFY heldChanged)

public:
    explicit PrimaryHold(QObject *parent = nullptr);

    bool held() const;

signals:
    void heldChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void release();
    void setHeld(bool held);

    QTimer m_delay;
    bool m_pressed = false;
    bool m_held = false;
};

// Makes `PrimaryHold` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerPrimaryHold();

} // namespace omaweb
