#include "PageImages.h"

#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using omaweb::PageImages;

namespace {

// A strip as a view leaves it: an image written to a file of its own.
QString strip(int width, int height, const QColor &colour, qreal ratio = 1)
{
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(colour);
    image.setDevicePixelRatio(ratio);
    const auto path = PageImages().reserveStrip();
    image.save(path, "PNG");
    return path;
}

} // namespace

class PageImagesTest final : public QObject {
    Q_OBJECT

private slots:
    void joinsStripsAtTheTopsThePageWasScrolledTo();
    void readsTheScaleOffTheFirstStrip();
    void refusesAPageTallerThanTheLimitAndLeavesNoFile();
    void removesTheStripsItWasGiven();
    void refusesStripsItCannotPlace();
};

// Three screenfuls of a page 250 tall in a viewport of 100: the last one is
// scrolled to the bottom and overlaps the second, and is drawn over it.
void PageImagesTest::joinsStripsAtTheTopsThePageWasScrolledTo()
{
    QTemporaryDir root;
    const auto path = root.filePath(QStringLiteral("page.png"));
    PageImages images;
    QVERIFY(
        images.join({strip(40, 100, Qt::red), strip(40, 100, Qt::green), strip(40, 100, Qt::blue)},
            {0, 100, 150}, 250, 100, path));

    const QImage page(path);
    QCOMPARE(page.size(), QSize(40, 250));
    QCOMPARE(page.pixelColor(20, 50), QColor(Qt::red));
    QCOMPARE(page.pixelColor(20, 120), QColor(Qt::green));
    QCOMPARE(page.pixelColor(20, 160), QColor(Qt::blue));
    QCOMPARE(page.pixelColor(20, 249), QColor(Qt::blue));
}

// A grab at twice the density is twice as many pixels for each of the page's,
// and the tops are the page's own.
void PageImagesTest::readsTheScaleOffTheFirstStrip()
{
    QTemporaryDir root;
    const auto path = root.filePath(QStringLiteral("page.png"));
    PageImages images;
    QVERIFY(images.join(
        {strip(80, 200, Qt::red, 2), strip(80, 200, Qt::blue, 2)}, {0, 50}, 150, 100, path));

    const QImage page(path);
    QCOMPARE(page.size(), QSize(80, 300));
    QCOMPARE(page.pixelColor(10, 90), QColor(Qt::red));
    QCOMPARE(page.pixelColor(10, 110), QColor(Qt::blue));
}

void PageImagesTest::refusesAPageTallerThanTheLimitAndLeavesNoFile()
{
    QTemporaryDir root;
    const auto path = root.filePath(QStringLiteral("page.png"));
    PageImages images;
    QVERIFY(!images.join({strip(10, 100, Qt::red)}, {0}, PageImages::kHeightLimit + 1, 100, path));
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(images.join({strip(10, 100, Qt::red)}, {0}, PageImages::kHeightLimit, 100, path));
}

// The strips are the join's to clear away, whatever it answers.
void PageImagesTest::removesTheStripsItWasGiven()
{
    QTemporaryDir root;
    PageImages images;
    const auto kept = strip(10, 100, Qt::red);
    QVERIFY(images.join({kept}, {0}, 100, 100, root.filePath(QStringLiteral("page.png"))));
    QVERIFY(!QFileInfo::exists(kept));
    const auto refused = strip(10, 100, Qt::red);
    QVERIFY(!images.join({refused}, {}, 0, 0, QString()));
    QVERIFY(!QFileInfo::exists(refused));
}

void PageImagesTest::refusesStripsItCannotPlace()
{
    QTemporaryDir root;
    const auto path = root.filePath(QStringLiteral("page.png"));
    PageImages images;
    QVERIFY(!images.join({}, {}, 100, 100, path));
    QVERIFY(!images.join({strip(10, 100, Qt::red)}, {0, 50}, 100, 100, path));
    QVERIFY(!images.join({strip(10, 100, Qt::red)}, {0}, 100, 0, path));
    QVERIFY(!images.join({root.filePath(QStringLiteral("missing.png"))}, {0}, 100, 100, path));
    QVERIFY(!QFileInfo::exists(path));
}

QTEST_GUILESS_MAIN(PageImagesTest)
#include "tst_pageimages.moc"
