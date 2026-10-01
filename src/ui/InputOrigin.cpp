#include "InputOrigin.h"

#include <QCoreApplication>
#include <QEvent>
#include <QQmlEngine>

namespace omaweb {

InputOrigin::InputOrigin(QObject *parent)
    : QObject(parent)
{
    if (auto *application = QCoreApplication::instance()) {
        application->installEventFilter(this);
    }
}

bool InputOrigin::pointer() const { return m_pointer; }

void InputOrigin::setPointer(bool pointer)
{
    if (m_pointer == pointer) {
        return;
    }
    m_pointer = pointer;
    emit pointerChanged();
}

bool InputOrigin::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    switch (event->type()) {
    // A key a window shortcut takes is offered to the shortcut first, so it
    // may never arrive as a press.
    case QEvent::KeyPress:
    case QEvent::ShortcutOverride:
    case QEvent::Shortcut:
        setPointer(false);
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::TouchBegin:
    case QEvent::TabletPress:
    case QEvent::Wheel:
    case QEvent::NativeGesture:
        setPointer(true);
        break;
    default:
        break;
    }
    return false;
}

void registerInputOrigin()
{
    qmlRegisterSingletonType<InputOrigin>("Omaweb", 1, 0, "InputOrigin",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new InputOrigin; });
}

} // namespace omaweb
