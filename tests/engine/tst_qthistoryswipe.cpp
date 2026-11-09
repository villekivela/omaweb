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

class QtHistorySwipeTest final : public QObject {
    Q_OBJECT
private slots:
    void init();
    void direction_data();
    void direction();
    void belowThresholdAndReversal();
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
    void wheel(
        Qt::ScrollPhase phase, QPoint delta = {}, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void begin();
    omaweb::QtHistorySwipe swipe;
    QQuickWindow window;
    QQuickItem view;
    probe::RenderWidgetHostViewQtDelegateItem page;
};

void QtHistorySwipeTest::wheel(Qt::ScrollPhase phase, QPoint delta, Qt::KeyboardModifiers modifiers)
{
    static quint64 time = 0;
    QWheelEvent event(QPointF(40, 40), QPointF(140, 140), delta, delta * 12, Qt::NoButton,
        modifiers, phase, false, Qt::MouseEventSynthesizedBySystem);
    event.setTimestamp(++time);
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
