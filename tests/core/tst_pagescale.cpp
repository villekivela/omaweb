#include "PageScale.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::PageScale;

class PageScaleTest final : public QObject {
    Q_OBJECT

private slots:
    void followsTheDesktopUntilTold();
    void setValueStandsOverTheDesktop();
    void takesTheNearestStepWithinTheRange();
    void movesOntoTheStepsFromTheDesktopsValue();
    void resetFollowsTheDesktopAgain();
    void keepsTheValueAcrossARestart();
    void followsAValueWrittenInTheFile();
    void readsAValueOutOfRangeAsNone();
    void drawsWithinTheEnginesRange();
};

namespace {

void writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(contents);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
}

} // namespace

// Omarchy's 16 px text size writes 1.3636 as the desktop's factor, and pages
// come up at that size with nothing written for it.
void PageScaleTest::followsTheDesktopUntilTold()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    PageScale scale(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&scale, &PageScale::changed);
    QCOMPARE(scale.value(), 1.0);
    QVERIFY(!scale.overridden());

    scale.setDesktopFactor(1.3636);

    QCOMPARE(scale.value(), 1.3636);
    QCOMPARE(scale.desktopFactor(), 1.3636);
    QCOMPARE(changed.count(), 1);
    scale.setDesktopFactor(1.3636);
    QCOMPARE(changed.count(), 1);
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/settings.json"))));
}

void PageScaleTest::setValueStandsOverTheDesktop()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    PageScale scale(root.path());
    scale.setDesktopFactor(1.3636);
    QSignalSpy changed(&scale, &PageScale::changed);

    scale.set(1.5);
    QCOMPARE(scale.value(), 1.5);
    QVERIFY(scale.overridden());
    QCOMPARE(changed.count(), 1);
    QVERIFY(readFile(path).contains("\"page-scale\": 1.5\n"));

    scale.setDesktopFactor(1.0);
    QCOMPARE(scale.value(), 1.5);
    QCOMPARE(changed.count(), 1);
}

// Settings steps by 10%, and a value between steps, as a float sum leaves, is
// the step it is nearest. The ends are ends.
void PageScaleTest::takesTheNearestStepWithinTheRange()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    PageScale scale(root.path());

    scale.set(0.1 + 0.6 + 0.6);
    QCOMPARE(scale.value(), 1.3);
    QVERIFY(readFile(path).contains("\"page-scale\": 1.3\n"));
    scale.set(0.2);
    QCOMPARE(scale.value(), 0.5);
    scale.set(3);
    QCOMPARE(scale.value(), 2.0);
}

// The desktop's value falls between steps, and the first step from it lands
// on one rather than keeping its fraction.
void PageScaleTest::movesOntoTheStepsFromTheDesktopsValue()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    PageScale scale(root.path());
    scale.setDesktopFactor(1.3636);

    scale.increase();
    QCOMPARE(scale.value(), 1.4);
    scale.increase();
    QCOMPARE(scale.value(), 1.5);

    scale.reset();
    scale.decrease();
    QCOMPARE(scale.value(), 1.3);

    scale.set(2.0);
    scale.increase();
    QCOMPARE(scale.value(), 2.0);
    scale.set(0.5);
    scale.decrease();
    QCOMPARE(scale.value(), 0.5);

    // A desktop beyond the range is the range's end one step down, and a
    // step up from it is no step: it never makes the page smaller.
    scale.reset();
    scale.setDesktopFactor(2.5);
    scale.increase();
    QCOMPARE(scale.value(), 2.5);
    QVERIFY(!scale.overridden());
    scale.decrease();
    QCOMPARE(scale.value(), 2.0);
    scale.reset();
    scale.setDesktopFactor(0.4);
    scale.decrease();
    QCOMPARE(scale.value(), 0.4);
    QVERIFY(!scale.overridden());
}

void PageScaleTest::resetFollowsTheDesktopAgain()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    PageScale scale(root.path());
    scale.setDesktopFactor(1.25);
    scale.set(0.8);
    QSignalSpy changed(&scale, &PageScale::changed);

    scale.reset();

    QVERIFY(!scale.overridden());
    QCOMPARE(scale.value(), 1.25);
    QCOMPARE(changed.count(), 1);
    QVERIFY(!readFile(path).contains("page-scale"));
    scale.reset();
    QCOMPARE(changed.count(), 1);
    scale.setDesktopFactor(1.5);
    QCOMPARE(scale.value(), 1.5);
}

void PageScaleTest::keepsTheValueAcrossARestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    {
        PageScale scale(root.path());
        scale.set(1.2);
    }
    const PageScale restarted(root.path());
    QCOMPARE(restarted.value(), 1.2);
    QVERIFY(restarted.overridden());
}

void PageScaleTest::followsAValueWrittenInTheFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    PageScale scale(root.path());
    scale.setDesktopFactor(1.1);
    QSignalSpy changed(&scale, &PageScale::changed);

    writeFile(path, R"({"version": 1, "page-scale": 1.7})");
    QTRY_COMPARE(scale.value(), 1.7);
    QCOMPARE(changed.count(), 1);

    writeFile(path, R"({"version": 1})");
    QTRY_VERIFY(!scale.overridden());
    QCOMPARE(scale.value(), 1.1);
    QCOMPARE(changed.count(), 2);
}

void PageScaleTest::readsAValueOutOfRangeAsNone()
{
    for (const auto *bad : {"0.4", "2.5", "\"large\""}) {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        writeFile(root.filePath(QStringLiteral("settings.json")),
            R"({"version": 1, "page-scale": )" + QByteArray(bad) + "}");
        const PageScale scale(root.path());
        QVERIFY2(!scale.overridden(), bad);
        QCOMPARE(scale.value(), 1.0);
    }
}

// QtWebEngine draws a page between 25 and 500 percent, so the product of the
// Page scale and a zoom is held there.
void PageScaleTest::drawsWithinTheEnginesRange()
{
    QCOMPARE(PageScale::drawn(1.5, 1.1), 1.5 * 1.1);
    QCOMPARE(PageScale::drawn(2.0, 3.0), 5.0);
    QCOMPARE(PageScale::drawn(0.5, 0.25), 0.25);
    QCOMPARE(PageScale::drawn(1.0, 1.0), 1.0);
}

QTEST_MAIN(PageScaleTest)
#include "tst_pagescale.moc"
