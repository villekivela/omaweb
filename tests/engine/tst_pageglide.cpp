#include "PageGlide.h"

#include <QTest>

#include <cmath>

class PageGlideTest final : public QObject {
    Q_OBJECT

private slots:
    void speedIsTheMovementSinceTheFirstSampleInTheWindow();
    void fingersAtRestBeforeTheyLiftHaveNoSpeed();
    void glideCoversTheSpeedOverTheFriction();
    void glideEndsWithUnderAPixelToGo();
};

namespace {

// Thirty units down every 10 ms from 1000 to 1100 ms: 3000 units a second.
omaweb::GlideSpeed steadyFlick()
{
    omaweb::GlideSpeed speed;
    for (quint64 at = 1000; at <= 1100; at += 10) {
        speed.add(at, QPointF(0, 30));
    }
    return speed;
}

} // namespace

void PageGlideTest::speedIsTheMovementSinceTheFirstSampleInTheWindow()
{
    QCOMPARE(steadyFlick().at(1100), QPointF(0, 3000));
}

// Lifted 100 ms after the last movement, the window holds 1050 to 1100: 150 units in 150 ms.
// Lifted 200 ms after it, the window holds nothing.
void PageGlideTest::fingersAtRestBeforeTheyLiftHaveNoSpeed()
{
    QCOMPARE(steadyFlick().at(1200), QPointF(0, 1000));
    QCOMPARE(steadyFlick().at(1300), QPointF(0, 0));
}

// GTK's friction of 4 a second takes a glide from 400 units a second 100 units in all, half of
// them in ln 2 / 4 seconds.
void PageGlideTest::glideCoversTheSpeedOverTheFriction()
{
    const omaweb::GlideCurve curve(QPointF(0, 400));
    QCOMPARE(curve.travelled(0), QPointF(0, 0));
    QCOMPARE(curve.travelled(std::log(2.0) / 4), QPointF(0, 50));
    QCOMPARE(curve.travelled(curve.duration()), QPointF(0, 98));
}

// With 100 units to go, two are left after ln 50 / 4 seconds. A speed that would carry the page
// less than a pixel does not glide.
void PageGlideTest::glideEndsWithUnderAPixelToGo()
{
    QVERIFY(omaweb::GlideCurve(QPointF(0, 400)).glides());
    QCOMPARE(omaweb::GlideCurve(QPointF(0, 400)).duration(), std::log(50.0) / 4);
    QCOMPARE(omaweb::GlideCurve(QPointF(-240, 320)).duration(), std::log(50.0) / 4);
    QVERIFY(!omaweb::GlideCurve(QPointF(0, 6)).glides());
    QCOMPARE(omaweb::GlideCurve(QPointF(0, 6)).duration(), 0.0);
}

QTEST_GUILESS_MAIN(PageGlideTest)
#include "tst_pageglide.moc"
