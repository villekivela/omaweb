#pragma once

#include <QObject>

namespace omaweb {

// Whether the reader's last deliberate input was the pointer or a key. The
// chrome's movement is for the pointer: it lets the eye follow where something
// went. A key is a decision already made, and its result arrives settled.
//
// A press, a tap, a wheel turn or a gesture is the pointer. A key press is a
// key, whether a page, a field or a window shortcut takes it. Moving the
// pointer is neither, since it starts nothing. Every input reaches the
// application before any item is handed it, so that is where this listens.
//
// A command the Omnibar's command scope runs is the reader's typed decision
// even when the row was clicked, so the Omnibar says so by setting `pointer`
// false before it runs one.
class InputOrigin final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool pointer READ pointer WRITE setPointer NOTIFY pointerChanged)

public:
    explicit InputOrigin(QObject *parent = nullptr);

    bool pointer() const;
    void setPointer(bool pointer);

signals:
    void pointerChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool m_pointer = true;
};

// Makes `InputOrigin` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerInputOrigin();

} // namespace omaweb
