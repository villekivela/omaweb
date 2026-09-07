#pragma once

#include <QString>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

namespace omaweb {

// The name a desktop portal takes for the window a dialog belongs to, so the
// portal's dialog stands over the window that asked for it rather than wherever
// the compositor happens to put a window of its own.
//
// On Wayland the name is `wayland:<handle>`, which is a surface exported through
// `zxdg_exporter_v2`; on X11 it is `x11:<xid>`. Empty means no name could be
// had, and a portal given an empty one shows an unparented dialog, which is
// what every portal call did before this existed.
QString portalWindowHandle(QWindow *window);

} // namespace omaweb
