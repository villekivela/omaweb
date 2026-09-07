#include "PortalWindow.h"

#include <QWindow>

// Qt already exports the surface a portal needs, because its own dialogs are
// portal dialogs and they are parented. Nothing in its public API hands the
// resulting name out, so this is the one place Omaweb reaches past it.
//
// The alternative was binding `zxdg_exporter_v2` here and exporting the surface
// a second time, which needs the window's `wl_surface` and reaches past the
// public API for that instead. Exporting twice what Qt has already exported
// once is the worse of the two.
#include <QtGui/private/qdesktopunixservices_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>

namespace omaweb {

QString portalWindowHandle(QWindow *window)
{
    if (!window) {
        return {};
    }
    auto *integration = QGuiApplicationPrivate::platformIntegration();
    if (!integration) {
        return {};
    }
    // A platform that is not one of the Unix window systems, the offscreen one
    // a test runs under among them, has no name to give and says so.
    auto *services = dynamic_cast<QDesktopUnixServices *>(integration->services());
    if (!services) {
        return {};
    }
    return services->portalWindowIdentifier(window);
}

} // namespace omaweb
