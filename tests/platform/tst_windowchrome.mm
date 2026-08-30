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
        tanto::installWindowChrome(qGuiApp);

        QQuickWindow window;
        window.setFlags(Qt::Window
            | Qt::ExpandedClientAreaHint
            | Qt::NoTitleBarBackgroundHint);
        window.setTitle(QStringLiteral("Tanto window chrome test"));
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
        tanto::installWindowChrome(qGuiApp);

        QQuickWindow window;
        window.setColor(QColor(0, 0, 0, 0));
        window.setFlags(Qt::Window
            | Qt::ExpandedClientAreaHint
            | Qt::NoTitleBarBackgroundHint);
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

    void blursBehindTantoOwnedSurfaces()
    {
        tanto::installWindowChrome(qGuiApp);

        QQuickWindow window;
        window.setColor(QColor(0, 0, 0, 0));
        window.setProperty("cornerRadius", 14.0);
        window.setFlags(Qt::Window
            | Qt::ExpandedClientAreaHint
            | Qt::NoTitleBarBackgroundHint);
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
};

QTEST_MAIN(WindowChromeTest)

#include "tst_windowchrome.moc"
