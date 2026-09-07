#include "PortalWindow.h"

#include <QWindow>
#include <QtGlobal>

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

bool portalNameIsSafeToAsk(const QString &compiledQt, const QString &runningQt)
{
    // Exactly the same build, not a compatible one. Public Qt keeps its ABI
    // across a patch release and private Qt promises nothing, so the only Qt
    // this can reach into is the one it was compiled against.
    return !compiledQt.isEmpty() && compiledQt == runningQt;
}

QString portalWindowHandle(QWindow *window)
{
    if (!window) {
        return {};
    }
    if (!portalNameIsSafeToAsk(QStringLiteral(QT_VERSION_STR), QString::fromLatin1(qVersion()))) {
        // Nothing is asked and nothing crashes: the portal is given no name and
        // places the dialog itself, which is what printing did before this
        // existed. A rebuild against the Qt in front of the browser is what
        // gives the dialog its parent back.
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
