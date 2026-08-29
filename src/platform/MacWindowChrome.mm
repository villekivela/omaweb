#include "WindowChrome.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QWindow>

#import <AppKit/AppKit.h>

namespace tanto {
namespace {

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
