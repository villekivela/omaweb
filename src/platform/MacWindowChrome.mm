#include "WindowChrome.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QWindow>

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

namespace tanto {
namespace {

NSString *const kBackdropIdentifier = @"tanto.window.backdrop";

// Tier one of the surface contract in ADR 0002: a native blur behind every
// Tanto-owned surface. The webpage viewport paints its own opaque backing on
// top, so nothing here reaches the page. NSVisualEffectView drops to a plain
// opaque fill by itself when the system asks for reduced transparency, which
// is the fallback that contract calls for.
void installBackdrop(QWindow *window, NSWindow *nativeWindow)
{
    NSView *contentView = nativeWindow.contentView;
    NSView *frameView = contentView.superview;
    if (!frameView) {
        return;
    }
    for (NSView *existing in frameView.subviews) {
        if ([existing.identifier isEqualToString:kBackdropIdentifier]) {
            return;
        }
    }

    auto *backdrop = [[NSVisualEffectView alloc] initWithFrame:frameView.bounds];
    backdrop.identifier = kBackdropIdentifier;
    backdrop.material = NSVisualEffectMaterialUnderWindowBackground;
    backdrop.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    backdrop.state = NSVisualEffectStateFollowsWindowActiveState;
    backdrop.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    // The shell is a rounded rectangle over a transparent window, so an
    // unmasked backdrop would blur the desktop in the four corners the shell
    // does not cover. QML owns the radius and hands it over as a property.
    const auto radius = window->property("cornerRadius");
    backdrop.wantsLayer = YES;
    backdrop.layer.cornerRadius = radius.isValid() ? radius.toReal() : 0;
    backdrop.layer.masksToBounds = YES;

    // Below the Qt view rather than inside it: subviews of a layer-backed Quick
    // view composite above the scene graph, which would bury the interface.
    [frameView addSubview:backdrop positioned:NSWindowBelow relativeTo:contentView];
}

void configureWindow(QWindow *window)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa")
        || !window
        || !window->flags().testFlag(Qt::ExpandedClientAreaHint)) {
        return;
    }

    auto *nativeView = reinterpret_cast<NSView *>(window->winId());
    NSWindow *nativeWindow = nativeView.window;
    if (!nativeWindow) {
        return;
    }

    nativeWindow.styleMask |= NSWindowStyleMaskTitled
        | NSWindowStyleMaskClosable
        | NSWindowStyleMaskMiniaturizable
        | NSWindowStyleMaskResizable
        | NSWindowStyleMaskFullSizeContentView;
    nativeWindow.titleVisibility = NSWindowTitleHidden;
    nativeWindow.titlebarAppearsTransparent = YES;
    [nativeWindow standardWindowButton:NSWindowCloseButton].hidden = YES;
    [nativeWindow standardWindowButton:NSWindowMiniaturizeButton].hidden = YES;
    [nativeWindow standardWindowButton:NSWindowZoomButton].hidden = YES;

    // Changing the style mask makes AppKit rebuild the frame view, which resets the
    // window to an opaque background and flattens every translucent surface Tanto
    // draws against it. Restore what Qt configured for an alpha surface, and only
    // for an alpha surface — a window with no alpha buffer would render black.
    if (window->format().alphaBufferSize() > 0) {
        nativeWindow.opaque = NO;
        nativeWindow.backgroundColor = NSColor.clearColor;
        installBackdrop(window, nativeWindow);
    }
}

class WindowChromeFilter final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::PlatformSurface) {
            const auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
            if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
                configureWindow(qobject_cast<QWindow *>(watched));
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

void installWindowChrome(QGuiApplication *application)
{
    auto *filter = new WindowChromeFilter(application);
    application->installEventFilter(filter);
}

} // namespace tanto
