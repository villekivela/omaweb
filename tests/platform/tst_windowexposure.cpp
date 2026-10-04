#include "WindowExposure.h"

#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

using namespace omaweb;

namespace {

// Counts the exposures the window handles itself, so a test can say whether
// the listener heard of one before the window did.
class RecordingWindow final : public QWindow {
public:
    void exposeEvent(QExposeEvent *event) override
    {
        QWindow::exposeEvent(event);
        if (isExposed()) {
            ++exposures;
        }
    }
    int exposures = 0;
};

} // namespace

class WindowExposureTest final : public QObject {
    Q_OBJECT

private slots:
    void staysQuietWhenTheWindowIsFirstShown();
    void speaksOnceForEachReturnOfTheWindow();
    void speaksBeforeTheWindowHandlesItsOwnExposure();
    void stopsListeningToAWindowItHasLeft();
};

static bool showAndWait(QWindow &window)
{
    window.show();
    return QTest::qWaitForWindowExposed(&window);
}

static void hideAndWait(QWindow &window)
{
    window.hide();
    QTRY_VERIFY(!window.isExposed());
}

// A window that was never lost has nothing to come back from.
void WindowExposureTest::staysQuietWhenTheWindowIsFirstShown()
{
    QWindow window;
    window.resize(120, 80);
    WindowExposure exposure;
    exposure.setWindow(&window);
    QSignalSpy returned(&exposure, &WindowExposure::exposedAgain);

    QVERIFY(showAndWait(window));
    QTest::qWait(50);

    QCOMPARE(returned.count(), 0);
}

// Hiding and showing is what a workspace switch does to the window, and each
// round trip is one return.
void WindowExposureTest::speaksOnceForEachReturnOfTheWindow()
{
    QWindow window;
    window.resize(120, 80);
    WindowExposure exposure;
    exposure.setWindow(&window);
    QSignalSpy returned(&exposure, &WindowExposure::exposedAgain);
    QVERIFY(showAndWait(window));

    hideAndWait(window);
    QCOMPARE(returned.count(), 0);
    QVERIFY(showAndWait(window));
    QCOMPARE(returned.count(), 1);

    hideAndWait(window);
    QVERIFY(showAndWait(window));
    QCOMPARE(returned.count(), 2);
}

// The first frame after a return is the one that shows a page that has lost
// its own, so the listener has to have acted by the time the window draws.
void WindowExposureTest::speaksBeforeTheWindowHandlesItsOwnExposure()
{
    RecordingWindow window;
    window.resize(120, 80);
    WindowExposure exposure;
    exposure.setWindow(&window);
    int exposuresHandledWhenTold = -1;
    QObject::connect(&exposure, &WindowExposure::exposedAgain, &exposure,
        [&] { exposuresHandledWhenTold = window.exposures; });
    QVERIFY(showAndWait(window));
    const int handledBefore = window.exposures;
    hideAndWait(window);

    QVERIFY(showAndWait(window));

    QCOMPARE(exposuresHandledWhenTold, handledBefore);
}

void WindowExposureTest::stopsListeningToAWindowItHasLeft()
{
    QWindow first;
    QWindow second;
    first.resize(120, 80);
    second.resize(120, 80);
    WindowExposure exposure;
    exposure.setWindow(&first);
    QSignalSpy returned(&exposure, &WindowExposure::exposedAgain);
    QVERIFY(showAndWait(first));
    exposure.setWindow(&second);

    hideAndWait(first);
    QVERIFY(showAndWait(first));

    QCOMPARE(returned.count(), 0);
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    WindowExposureTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_windowexposure.moc"
