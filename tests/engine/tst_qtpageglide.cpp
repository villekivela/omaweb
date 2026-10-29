#include "QtPageGlide.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace probe {

struct Heard {
    Qt::ScrollPhase phase;
    QPoint angle;
};

// Stands in for QtWebEngine's input item, which the glide knows by its class name.
class RenderWidgetHostViewQtDelegateItem final : public QQuickItem {
    Q_OBJECT

public:
    QList<Heard> heard;

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        heard.append({event->phase(), event->angleDelta()});
        event->accept();
    }
};

// Any other item, such as the sidebar the Space swipe stands over.
class OtherItem final : public QQuickItem {
    Q_OBJECT

public:
    QList<Heard> heard;

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        heard.append({event->phase(), event->angleDelta()});
        event->accept();
    }
};

} // namespace probe

class QtPageGlideTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void everyQmlEngineReachesTheOneGlide();
    void init();
    void cleanup();
    void flickGlidesOnAndSlowsToAStop();
    void dragThatEndsAtRestDoesNotGlide();
    void mouseWheelScrollsAsBefore();
    void gestureOverAnotherItemDoesNotGlide();
    void touchingAgainStopsTheGlide();
    void inputStopsTheGlide_data();
    void inputStopsTheGlide();
    void restingFingersStopTheGlide();
    void pageLoadStopsTheGlide();
    void hiddenPageStopsTheGlide_data();
    void hiddenPageStopsTheGlide();
    void chromeViewGlidesInPixels();
    void chromeViewHeldAtItsEndDoesNotGlide();
    void slantedSwipeGlidesAListThatScrollsOneWay();
    void chromeViewGlideStopsAtItsEnd();

private:
    // Flicks the page and waits for the glide's first steps.
    bool startGlide();
    // The glide ended with one `ScrollEnd` and sends nothing more.
    bool endedOnce();

    // One for the process, as the browser has, since each watches every event.
    omaweb::QtPageGlide m_glideItself;
    omaweb::QtPageGlide *m_glide = &m_glideItself;
    std::unique_ptr<QQuickItem> m_view;
    probe::RenderWidgetHostViewQtDelegateItem *m_page = nullptr;
};

namespace {

// One event as Qt delivers it: to the window first, then to the item under the pointer.
void wheel(QObject *to, Qt::ScrollPhase phase, QPoint angle, quint64 at,
    Qt::MouseEventSource source = Qt::MouseEventSynthesizedBySystem)
{
    static QObject window;
    for (auto *receiver : {&window, to}) {
        QWheelEvent event(QPointF(40, 40), QPointF(140, 140), QPoint(), angle, Qt::NoButton,
            Qt::NoModifier, phase, false, source);
        event.setTimestamp(at);
        QCoreApplication::sendEvent(receiver, &event);
    }
}

// A begin or an end, which has no delta. Qt 6.12's `QWindowSystemInterface::handleWheelEvent`
// sends an event whose angle delta is neither only vertical nor only horizontal twice, the second
// time for Qt 4's horizontal wheel, so both arrive twice, the same each time.
void edge(QObject *to, Qt::ScrollPhase phase, quint64 at, Qt::MouseEventSource source)
{
    wheel(to, phase, QPoint(), at, source);
    wheel(to, phase, QPoint(), at, source);
}

// Fingers moving the page 30 angle units every 10 ms, from 1000 to 1100 ms, as Qt's Wayland
// plugin delivers them. Its begin says the source is not synthesized, and the rest say it is.
void drag(QObject *to)
{
    edge(to, Qt::ScrollBegin, 990, Qt::MouseEventNotSynthesized);
    for (quint64 at = 1000; at <= 1100; at += 10) {
        wheel(to, Qt::ScrollUpdate, QPoint(0, 30), at);
    }
}

// The fingers lifting as they move, at 3000 units a second. GTK's friction of 4 a second takes the
// page 750 more in ln(375) / 4 seconds, when two are left.
void flick(QObject *to)
{
    drag(to);
    edge(to, Qt::ScrollEnd, 1100, Qt::MouseEventSynthesizedBySystem);
}

// The fingers on a view in the chrome, which scrolls by their pixels: `move` every 10 ms from 1000
// to 1100 ms, then the lift, at a hundred times `move` a second. Qt's Wayland plugin gives each an
// angle twelve times its pixels.
void flickPixels(QQuickItem *view, QPoint move = QPoint(0, -10))
{
    const auto send
        = [view](Qt::ScrollPhase phase, QPoint pixels, quint64 at, Qt::MouseEventSource source) {
              QWheelEvent event(QPointF(40, 40), QPointF(140, 140), pixels,
                  pixels * omaweb::QtPageGlide::anglePerPixel, Qt::NoButton, Qt::NoModifier, phase,
                  false, source);
              event.setTimestamp(at);
              QCoreApplication::sendEvent(view, &event);
          };
    send(Qt::ScrollBegin, {}, 990, Qt::MouseEventNotSynthesized);
    for (quint64 at = 1000; at <= 1100; at += 10) {
        send(Qt::ScrollUpdate, move, at, Qt::MouseEventSynthesizedBySystem);
    }
    send(Qt::ScrollEnd, {}, 1100, Qt::MouseEventSynthesizedBySystem);
}

// A `Flickable` 100 pixels high over 1000 pixels of content, in a window, scrolled `contentY`
// down them.
struct ChromeView {
    explicit ChromeView(qreal contentY = 0)
    {
        component.setData("import QtQuick\n"
                          "Flickable { width: 100; height: 100; contentHeight: 1000 }",
            QUrl());
        view.reset(qobject_cast<QQuickItem *>(component.create()));
        view->setParentItem(window.contentItem());
        view->setProperty("contentY", contentY);
    }

    qreal contentY() const { return view->property("contentY").toReal(); }

    QQuickWindow window;
    QQmlEngine engine;
    QQmlComponent component {&engine};
    std::unique_ptr<QQuickItem> view;
};

// What the page hears of the fingers before the lift: two begins and eleven updates.
constexpr qsizetype fingerEvents = 13;

int ends(const QList<probe::Heard> &heard)
{
    return static_cast<int>(std::count_if(heard.cbegin(), heard.cend(),
        [](const probe::Heard &one) { return one.phase == Qt::ScrollEnd; }));
}

} // namespace

void QtPageGlideTest::initTestCase() { omaweb::registerQtPageGlide(m_glide); }

// A Tab window's page and the main window's can be in different QML engines, and a load in
// either stops the glide.
void QtPageGlideTest::everyQmlEngineReachesTheOneGlide()
{
    QQmlEngine main;
    QQmlEngine tabWindow;
    std::vector<std::unique_ptr<QObject>> made;
    for (auto *engine : {&main, &tabWindow}) {
        QQmlComponent component(engine);
        component.setData(
            "import QtQml\nimport Omaweb.Engine\n"
            "QtObject { property bool reached: typeof QtPageGlide.stopIn === \"function\" }",
            QUrl());
        made.emplace_back(component.create());
        QVERIFY2(made.back(), qPrintable(component.errorString()));
        QVERIFY(made.back()->property("reached").toBool());
    }
}

void QtPageGlideTest::init()
{
    m_view = std::make_unique<QQuickItem>();
    m_page = new probe::RenderWidgetHostViewQtDelegateItem;
    m_page->setParentItem(m_view.get());
}

void QtPageGlideTest::cleanup()
{
    m_glide->stop();
    m_view.reset();
    m_page = nullptr;
}

void QtPageGlideTest::flickGlidesOnAndSlowsToAStop()
{
    QElapsedTimer since;
    since.start();
    flick(m_page);
    QCOMPARE(m_page->heard.size(), fingerEvents);
    QCOMPARE(m_page->heard.last().phase, Qt::ScrollUpdate);

    QTRY_COMPARE_WITH_TIMEOUT(ends(m_page->heard), 1, 4000);
    QVERIFY(since.elapsed() >= std::floor(std::log(375.0) / 4 * 1000));
    QCOMPARE(m_page->heard.last().phase, Qt::ScrollEnd);

    const auto glide = m_page->heard.mid(fingerEvents, m_page->heard.size() - fingerEvents - 1);
    QVERIFY(glide.size() > 10);
    int travelled = 0;
    for (const auto &step : glide) {
        QCOMPARE(step.phase, Qt::ScrollUpdate);
        QCOMPARE(step.angle.x(), 0);
        QVERIFY(step.angle.y() > 0);
        travelled += step.angle.y();
    }
    QVERIFY2(travelled >= 747 && travelled <= 750, qPrintable(QString::number(travelled)));
    QVERIFY(glide.first().angle.y() > glide.last().angle.y());

    // Nothing more once it has stopped.
    const auto heard = m_page->heard.size();
    QTest::qWait(100);
    QCOMPARE(m_page->heard.size(), heard);
}

bool QtPageGlideTest::startGlide()
{
    flick(m_page);
    return QTest::qWaitFor([this] { return m_page->heard.size() >= fingerEvents + 2; }, 1000)
        && m_glide->gliding() && ends(m_page->heard) == 0;
}

bool QtPageGlideTest::endedOnce()
{
    if (m_glide->gliding() || ends(m_page->heard) != 1
        || m_page->heard.last().phase != Qt::ScrollEnd) {
        return false;
    }
    const auto heard = m_page->heard.size();
    QTest::qWait(100);
    return m_page->heard.size() == heard;
}

// The fingers stopped 200 ms before they lifted, longer than the speed is measured over.
void QtPageGlideTest::dragThatEndsAtRestDoesNotGlide()
{
    drag(m_page);
    edge(m_page, Qt::ScrollEnd, 1300, Qt::MouseEventSynthesizedBySystem);
    QVERIFY(!m_glide->gliding());
    QTest::qWait(100);
    QCOMPARE(m_page->heard.size(), fingerEvents + 2);
    QCOMPARE(ends(m_page->heard), 2);
}

// A wheel has no phases, however fast its notches come. The wait outlasts the 120 ms after which
// `SpaceSwipe` takes a gesture without phases to have ended.
void QtPageGlideTest::mouseWheelScrollsAsBefore()
{
    for (quint64 at = 1000; at <= 1100; at += 10) {
        wheel(m_page, Qt::NoScrollPhase, QPoint(0, 120), at, Qt::MouseEventNotSynthesized);
    }
    QVERIFY(!m_glide->gliding());
    QTest::qWait(300);
    QCOMPARE(m_page->heard.size(), 11);
    for (const auto &notch : std::as_const(m_page->heard)) {
        QCOMPARE(notch.phase, Qt::NoScrollPhase);
        QCOMPARE(notch.angle, QPoint(0, 120));
    }
}

// The Space swipe and the tab list read their own gestures, momentum and all.
void QtPageGlideTest::gestureOverAnotherItemDoesNotGlide()
{
    probe::OtherItem sidebar;
    flick(&sidebar);
    QVERIFY(!m_glide->gliding());
    QCOMPARE(sidebar.heard.size(), fingerEvents + 2);
    QCOMPARE(sidebar.heard.last().phase, Qt::ScrollEnd);
    QTest::qWait(100);
    QCOMPARE(sidebar.heard.size(), fingerEvents + 2);
}

// Fingers that move again begin a gesture of their own, which Chromium hears after the glide's
// has ended.
void QtPageGlideTest::touchingAgainStopsTheGlide()
{
    QVERIFY(startGlide());
    edge(m_page, Qt::ScrollBegin, 5000, Qt::MouseEventNotSynthesized);
    QVERIFY(!m_glide->gliding());
    QCOMPARE(m_page->heard.last().phase, Qt::ScrollBegin);
    QCOMPARE(m_page->heard.at(m_page->heard.size() - 2).phase, Qt::ScrollBegin);
    QCOMPARE(m_page->heard.at(m_page->heard.size() - 3).phase, Qt::ScrollEnd);
    const auto heard = m_page->heard.size();
    QTest::qWait(100);
    QCOMPARE(m_page->heard.size(), heard);
}

void QtPageGlideTest::inputStopsTheGlide_data()
{
    QTest::addColumn<int>("type");
    QTest::newRow("press") << int(QEvent::MouseButtonPress);
    QTest::newRow("key") << int(QEvent::KeyPress);
    QTest::newRow("shortcut") << int(QEvent::ShortcutOverride);
    QTest::newRow("window left") << int(QEvent::WindowDeactivate);
}

// Wherever it lands: a click in the page, a key in the address field.
void QtPageGlideTest::inputStopsTheGlide()
{
    QFETCH(int, type);
    QVERIFY(startGlide());
    QObject elsewhere;
    const auto eventType = static_cast<QEvent::Type>(type);
    if (eventType == QEvent::MouseButtonPress) {
        QMouseEvent press(eventType, QPointF(10, 10), QPointF(10, 10), Qt::LeftButton,
            Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&elsewhere, &press);
    } else if (eventType == QEvent::WindowDeactivate) {
        QEvent left(eventType);
        QCoreApplication::sendEvent(&elsewhere, &left);
    } else {
        QKeyEvent key(eventType, Qt::Key_Down, Qt::NoModifier);
        QCoreApplication::sendEvent(&elsewhere, &key);
    }
    QVERIFY(endedOnce());
}

// What Omaweb hears from the touchpad's hold gesture.
void QtPageGlideTest::restingFingersStopTheGlide()
{
    QVERIFY(startGlide());
    m_glide->stop();
    QVERIFY(endedOnce());
}

// A load in another view leaves the glide alone.
void QtPageGlideTest::pageLoadStopsTheGlide()
{
    QVERIFY(startGlide());
    QQuickItem otherView;
    m_glide->stopIn(&otherView);
    QVERIFY(m_glide->gliding());
    m_glide->stopIn(m_view.get());
    QVERIFY(endedOnce());
}

void QtPageGlideTest::hiddenPageStopsTheGlide_data()
{
    QTest::addColumn<bool>("agentTab");
    QTest::newRow("tab") << false;
    QTest::newRow("Agent tab") << true;
}

// A tab or Space switch hides the view the page is drawn in. An Agent's tab stays visible to
// the engine and is drawn at no opacity instead.
void QtPageGlideTest::hiddenPageStopsTheGlide()
{
    QFETCH(bool, agentTab);
    QVERIFY(startGlide());
    if (agentTab) {
        m_view->setOpacity(0);
    } else {
        m_view->setVisible(false);
    }
    QTRY_VERIFY_WITH_TIMEOUT(!m_glide->gliding(), 500);
    QVERIFY(endedOnce());
}

// The fingers' 1000 pixels a second carry the view a quarter of that further, 250 pixels, less
// the last one.
void QtPageGlideTest::chromeViewGlidesInPixels()
{
    ChromeView chrome;
    QVERIFY2(chrome.view, qPrintable(chrome.component.errorString()));
    flickPixels(chrome.view.get());
    const qreal lifted = chrome.contentY();
    QVERIFY(lifted > 0);
    QVERIFY(m_glide->gliding());
    QTRY_VERIFY_WITH_TIMEOUT(!m_glide->gliding(), 4000);
    const qreal glided = chrome.contentY() - lifted;
    QVERIFY2(glided >= 248 && glided <= 250, qPrintable(QString::number(glided)));
}

// Fingers that push a view against its end move nothing, so nothing glides.
void QtPageGlideTest::chromeViewHeldAtItsEndDoesNotGlide()
{
    ChromeView chrome(900);
    QVERIFY2(chrome.view, qPrintable(chrome.component.errorString()));
    flickPixels(chrome.view.get());
    QVERIFY(!m_glide->gliding());
}

// The fingers take the view to about 800, and the glide would take it 250 further, but it stops
// with the step that reaches the end at 900. The fingers may drag this view past its ends, and the
// glide would have held it there until it ran out.
void QtPageGlideTest::chromeViewGlideStopsAtItsEnd()
{
    ChromeView chrome(700);
    QVERIFY2(chrome.view, qPrintable(chrome.component.errorString()));
    flickPixels(chrome.view.get());
    QVERIFY(chrome.contentY() < 900);
    QVERIFY(m_glide->gliding());
    QTRY_VERIFY_WITH_TIMEOUT(chrome.contentY() >= 900, 2000);
    QVERIFY(!m_glide->gliding());
}

// Fingers never move straight down. A list that scrolls only up and down is at both its ends
// across, and that is no end of the way it scrolls.
void QtPageGlideTest::slantedSwipeGlidesAListThatScrollsOneWay()
{
    ChromeView chrome;
    QVERIFY2(chrome.view, qPrintable(chrome.component.errorString()));
    flickPixels(chrome.view.get(), QPoint(-2, -10));
    const qreal lifted = chrome.contentY();
    QVERIFY(m_glide->gliding());
    QTest::qWait(100);
    QVERIFY(m_glide->gliding());
    QVERIFY(chrome.contentY() > lifted);
}

QTEST_MAIN(QtPageGlideTest)
#include "tst_qtpageglide.moc"
