#include "WindowAppId.h"

#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

using namespace omaweb;

class WindowAppIdTest final : public QObject {
    Q_OBJECT

private slots:
    void asksNothingOffWayland();
    void namesTheWindowBeforeItIsMapped();
    void namesTheWindowAgainWhenItIsMadeAgain();
};

// A platform with no Wayland window interface, as the offscreen one a test
// runs under, has no name to give, and the window keeps the application's.
void WindowAppIdTest::asksNothingOffWayland()
{
    if (QGuiApplication::platformName() == u"wayland") {
        QSKIP("This is about a platform other than Wayland.");
    }
    QWindow window;
    WindowAppId appId;
    appId.setAppId(QStringLiteral("omaweb-tab"));
    appId.setWindow(&window);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(!appId.applied());
}

// On Wayland the name is sent with the window's role, before the compositor
// first maps it. Run under a Wayland session, which CI's offscreen run is not.
void WindowAppIdTest::namesTheWindowBeforeItIsMapped()
{
    if (QGuiApplication::platformName() != u"wayland") {
        QSKIP("Needs a Wayland session.");
    }
    QWindow window;
    WindowAppId appId;
    QSignalSpy appliedSpy(&appId, &WindowAppId::appliedChanged);
    appId.setWindow(&window);
    appId.setAppId(QStringLiteral("omaweb-tab"));
    window.show();
    QVERIFY(appId.applied());
    QCOMPARE(appliedSpy.count(), 1);
    QVERIFY(QTest::qWaitForWindowExposed(&window));
}

// A window hidden and shown again can be given a new platform window, which
// may sit where the last one did. That one is named too, before it is mapped.
// Run under a Wayland session, as above.
void WindowAppIdTest::namesTheWindowAgainWhenItIsMadeAgain()
{
    if (QGuiApplication::platformName() != u"wayland") {
        QSKIP("Needs a Wayland session.");
    }
    QWindow window;
    WindowAppId appId;
    QSignalSpy appliedSpy(&appId, &WindowAppId::appliedChanged);
    appId.setAppId(QStringLiteral("omaweb-tab"));
    appId.setWindow(&window);
    window.show();
    QVERIFY(appId.applied());

    window.destroy();
    QVERIFY(!appId.applied());
    window.show();
    QVERIFY(appId.applied());
    QCOMPARE(appliedSpy.count(), 3);
}

QTEST_MAIN(WindowAppIdTest)

#include "tst_windowappid.moc"
