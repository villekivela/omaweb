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

// Whether the name is safe to ask Qt for at all. It is read out of Qt through a
// private class, which carries no ABI guarantee between Qt builds, so a browser
// meeting a Qt it was not compiled against would be calling whatever now stands
// where that function stood. Both versions are arguments so the rule can be
// stated rather than only run on whatever Qt the checking machine has.
bool portalNameIsSafeToAsk(const QString &compiledQt, const QString &runningQt);

} // namespace omaweb
