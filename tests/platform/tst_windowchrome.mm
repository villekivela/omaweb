#include "WindowChrome.h"

#include <QColor>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTest>

#import <AppKit/AppKit.h>

class WindowChromeTest final : public QObject {
    Q_OBJECT

private slots:
    void expandedClientAreaRemainsAStandardWindow()
    {
        omaweb::installWindowChrome(qGuiApp);

        QQuickWindow window;
        window.setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
        window.setTitle(QStringLiteral("Omaweb window chrome test"));
        window.resize(480, 320);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCoreApplication::processEvents();

        auto *nativeView = reinterpret_cast<NSView *>(window.winId());
        NSWindow *nativeWindow = nativeView.window;
        QVERIFY(nativeWindow != nil);
        QVERIFY(nativeWindow.styleMask & NSWindowStyleMaskTitled);
        QVERIFY(nativeWindow.styleMask & NSWindowStyleMaskResizable);
        QCOMPARE(nativeWindow.titleVisibility, NSWindowTitleHidden);
        QVERIFY(nativeWindow.titlebarAppearsTransparent);
        QVERIFY([nativeWindow standardWindowButton:NSWindowCloseButton].hidden);
        QVERIFY([nativeWindow standardWindowButton:NSWindowMiniaturizeButton].hidden);
        QVERIFY([nativeWindow standardWindowButton:NSWindowZoomButton].hidden);
        QVERIFY([[nativeWindow accessibilitySubrole]
            isEqualToString:NSAccessibilityStandardWindowSubrole]);
    }

    void keepsAlphaSurfacesNonOpaque()
    {
        omaweb::installWindowChrome(qGuiApp);

        QQuickWindow window;
        window.setColor(QColor(0, 0, 0, 0));
        window.setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
        window.resize(480, 320);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCoreApplication::processEvents();

        auto *nativeView = reinterpret_cast<NSView *>(window.winId());
        NSWindow *nativeWindow = nativeView.window;
        QVERIFY(nativeWindow != nil);
        QVERIFY(!nativeWindow.opaque);
        QCOMPARE([nativeWindow.backgroundColor alphaComponent], 0.0);
    }

    void blursBehindOmawebOwnedSurfaces()
    {
        omaweb::installWindowChrome(qGuiApp);

        QQuickWindow window;
        window.setColor(QColor(0, 0, 0, 0));
        window.setProperty("cornerRadius", 14.0);
        window.setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
        window.resize(480, 320);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCoreApplication::processEvents();

        auto *nativeView = reinterpret_cast<NSView *>(window.winId());
        NSView *contentView = nativeView.window.contentView;
        NSVisualEffectView *backdrop = nil;
        for (NSView *sibling in contentView.superview.subviews) {
            if ([sibling isKindOfClass:NSVisualEffectView.class]) {
                backdrop = static_cast<NSVisualEffectView *>(sibling);
            }
        }
        QVERIFY(backdrop != nil);
        QCOMPARE(backdrop.blendingMode, NSVisualEffectBlendingModeBehindWindow);
        // Masked to the shell's rounded rect so the corners stay clear of the desktop.
        QCOMPARE(backdrop.layer.cornerRadius, 14.0);
        // Behind the scene graph, never layered over it.
        const auto backdropIndex = [contentView.superview.subviews indexOfObject:backdrop];
        const auto contentIndex = [contentView.superview.subviews indexOfObject:contentView];
        QVERIFY(backdropIndex < contentIndex);
    }

    // AppKit rebuilds the frame view across a fullscreen transition: the
    // standard window buttons come back under its own management, the backdrop
    // goes with the frame view that held it, and the window returns to an
    // opaque background. Coming back from fullscreen has to leave the window
    // looking the way it went in.
    //
    // The transition itself is animated, needs a display, and moves the window
    // to a Space of its own, so what is exercised here is the repair rather
    // than AppKit's own machinery: the window is left in the state a transition
    // leaves it in, and the notification that ends one is posted for it.
    void restoresItsChromeAfterAFullScreenTransition()
    {
        omaweb::installWindowChrome(qGuiApp);

        QQuickWindow window;
        window.setColor(QColor(0, 0, 0, 0));
        window.setProperty("cornerRadius", 14.0);
        window.setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
        window.resize(480, 320);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCoreApplication::processEvents();

        auto *nativeView = reinterpret_cast<NSView *>(window.winId());
        NSWindow *nativeWindow = nativeView.window;
        QVERIFY(nativeWindow != nil);

        // By identifier: AppKit puts visual-effect views of its own in the
        // frame view, and only the one Omaweb installed is Omaweb's to repair.
        const auto backdropOf = [](NSWindow *host) -> NSVisualEffectView * {
            for (NSView *sibling in host.contentView.superview.subviews) {
                if ([sibling.identifier isEqualToString:@"omaweb.window.backdrop"]) {
                    return static_cast<NSVisualEffectView *>(sibling);
                }
            }
            return nil;
        };

        NSVisualEffectView *original = backdropOf(nativeWindow);
        QVERIFY(original != nil);

        // What a rebuilt frame view leaves behind.
        [nativeWindow standardWindowButton:NSWindowCloseButton].hidden = NO;
        [nativeWindow standardWindowButton:NSWindowMiniaturizeButton].hidden = NO;
        [nativeWindow standardWindowButton:NSWindowZoomButton].hidden = NO;
        nativeWindow.titleVisibility = NSWindowTitleVisible;
        nativeWindow.titlebarAppearsTransparent = NO;
        nativeWindow.opaque = YES;
        nativeWindow.backgroundColor = NSColor.windowBackgroundColor;
        [original removeFromSuperview];
        QVERIFY(backdropOf(nativeWindow) == nil);

        [NSNotificationCenter.defaultCenter
            postNotificationName:NSWindowDidExitFullScreenNotification
                          object:nativeWindow];
        QCoreApplication::processEvents();

        QVERIFY([nativeWindow standardWindowButton:NSWindowCloseButton].hidden);
        QVERIFY([nativeWindow standardWindowButton:NSWindowMiniaturizeButton].hidden);
        QVERIFY([nativeWindow standardWindowButton:NSWindowZoomButton].hidden);
        QCOMPARE(nativeWindow.titleVisibility, NSWindowTitleHidden);
        QVERIFY(nativeWindow.titlebarAppearsTransparent);
        QVERIFY(nativeWindow.styleMask & NSWindowStyleMaskFullSizeContentView);
        QVERIFY(!nativeWindow.opaque);
        QCOMPARE([nativeWindow.backgroundColor alphaComponent], 0.0);

        NSVisualEffectView *restored = backdropOf(nativeWindow);
        QVERIFY(restored != nil);
        QCOMPARE(restored.layer.cornerRadius, 14.0);
        const auto backdropIndex =
            [nativeWindow.contentView.superview.subviews indexOfObject:restored];
        const auto contentIndex =
            [nativeWindow.contentView.superview.subviews indexOfObject:nativeWindow.contentView];
        QVERIFY(backdropIndex < contentIndex);
    }

    // A window that stops watching for the transition would leave the next one
    // unrepaired, so the observer belongs to the window and goes with it.
    void stopsWatchingAWindowThatHasGoneAway()
    {
        omaweb::installWindowChrome(qGuiApp);

        NSWindow *nativeWindow = nil;
        {
            QQuickWindow window;
            window.setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
            window.resize(480, 320);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QCoreApplication::processEvents();
            nativeWindow = [reinterpret_cast<NSView *>(window.winId()).window retain];
        }
        QCoreApplication::processEvents();

        // Nothing is left listening for this window, so posting the
        // notification reaches no observer holding a destroyed QWindow.
        [NSNotificationCenter.defaultCenter
            postNotificationName:NSWindowDidExitFullScreenNotification
                          object:nativeWindow];
        QCoreApplication::processEvents();
        [nativeWindow release];
    }
};

QTEST_MAIN(WindowChromeTest)

#include "tst_windowchrome.moc"
