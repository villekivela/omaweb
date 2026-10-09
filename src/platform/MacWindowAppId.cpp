#include "WindowAppId.h"

namespace omaweb {

// A macOS window has no name of its own for the window server to match, so
// there is nothing to ask.
void watchWindowRole(QWindow *, const QString &, QObject *, const std::function<void()> &) { }

} // namespace omaweb
