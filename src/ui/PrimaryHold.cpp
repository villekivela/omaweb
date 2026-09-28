#include "PrimaryHold.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlEngine>
#include <QWindow>

namespace omaweb {

namespace {

    // Long enough that Primary pressed on the way to a chord comes up, or is
    // joined, before anything is drawn.
    constexpr int holdDelayMs = 400;

} // namespace

PrimaryHold::PrimaryHold(QObject *parent)
    : QObject(parent)
{
    m_delay.setSingleShot(true);
    m_delay.setInterval(holdDelayMs);
    connect(&m_delay, &QTimer::timeout, this, [this] { setHeld(m_pressed); });
    if (auto *application = QCoreApplication::instance()) {
        application->installEventFilter(this);
    }
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
        [this](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive) {
                release();
            }
        });
}

bool PrimaryHold::held() const { return m_held; }

bool PrimaryHold::eventFilter(QObject *watched, QEvent *event)
{
    // Every item on the way to the focused one is handed the same event, so
    // only the window's copy is read.
    if (qobject_cast<QWindow *>(watched) == nullptr) {
        return false;
    }
    switch (event->type()) {
    case QEvent::KeyPress: {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->isAutoRepeat()) {
            break;
        }
        // Some platforms report Primary's own press with its modifier set and
        // some without; anything else held with it is a chord.
        const auto others = key->modifiers() & ~(Qt::ControlModifier | Qt::KeypadModifier);
        if (key->key() == Qt::Key_Control && others == Qt::NoModifier) {
            m_pressed = true;
            m_delay.start();
        } else {
            release();
        }
        break;
    }
    case QEvent::KeyRelease:
        if (!static_cast<QKeyEvent *>(event)->isAutoRepeat()) {
            release();
        }
        break;
    case QEvent::FocusOut:
    case QEvent::WindowDeactivate:
        release();
        break;
    default:
        break;
    }
    return false;
}

void PrimaryHold::release()
{
    m_pressed = false;
    m_delay.stop();
    setHeld(false);
}

void PrimaryHold::setHeld(bool held)
{
    if (m_held == held) {
        return;
    }
    m_held = held;
    emit heldChanged();
}

void registerPrimaryHold()
{
    qmlRegisterSingletonType<PrimaryHold>("Omaweb", 1, 0, "PrimaryHold",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new PrimaryHold; });
}

} // namespace omaweb
