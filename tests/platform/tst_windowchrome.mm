#include "WindowChrome.h"

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
};

QTEST_MAIN(WindowChromeTest)

#include "tst_windowchrome.moc"
