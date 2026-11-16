#include "QtHistorySwipe.h"
#include "QtPageInput.h"

#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>

namespace probe {
class RenderWidgetHostViewQtDelegateItem final : public QQuickItem {
    Q_OBJECT
public:
    QList<Qt::ScrollPhase> heard;

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        heard.append(event->phase());
        event->accept();
    }
};
} // namespace probe

class CountingDispatcher : public omaweb::QtHistorySwipeDispatcher {
public:
    int unrelatedEvents = 0;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::User)
            ++unrelatedEvents;
        return omaweb::QtHistorySwipeDispatcher::eventFilter(watched, event);
    }
};

class CountingSwipe : public omaweb::QtHistorySwipe {
public:
    int unrelatedEvents = 0;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::User)
            ++unrelatedEvents;
        return omaweb::QtHistorySwipe::eventFilter(watched, event);
    }
};

class QtHistorySwipeTest final : public QObject {
    Q_OBJECT
private slots:
    void init();
    void sharedDispatch();
    void multiViewLifecycle();
    void cancelActiveViews();
    void cancelSettledBeforeDuplicateBegin_data();
    void cancelSettledBeforeDuplicateBegin();
    void axisLock();
    void verticalAxisLock();
    void diagonalAxisLock();
    void flick_data();
    void flick();
    void flickSamplesReset();
    void direction_data();
    void direction();
    void belowThresholdAndReversal();
    void readersDistance();
    void verticalModifiersAndWheel();
    void modifiedInputPasses();
    void plainWheelPasses();
    void unavailable();
    void rejectedAndTimeoutReplay();
    void timeoutReplays();
    void asynchronousEndAndMomentum();
    void momentumCannotCompleteGesture();
    void syntheticAndDisabled();
    void modifierDuringQuery();
    void angleOnlyAndDiagonal();
    void missingEnd();
    void hideAndStaleQuery();
    void destructionCancels_data();
    void destructionCancels();

private:
    void wheel(Qt::ScrollPhase phase, QPoint delta = {},
        Qt::KeyboardModifiers modifiers = Qt::NoModifier, qint64 elapsed = 200);
    void begin();
    CountingDispatcher dispatcher;
    omaweb::QtHistorySwipe swipe;
    QQuickWindow window;
    QQuickItem view;
    probe::RenderWidgetHostViewQtDelegateItem page;
};

void QtHistorySwipeTest::wheel(
    Qt::ScrollPhase phase, QPoint delta, Qt::KeyboardModifiers modifiers, qint64 elapsed)
{
    static quint64 time = 0;
    QWheelEvent event(QPointF(40, 40), QPointF(140, 140), delta, delta * 12, Qt::NoButton,
        modifiers, phase, false, Qt::MouseEventSynthesizedBySystem);
    time += elapsed;
    event.setTimestamp(time);
    QCoreApplication::sendEvent(&page, &event);
}

void QtHistorySwipeTest::begin() { wheel(Qt::ScrollBegin); }

void QtHistorySwipeTest::init()
{
    swipe.cancel();
    view.setParentItem(window.contentItem());
    window.show();
    view.setVisible(true);
    page.setParentItem(&view);
    page.setVisible(true);
    QVERIFY(view.isVisible());
    QVERIFY(page.isVisible());
    page.heard.clear();
    swipe.setView(&view);
    swipe.setProperty("enabled", true);
    swipe.setProperty("canGoBack", true);
    swipe.setProperty("canGoForward", true);
    swipe.setProperty("distance", 120);
}

void QtHistorySwipeTest::sharedDispatch()
{
    QQuickItem firstView(window.contentItem());
    QQuickItem otherView(window.contentItem());
    probe::RenderWidgetHostViewQtDelegateItem otherPage;
    otherPage.setParentItem(&otherView);
    CountingSwipe first;
    CountingSwipe second;
    std::vector<std::unique_ptr<QQuickItem>> extraViews;
    std::vector<std::unique_ptr<CountingSwipe>> extras;
    for (int i = 0; i < 30; ++i) {
        extraViews.push_back(std::make_unique<QQuickItem>(window.contentItem()));
        extras.push_back(std::make_unique<CountingSwipe>());
        extras.back()->setView(extraViews.back().get());
    }
    first.setView(&firstView);
    second.setView(&otherView);
    first.setProperty("canGoBack", true);
    second.setProperty("canGoBack", true);
    QSignalSpy firstQuery(&first, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy secondQuery(&second, &omaweb::QtHistorySwipe::queryNeeded);
    const auto send = [](QQuickItem *target, Qt::ScrollPhase phase, QPoint delta) {
        QWheelEvent event(QPointF(40, 40), QPointF(140, 140), delta, delta * 12, Qt::NoButton,
            Qt::NoModifier, phase, false, Qt::MouseEventSynthesizedBySystem);
        QCoreApplication::sendEvent(target, &event);
    };
    send(&otherPage, Qt::ScrollBegin, {});
    send(&otherPage, Qt::ScrollUpdate, {20, 0});
    QCOMPARE(firstQuery.size(), 0);
    QCOMPARE(secondQuery.size(), 1);
    second.resolve(secondQuery.first().first().toInt(), false);
    QObject unrelated;
    dispatcher.unrelatedEvents = 0;
    QEvent event(QEvent::User);
    QCoreApplication::sendEvent(&unrelated, &event);
    QCOMPARE(dispatcher.unrelatedEvents, 1);
    QCOMPARE(first.unrelatedEvents, 0);
    QCOMPARE(second.unrelatedEvents, 0);
    for (const auto &extra : extras)
        QCOMPARE(extra->unrelatedEvents, 0);
}

void QtHistorySwipeTest::multiViewLifecycle()
{
    QQuickItem firstView(window.contentItem());
    QQuickItem secondView(&firstView);
    QQuickItem reboundView(window.contentItem());
    probe::RenderWidgetHostViewQtDelegateItem firstPage;
    probe::RenderWidgetHostViewQtDelegateItem secondPage;
    probe::RenderWidgetHostViewQtDelegateItem reboundPage;
    firstPage.setParentItem(&firstView);
    secondPage.setParentItem(&secondView);
    reboundPage.setParentItem(&reboundView);
    auto first = std::make_unique<omaweb::QtHistorySwipe>();
    omaweb::QtHistorySwipe second;
    first->setView(&firstView);
    second.setView(&secondView);
    first->setProperty("canGoBack", true);
    second.setProperty("canGoBack", true);
    QSignalSpy firstQuery(first.get(), &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy secondQuery(&second, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy secondNavigation(&second, &omaweb::QtHistorySwipe::navigate);
    quint64 timestamp = 100;
    const auto send = [&timestamp](QQuickItem *target, Qt::ScrollPhase phase, QPoint delta) {
        QWheelEvent event(QPointF(40, 40), QPointF(140, 140), delta, delta * 12, Qt::NoButton,
            Qt::NoModifier, phase, false, Qt::MouseEventSynthesizedBySystem);
        event.setTimestamp(timestamp += 200);
        QCoreApplication::sendEvent(target, &event);
    };
    send(&firstPage, Qt::ScrollBegin, {});
    send(&firstPage, Qt::ScrollUpdate, {130, 0});
    QCOMPARE(firstQuery.size(), 1);
    send(&secondPage, Qt::ScrollBegin, {});
    send(&secondPage, Qt::ScrollUpdate, {130, 0});
    QCOMPARE(firstQuery.size(), 1);
    QCOMPARE(secondQuery.size(), 1);
    second.resolve(secondQuery.first().first().toInt(), true);
    QVERIFY(second.progress() > 0);
    QCOMPARE(first->progress(), 0);
    send(&secondPage, Qt::ScrollEnd, {});
    QCOMPARE(secondNavigation.size(), 1);
    const int stale = firstQuery.first().first().toInt();
    first->setView(&reboundView);
    first->resolve(stale, true);
    QCOMPARE(first->progress(), 0);
    send(&firstPage, Qt::ScrollBegin, {});
    send(&firstPage, Qt::ScrollUpdate, {130, 0});
    QCOMPARE(firstQuery.size(), 1);
    send(&reboundPage, Qt::ScrollBegin, {});
    send(&reboundPage, Qt::ScrollUpdate, {130, 0});
    QCOMPARE(firstQuery.size(), 2);
    first.reset();
    send(&reboundPage, Qt::ScrollBegin, {});
    send(&reboundPage, Qt::ScrollUpdate, {130, 0});
    QCOMPARE(secondQuery.size(), 1);
}

void QtHistorySwipeTest::cancelActiveViews()
{
    QQuickItem otherView(window.contentItem());
    probe::RenderWidgetHostViewQtDelegateItem otherPage;
    otherPage.setParentItem(&otherView);
    omaweb::QtHistorySwipe other;
    other.setView(&otherView);
    other.setProperty("canGoBack", true);
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy otherQuery(&other, &omaweb::QtHistorySwipe::queryNeeded);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    QWheelEvent beginOther(QPointF(40, 40), QPointF(140, 140), {}, {}, Qt::NoButton, Qt::NoModifier,
        Qt::ScrollBegin, false, Qt::MouseEventSynthesizedBySystem);
    QWheelEvent moveOther(QPointF(40, 40), QPointF(140, 140), {130, 0}, {1560, 0}, Qt::NoButton,
        Qt::NoModifier, Qt::ScrollUpdate, false, Qt::MouseEventSynthesizedBySystem);
    QCoreApplication::sendEvent(&otherPage, &beginOther);
    QCoreApplication::sendEvent(&otherPage, &moveOther);
    QCOMPARE(query.size(), 1);
    QCOMPARE(otherQuery.size(), 1);
    QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &key);
    swipe.resolve(query.first().first().toInt(), true);
    other.resolve(otherQuery.first().first().toInt(), true);
    QCOMPARE(swipe.progress(), 0);
    QCOMPARE(other.progress(), 0);
    QCOMPARE(page.heard.size(), 2);
    QCOMPARE(otherPage.heard.size(), 2);
}

void QtHistorySwipeTest::cancelSettledBeforeDuplicateBegin_data()
{
    QTest::addColumn<bool>("settled");
    QTest::newRow("settled") << true;
    QTest::newRow("passing") << false;
}

void QtHistorySwipeTest::cancelSettledBeforeDuplicateBegin()
{
    QFETCH(bool, settled);
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    const auto send = [this](Qt::ScrollPhase phase, QPoint delta) {
        QWheelEvent event(QPointF(40, 40), QPointF(140, 140), delta, delta * 12, Qt::NoButton,
            Qt::NoModifier, phase, false, Qt::MouseEventSynthesizedBySystem);
        event.setTimestamp(123);
        QCoreApplication::sendEvent(&page, &event);
    };
    send(Qt::ScrollBegin, {});
    if (settled) {
        send(Qt::ScrollUpdate, {130, 0});
        QCOMPARE(query.size(), 1);
        swipe.resolve(query.first().first().toInt(), true);
        send(Qt::ScrollEnd, {});
    } else {
        send(Qt::ScrollUpdate, {0, 6});
        QCOMPARE(query.size(), 0);
    }
    QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &key);
    send(Qt::ScrollBegin, {});
    send(Qt::ScrollUpdate, {130, 0});
    QCOMPARE(query.size(), settled ? 2 : 1);
}

void QtHistorySwipeTest::axisLock()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    begin();
    wheel(Qt::ScrollUpdate, {5, 0});
    QCOMPARE(query.size(), 0);
    wheel(Qt::ScrollUpdate, {1, 0});
    QCOMPARE(query.size(), 1);
}

void QtHistorySwipeTest::verticalAxisLock()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    begin();
    wheel(Qt::ScrollUpdate, {6, 6});
    wheel(Qt::ScrollUpdate, {130, 0});
    QCOMPARE(query.size(), 0);
}

void QtHistorySwipeTest::diagonalAxisLock()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    begin();
    wheel(Qt::ScrollUpdate, {5, 5});
    QCOMPARE(query.size(), 0);
    wheel(Qt::ScrollUpdate, {1, 0});
    QCOMPARE(query.size(), 1);
    swipe.resolve(query.first().first().toInt(), true);
    wheel(Qt::ScrollUpdate, {0, 10});
    QVERIFY(swipe.progress() > 0);
}

void QtHistorySwipeTest::flick_data()
{
    QTest::addColumn<int>("sign");
    QTest::addColumn<int>("secondDelta");
    QTest::addColumn<qint64>("elapsed");
    QTest::addColumn<quint64>("releaseDelay");
    QTest::addColumn<bool>("commits");
    QTest::newRow("fast back") << 1 << 21 << qint64(20) << quint64(1) << true;
    QTest::newRow("fast forward") << -1 << 21 << qint64(20) << quint64(1) << true;
    QTest::newRow("exact speed") << 1 << 20 << qint64(20) << quint64(1) << false;
    QTest::newRow("slow") << 1 << 19 << qint64(20) << quint64(1) << false;
    QTest::newRow("rest before release") << 1 << 21 << qint64(20) << quint64(101) << false;
    QTest::newRow("reverse") << 1 << -21 << qint64(20) << quint64(1) << false;
    QTest::newRow("same timestamp") << 1 << 21 << qint64(0) << quint64(1) << false;
    QTest::newRow("backward timestamp") << 1 << 21 << qint64(-1) << quint64(2) << false;
}

void QtHistorySwipeTest::flick()
{
    QFETCH(int, sign);
    QFETCH(int, secondDelta);
    QFETCH(qint64, elapsed);
    QFETCH(quint64, releaseDelay);
    QFETCH(bool, commits);
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    begin();
    wheel(Qt::ScrollUpdate, {30 * sign, 0});
    QCOMPARE(query.size(), 1);
    swipe.resolve(query.first().first().toInt(), true);
    wheel(Qt::ScrollUpdate, {secondDelta * sign, 0}, Qt::NoModifier, elapsed);
    wheel(Qt::ScrollEnd, {}, Qt::NoModifier, releaseDelay);
    QCOMPARE(navigation.size(), commits ? 1 : 0);
    if (commits)
        QCOMPARE(navigation.first().first().toInt(), -sign);
}

void QtHistorySwipeTest::flickSamplesReset()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    begin();
    wheel(Qt::ScrollUpdate, {10, 0});
    swipe.resolve(query.first().first().toInt(), true);
    wheel(Qt::ScrollEnd, {}, Qt::NoModifier, 1);
    wheel(Qt::ScrollBegin, {}, Qt::NoModifier, 1);
    wheel(Qt::ScrollUpdate, {40, 0}, Qt::NoModifier, 1);
    QCOMPARE(query.size(), 2);
    swipe.resolve(query.last().first().toInt(), true);
    wheel(Qt::ScrollEnd, {}, Qt::NoModifier, 1);
    QCOMPARE(navigation.size(), 0);
}

void QtHistorySwipeTest::direction_data()
{
    QTest::addColumn<int>("delta");
    QTest::addColumn<int>("direction");
    QTest::newRow("back") << 130 << -1;
    QTest::newRow("forward") << -130 << 1;
}

void QtHistorySwipeTest::direction()
{
    QFETCH(int, delta);
    QFETCH(int, direction);
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    begin();
    wheel(Qt::ScrollUpdate, {delta, 0});
    QCOMPARE(query.size(), 1);
    QCOMPARE(query.first().at(2).toInt(), direction);
    swipe.resolve(query.first().at(0).toInt(), true);
    wheel(Qt::ScrollEnd);
    QCOMPARE(navigation.size(), 1);
    QCOMPARE(navigation.first().first().toInt(), direction);
}

void QtHistorySwipeTest::belowThresholdAndReversal()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    begin();
    wheel(Qt::ScrollUpdate, {100, 0});
    swipe.resolve(query.first().first().toInt(), true);
    wheel(Qt::ScrollUpdate, {-60, 0});
    wheel(Qt::ScrollEnd);
    QCOMPARE(navigation.size(), 0);
}

// The distance is the reader's, from Settings: a slow swipe goes at that far
// and not a pixel short of it.
void QtHistorySwipeTest::readersDistance()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    const auto swipeBy = [&](int pixels) {
        begin();
        wheel(Qt::ScrollUpdate, {pixels, 0});
        swipe.resolve(query.last().first().toInt(), true);
        wheel(Qt::ScrollEnd);
    };

    swipe.setProperty("distance", 60);
    swipeBy(59);
    QCOMPARE(navigation.size(), 0);
    swipeBy(60);
    QCOMPARE(navigation.size(), 1);

    swipe.setProperty("distance", 200);
    swipeBy(150);
    QCOMPARE(navigation.size(), 1);
    swipeBy(200);
    QCOMPARE(navigation.size(), 2);
}

void QtHistorySwipeTest::verticalModifiersAndWheel()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    begin();
    wheel(Qt::ScrollUpdate, {20, 20});
    wheel(Qt::ScrollUpdate, {130, 0});
    wheel(Qt::ScrollEnd);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0}, Qt::ControlModifier);
    wheel(Qt::NoScrollPhase, {130, 0});
    QCOMPARE(query.size(), 0);
}

void QtHistorySwipeTest::modifiedInputPasses()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0}, Qt::ControlModifier);
    QCOMPARE(query.size(), 0);
    QCOMPARE(page.heard.size(), 2);
}

void QtHistorySwipeTest::plainWheelPasses()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    begin();
    wheel(Qt::NoScrollPhase, {130, 0});
    QCOMPARE(query.size(), 0);
    QCOMPARE(page.heard.size(), 2);
}

void QtHistorySwipeTest::unavailable()
{
    swipe.setProperty("canGoBack", false);
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    QCOMPARE(query.size(), 0);
    QCOMPARE(page.heard.size(), 2);
}

void QtHistorySwipeTest::rejectedAndTimeoutReplay()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    QCOMPARE(page.heard.size(), 1);
    swipe.resolve(query.first().first().toInt(), false);
    QCOMPARE(page.heard.size(), 2);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    QTRY_COMPARE_WITH_TIMEOUT(page.heard.size(), 4, 500);
}

void QtHistorySwipeTest::timeoutReplays()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    QCOMPARE(query.size(), 1);
    const int request = query.first().first().toInt();
    QTRY_COMPARE_WITH_TIMEOUT(page.heard.size(), 2, 500);
    swipe.resolve(request, true);
    wheel(Qt::ScrollEnd);
    QCOMPARE(navigation.size(), 0);
}

void QtHistorySwipeTest::asynchronousEndAndMomentum()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    wheel(Qt::ScrollEnd);
    swipe.resolve(query.first().first().toInt(), true);
    QCOMPARE(navigation.size(), 1);
    const auto heard = page.heard.size();
    wheel(Qt::ScrollEnd);
    wheel(Qt::ScrollMomentum, {130, 0});
    QCOMPARE(navigation.size(), 1);
    QCOMPARE(page.heard.size(), heard);
}

void QtHistorySwipeTest::momentumCannotCompleteGesture()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    begin();
    wheel(Qt::ScrollUpdate, {30, 0});
    QCOMPARE(query.size(), 1);
    swipe.resolve(query.first().first().toInt(), true);
    wheel(Qt::ScrollMomentum, {130, 0});
    wheel(Qt::ScrollEnd);
    QCOMPARE(navigation.size(), 0);
}

void QtHistorySwipeTest::syntheticAndDisabled()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    omaweb::pageGlideEvent = true;
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    omaweb::pageGlideEvent = false;
    swipe.setProperty("enabled", false);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    QCOMPARE(query.size(), 0);
    QCOMPARE(page.heard.size(), 4);
}

void QtHistorySwipeTest::hideAndStaleQuery()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    QCOMPARE(query.size(), 1);
    const int request = query.first().first().toInt();
    swipe.resolve(request, true);
    QVERIFY(swipe.progress() > 0);
    view.setVisible(false);
    QCOMPARE(swipe.progress(), 0);
    swipe.resolve(request, true);
    view.setVisible(true);
    wheel(Qt::ScrollEnd);
    QCOMPARE(navigation.size(), 0);
    QCOMPARE(swipe.progress(), 0);
}

void QtHistorySwipeTest::modifierDuringQuery()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    QCOMPARE(query.size(), 1);
    const int request = query.first().first().toInt();
    wheel(Qt::ScrollUpdate, {1, 0}, Qt::ControlModifier);
    QCOMPARE(page.heard.size(), 3);
    swipe.resolve(request, true);
    QCOMPARE(swipe.progress(), 0);
}

void QtHistorySwipeTest::angleOnlyAndDiagonal()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    begin();
    QWheelEvent event(QPointF(40, 40), QPointF(140, 140), {}, {1560, 300}, Qt::NoButton,
        Qt::NoModifier, Qt::ScrollUpdate, false, Qt::MouseEventSynthesizedBySystem);
    QCoreApplication::sendEvent(&page, &event);
    QCOMPARE(query.size(), 1);
    swipe.resolve(query.first().first().toInt(), true);
    wheel(Qt::ScrollEnd);
    QCOMPARE(navigation.size(), 1);
}

void QtHistorySwipeTest::missingEnd()
{
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    begin();
    wheel(Qt::ScrollUpdate, {130, 0});
    QCOMPARE(query.size(), 1);
    swipe.resolve(query.first().first().toInt(), true);
    QVERIFY(swipe.progress() > 0);
    QTRY_COMPARE_WITH_TIMEOUT(swipe.progress(), 0, 1800);
    QCOMPARE(navigation.size(), 0);
}

void QtHistorySwipeTest::destructionCancels_data()
{
    QTest::addColumn<bool>("destroyView");
    QTest::addColumn<bool>("pending");
    QTest::newRow("view active") << true << false;
    QTest::newRow("page active") << false << false;
    QTest::newRow("page query") << false << true;
}

void QtHistorySwipeTest::destructionCancels()
{
    QFETCH(bool, destroyView);
    QFETCH(bool, pending);
    auto watched = std::make_unique<QQuickItem>();
    auto input = std::make_unique<probe::RenderWidgetHostViewQtDelegateItem>();
    watched->setParentItem(window.contentItem());
    input->setParentItem(watched.get());
    swipe.setView(watched.get());
    QSignalSpy query(&swipe, &omaweb::QtHistorySwipe::queryNeeded);
    QSignalSpy navigation(&swipe, &omaweb::QtHistorySwipe::navigate);
    const auto send = [&](Qt::ScrollPhase phase, QPoint delta) {
        QWheelEvent event(QPointF(40, 40), QPointF(140, 140), delta, delta * 12, Qt::NoButton,
            Qt::NoModifier, phase, false, Qt::MouseEventSynthesizedBySystem);
        QCoreApplication::sendEvent(input.get(), &event);
    };
    send(Qt::ScrollBegin, {});
    send(Qt::ScrollUpdate, {130, 0});
    QCOMPARE(query.size(), 1);
    const int request = query.first().first().toInt();
    if (!pending) {
        swipe.resolve(request, true);
        QVERIFY(swipe.progress() > 0);
    }
    if (destroyView)
        watched.reset();
    else
        input.reset();
    QCOMPARE(swipe.progress(), 0);
    swipe.resolve(request, true);
    QCOMPARE(swipe.progress(), 0);
    if (input)
        send(Qt::ScrollEnd, {});
    QCOMPARE(navigation.size(), 0);
    swipe.setView(nullptr);
}

QTEST_MAIN(QtHistorySwipeTest)
#include "tst_qthistoryswipe.moc"
