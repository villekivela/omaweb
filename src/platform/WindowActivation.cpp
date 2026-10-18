#include "WindowActivation.h"

#include <QWindow>

namespace omaweb {

void bringForward(QWindow *window, const QString &activationToken)
{
    if (!window) {
        return;
    }
    // Qt's Wayland window takes a token to activate with from the variable a
    // launched process finds it in, and has no public call to hand it one.
    if (!activationToken.isEmpty()) {
        qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
    }
    window->raise();
    window->requestActivate();
    // Wayland reads it and clears it inside the call. Elsewhere nothing does,
    // and a token is good for one activation, so no later one may find it.
    qunsetenv("XDG_ACTIVATION_TOKEN");
}

} // namespace omaweb
