#include "PortalWindow.h"
#include "WindowAppId.h"

#include <QtGlobal>

// Qt's private Wayland window interface, which hands out the window's
// `xdg_toplevel` and says when it is made. It carries no ABI promise between Qt
// builds, which is why the call below is made only on the Qt this was compiled
// against.
#include <QtGui/qpa/qplatformwindow_p.h>

#include <wayland-client-core.h>

// Declared rather than included: the toplevel is only passed through, and the
// protocol's generated header is Qt Wayland's private one, which no build job
// otherwise needs.
struct xdg_toplevel;

namespace omaweb {

namespace {

    // `xdg_toplevel.set_app_id` is request 3 of xdg-shell: destroy, set_parent,
    // set_title, set_app_id. A protocol request's number is part of the stable
    // protocol and never moves, which is what makes sending it by number safe.
    constexpr uint32_t setAppIdRequest = 3;

} // namespace

void watchWindowRole(
    QWindow *window, const QString &appId, QObject *context, const std::function<void()> &done)
{
    // The same rule as the portal's name: exactly the Qt this was built
    // against, or nothing is asked and the window keeps the application's
    // name.
    if (!window
        || !portalNameIsSafeToAsk(
            QStringLiteral(QT_VERSION_STR), QString::fromLatin1(qVersion()))) {
        return;
    }
    using WaylandWindow = QNativeInterface::Private::QWaylandWindow;
    auto *wayland = window->nativeInterface<WaylandWindow>();
    if (!wayland) {
        return;
    }
    const auto name = appId.toUtf8();
    const auto send = [wayland, name, done] {
        auto *toplevel = wayland->surfaceRole<::xdg_toplevel>();
        if (!toplevel) {
            return;
        }
        auto *proxy = reinterpret_cast<wl_proxy *>(toplevel);
        wl_proxy_marshal_flags(
            proxy, setAppIdRequest, nullptr, wl_proxy_get_version(proxy), 0, name.constData());
        done();
    };
    // Once now, for a window whose role is already made, and again whenever
    // it is made anew: hiding a window and showing it again makes a new one,
    // which starts with the application's name.
    QObject::connect(wayland, &WaylandWindow::surfaceRoleCreated, context, send);
    send();
}

} // namespace omaweb
