#pragma once

#include <QString>

class QWindow;

namespace omaweb {

// Brings the window forward and asks for the keyboard, the way a launch that
// reached the running browser expects to see it.
//
// A Wayland window cannot take focus on its own say. The compositor gives it
// focus for a token it issued to the app the reader was using, which hands it
// on with the launch. Without one, Qt asks for a token itself, and a
// compositor that is strict about focus may only mark the window as wanting
// attention.
void bringForward(QWindow *window, const QString &activationToken);

} // namespace omaweb
