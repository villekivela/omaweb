#include "WindowChrome.h"

namespace omaweb {

// Empty by design rather than unimplemented. The two things the macOS adapter
// installs are already accounted for here: `Main.qml` asks for
// `Qt.FramelessWindowHint` off macOS, so the window has no frame to remove, and
// blur behind a transparent surface belongs to the compositor. Hyprland has no
// client-side blur protocol to bind, so a client cannot ask for blur; it blurs
// what shows through from its own `decoration:blur` setting, which is the
// reader's configuration rather than the browser's. See ADR 0002.
void installWindowChrome(QGuiApplication *) { }

} // namespace omaweb
