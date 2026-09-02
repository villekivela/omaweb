#include "WindowChrome.h"

#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QPlatformSurfaceEvent>
#include <QWindow>

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

namespace omaweb {
namespace {

NSString *const kBackdropIdentifier = @"omaweb.window.backdrop";

bool windowIsFullScreen(NSWindow *nativeWindow)
{
    return (nativeWindow.styleMask & NSWindowStyleMaskFullScreen) != 0;
}

// A window filling the screen has no corners to round. The native fullscreen
// bit is read rather than the window's `cornerRadius` property, because that
// property follows Qt's own view of the transition and nothing orders that
// against the notification this is applied from.
CGFloat backdropCornerRadius(QWindow *window, NSWindow *nativeWindow)
{
    if (windowIsFullScreen(nativeWindow)) {
        return 0;
    }
    const auto radius = window->property("cornerRadius");
    return radius.isValid() ? radius.toReal() : 0;
}

// Tier one of the surface contract in ADR 0002: a native blur behind every
// Omaweb-owned surface. The webpage viewport paints its own opaque backing on
// top, so nothing here reaches the page. NSVisualEffectView drops to a plain
// opaque fill by itself when the system asks for reduced transparency, which
// is the fallback that contract calls for.
//
// The backdrop is a subview of the frame view, which AppKit rebuilds across a
// fullscreen transition — so this both installs one and repairs the one that
// went with the old frame view.
void ensureBackdrop(QWindow *window, NSWindow *nativeWindow)
{
    NSView *contentView = nativeWindow.contentView;
    NSView *frameView = contentView.superview;
    if (!frameView) {
        return;
    }
    const auto radius = backdropCornerRadius(window, nativeWindow);
    for (NSView *existing in frameView.subviews) {
        if ([existing.identifier isEqualToString:kBackdropIdentifier]) {
            // The shell squares its corners in fullscreen and rounds them again
            // on the way back, and the mask follows it.
            existing.layer.cornerRadius = radius;
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
    backdrop.wantsLayer = YES;
    backdrop.layer.cornerRadius = radius;
    backdrop.layer.masksToBounds = YES;

    // Below the Qt view rather than inside it: subviews of a layer-backed Quick
    // view composite above the scene graph, which would bury the interface.
    [frameView addSubview:backdrop positioned:NSWindowBelow relativeTo:contentView];
}

// Everything about a Omaweb window that AppKit does not keep for itself. It is
// applied when the window's surface is created and again after every fullscreen
// transition, because AppKit rebuilds the frame view across one: the standard
// window buttons come back under its own management, the frame view loses the
// backdrop that was a subview of it, and the window returns to an opaque
// background. Nothing here reads the state it is setting, so re-applying it is
// the same as applying it.
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

    // The style mask is AppKit's own while a window is fullscreen — it carries
    // the fullscreen bit there, and writing the mask back would fight the state
    // it is in. Every other window in that state is left as it is until the
    // transition out, which applies this again.
    if (!windowIsFullScreen(nativeWindow)) {
        nativeWindow.styleMask |= NSWindowStyleMaskTitled
            | NSWindowStyleMaskClosable
            | NSWindowStyleMaskMiniaturizable
            | NSWindowStyleMaskResizable
            | NSWindowStyleMaskFullSizeContentView;
    }
    nativeWindow.titleVisibility = NSWindowTitleHidden;
    nativeWindow.titlebarAppearsTransparent = YES;
    [nativeWindow standardWindowButton:NSWindowCloseButton].hidden = YES;
    [nativeWindow standardWindowButton:NSWindowMiniaturizeButton].hidden = YES;
    [nativeWindow standardWindowButton:NSWindowZoomButton].hidden = YES;

    // Changing the style mask makes AppKit rebuild the frame view, which resets the
    // window to an opaque background and flattens every translucent surface Omaweb
    // draws against it. Restore what Qt configured for an alpha surface, and only
    // for an alpha surface — a window with no alpha buffer would render black.
    if (window->format().alphaBufferSize() > 0) {
        nativeWindow.opaque = NO;
        nativeWindow.backgroundColor = NSColor.clearColor;
        ensureBackdrop(window, nativeWindow);
    }
}

class WindowChromeFilter final : public QObject {
public:
    using QObject::QObject;

    ~WindowChromeFilter() override
    {
        const auto watched = m_observers.keys();
        for (auto *window : watched) {
            forget(window);
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::PlatformSurface) {
            return QObject::eventFilter(watched, event);
        }
        auto *window = qobject_cast<QWindow *>(watched);
        if (!window) {
            return QObject::eventFilter(watched, event);
        }
        const auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
        if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
            configureWindow(window);
            watchFullScreenTransitions(window);
        } else {
            forget(window);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    // The transition notifications rather than Qt's own visibility signal: they
    // arrive once AppKit has finished rebuilding the window, which is the only
    // moment the chrome can be put back and stay put.
    void watchFullScreenTransitions(QWindow *window)
    {
        if (QGuiApplication::platformName() != QStringLiteral("cocoa")
            || !window->flags().testFlag(Qt::ExpandedClientAreaHint)
            || m_observers.contains(window)) {
            return;
        }
        auto *nativeView = reinterpret_cast<NSView *>(window->winId());
        NSWindow *nativeWindow = nativeView.window;
        if (!nativeWindow) {
            return;
        }

        auto *tokens = [[NSMutableArray alloc] init];
        for (NSNotificationName name : @[NSWindowDidEnterFullScreenNotification,
                 NSWindowDidExitFullScreenNotification]) {
            id token = [[NSNotificationCenter defaultCenter]
                addObserverForName:name
                            object:nativeWindow
                             queue:nil
                        usingBlock:^(NSNotification *) { configureWindow(window); }];
            if (token) {
                [tokens addObject:token];
            }
        }
        m_observers.insert(window, tokens);
    }

    void forget(QWindow *window)
    {
        auto *tokens = m_observers.take(window);
        if (!tokens) {
            return;
        }
        for (id token in tokens) {
            [[NSNotificationCenter defaultCenter] removeObserver:token];
        }
        [tokens release];
    }

    // This file is compiled without ARC, as the rest of Omaweb's AppKit code is,
    // so the observer tokens are owned here and released with their window.
    QHash<QWindow *, NSMutableArray *> m_observers;
};

} // namespace

void installWindowChrome(QGuiApplication *application)
{
    auto *filter = new WindowChromeFilter(application);
    application->installEventFilter(filter);
}

} // namespace omaweb
