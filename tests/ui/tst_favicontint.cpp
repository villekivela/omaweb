#include "FaviconTint.h"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>

using namespace tanto;

namespace {

QImage filled(const QColor &color, int size = 32)
{
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(color);
    return image;
}

// A favicon as most sites actually ship one: a plain plate with a small mark
// on it. The mark is the only thing that says which site this is.
QImage markOn(const QColor &plate, const QColor &mark)
{
    auto image = filled(plate);
    QPainter painter(&image);
    painter.fillRect(12, 12, 8, 8, mark);
    painter.end();
    return image;
}

// The shape a web engine's icon store takes on the QML engine: an image
// provider, reached by an `image://` URL rather than a path.
class SynchronousIconStore final : public QQuickImageProvider {
public:
    SynchronousIconStore()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &) override
    {
        const auto icon = filled(QColor(id));
        if (size) {
            *size = icon.size();
        }
        return icon;
    }
};

// QtWebEngine's own favicon provider answers asynchronously, so the chip has
// to survive being asked before the icon exists.
class DeferredIconResponse final : public QQuickImageResponse {
public:
    explicit DeferredIconResponse(const QColor &colour)
        : m_image(filled(colour))
    {
    }

    void respond() { emit finished(); }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

private:
    QImage m_image;
};

class AsynchronousIconStore final : public QQuickAsyncImageProvider {
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &) override
    {
        auto *response = new DeferredIconResponse(QColor(id));
        pending.append(response);
        return response;
    }

    QList<DeferredIconResponse *> pending;
};

} // namespace

class TestFaviconTint final : public QObject {
    Q_OBJECT

private slots:
    void readsTheHueOfASolidIcon_data()
    {
        QTest::addColumn<QColor>("color");
        QTest::newRow("red") << QColor(220, 40, 40);
        QTest::newRow("green") << QColor(40, 200, 60);
        QTest::newRow("blue") << QColor(50, 90, 230);
        QTest::newRow("magenta") << QColor(210, 60, 190);
    }

    void readsTheHueOfASolidIcon()
    {
        QFETCH(QColor, color);
        const auto hue = faviconHue(filled(color));
        QVERIFY(hue.has_value());
        QVERIFY(qAbs(*hue - color.hueF()) < 0.03);
    }

    void takesTheHueOfTheMarkNotThePlate()
    {
        // A white plate carries no hue, so a teal mark on it decides the chip
        // even though it covers a fraction of the icon.
        const QColor mark(20, 170, 180);
        const auto hue = faviconHue(markOn(Qt::white, mark));
        QVERIFY(hue.has_value());
        QVERIFY(qAbs(*hue - mark.hueF()) < 0.05);
    }

    void readsAMarkThatStraddlesHueZero()
    {
        // Red sits at both ends of the hue circle. Half the pixels of a red
        // mark land just above zero and half just below, and a vote counted
        // along a line rather than around a circle elects neither.
        QImage icon(32, 32, QImage::Format_ARGB32);
        icon.fill(Qt::transparent);
        QPainter painter(&icon);
        painter.fillRect(8, 8, 16, 8, QColor::fromHsv(3, 200, 220));
        painter.fillRect(8, 16, 16, 8, QColor::fromHsv(357, 200, 220));
        painter.end();

        const auto hue = faviconHue(icon);
        QVERIFY(hue.has_value());
        // The two halves average to red itself. Electing either half alone
        // lands a whole bucket — ten degrees — off.
        QVERIFY(std::min(*hue, 1.0 - *hue) < 0.004);
    }

    void offersNoHue_data()
    {
        QTest::addColumn<QImage>("icon");
        QTest::newRow("empty") << QImage();
        QTest::newRow("transparent") << filled(QColor(0, 0, 0, 0));
        QTest::newRow("white") << filled(Qt::white);
        QTest::newRow("grey") << filled(QColor(128, 128, 128));
        QTest::newRow("black") << filled(Qt::black);
        QTest::newRow("black on white") << markOn(Qt::white, Qt::black);
    }

    void offersNoHue()
    {
        QFETCH(QImage, icon);
        QVERIFY(!faviconHue(icon).has_value());
    }

    void ignoresTransparentPixels()
    {
        // Padding around a mark is transparent in most icons, and averaging it
        // in would drag every chip towards the same wash.
        QImage icon(32, 32, QImage::Format_ARGB32);
        icon.fill(QColor(0, 0, 0, 0));
        QPainter painter(&icon);
        painter.fillRect(10, 10, 6, 6, QColor(240, 150, 20));
        painter.end();
        const auto hue = faviconHue(icon);
        QVERIFY(hue.has_value());
        QVERIFY(qAbs(*hue - QColor(240, 150, 20).hueF()) < 0.05);
    }

    void readsAFileUrl()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("icon.png"));
        QVERIFY(filled(QColor(50, 90, 230)).save(path));

        FaviconTint tint;
        tint.setSaturation(0.5);
        tint.setLightness(0.6);
        tint.setSource(QUrl::fromLocalFile(path));
        QVERIFY(tint.isValid());
        const auto color = tint.color();
        QVERIFY(qAbs(color.hueF() - QColor(50, 90, 230).hueF()) < 0.03);
        // The site picks the hue; the theme keeps saturation and lightness, so
        // a chip never leaves the palette.
        QVERIFY(qAbs(color.hslSaturationF() - 0.5) < 0.02);
        QVERIFY(qAbs(color.lightnessF() - 0.6) < 0.02);
    }

    void staysInvalidForAnIconItCannotRead()
    {
        FaviconTint tint;
        tint.setSource(QUrl(QStringLiteral("file:///nowhere/missing.png")));
        QVERIFY(!tint.isValid());
        QVERIFY(!tint.color().isValid());
    }

    void staysInvalidForARemoteIcon()
    {
        // Nothing here fetches an icon: docs/network-requests.md lists every
        // request Tanto makes on its own, and a chip is not worth one.
        FaviconTint tint;
        tint.setSource(QUrl(QStringLiteral("https://example.com/favicon.ico")));
        QVERIFY(!tint.isValid());
    }

    void reportsAColourChangeOnce()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("icon.png"));
        QVERIFY(filled(QColor(220, 40, 40)).save(path));

        FaviconTint tint;
        QSignalSpy spy(&tint, &FaviconTint::colorChanged);
        tint.setSource(QUrl::fromLocalFile(path));
        QCOMPARE(spy.count(), 1);
        tint.setSource(QUrl::fromLocalFile(path));
        QCOMPARE(spy.count(), 1);
    }

    void readsASynchronousImageProvider()
    {
        QQmlEngine engine;
        registerFaviconTint();
        engine.addImageProvider(QStringLiteral("icons"), new SynchronousIconStore);

        auto *tint = createTint(engine, QStringLiteral("image://icons/#dc2828"));
        QVERIFY(tint);
        QVERIFY(tint->isValid());
        QVERIFY(qAbs(tint->color().hueF() - QColor(QStringLiteral("#dc2828")).hueF()) < 0.03);
    }

    void waitsForAnAsynchronousImageProvider()
    {
        QQmlEngine engine;
        registerFaviconTint();
        auto *store = new AsynchronousIconStore;
        engine.addImageProvider(QStringLiteral("deferred"), store);

        auto *tint = createTint(engine, QStringLiteral("image://deferred/#2850dc"));
        QVERIFY(tint);
        // The icon has not arrived, so the chip has no colour to draw yet
        // rather than a wrong one.
        QVERIFY(!tint->isValid());
        QCOMPARE(store->pending.size(), 1);

        store->pending.constFirst()->respond();
        QTRY_VERIFY(tint->isValid());
        QVERIFY(qAbs(tint->color().hueF() - QColor(QStringLiteral("#2850dc")).hueF()) < 0.03);
    }

    void dropsTheOldColourWhileAnAsynchronousIconIsPending()
    {
        // A tab that navigates must not keep painting the site it left behind
        // while the new icon is still on its way.
        QQmlEngine engine;
        registerFaviconTint();
        auto *store = new AsynchronousIconStore;
        engine.addImageProvider(QStringLiteral("navigating"), store);

        auto *tint = createTint(engine, QStringLiteral("image://navigating/#dc2828"));
        QVERIFY(tint);
        store->pending.constFirst()->respond();
        QTRY_VERIFY(tint->isValid());

        tint->setSource(QUrl(QStringLiteral("image://navigating/#2850dc")));
        QVERIFY(!tint->isValid());
        QCOMPARE(store->pending.size(), 2);
        store->pending.constLast()->respond();
        QTRY_VERIFY(tint->isValid());
        QVERIFY(qAbs(tint->color().hueF() - QColor(QStringLiteral("#2850dc")).hueF()) < 0.03);
    }

    void staysInvalidForAProviderTheEngineDoesNotHave()
    {
        QQmlEngine engine;
        registerFaviconTint();
        auto *tint = createTint(engine, QStringLiteral("image://absent/anything"));
        QVERIFY(tint);
        QVERIFY(!tint->isValid());
    }

    void followsTheThemeWithoutRereadingTheIcon()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("icon.png"));
        QVERIFY(filled(QColor(220, 40, 40)).save(path));

        FaviconTint tint;
        tint.setSource(QUrl::fromLocalFile(path));
        // The theme repaints often — on every theme-file save. Re-reading the
        // icon for a change the icon has nothing to do with would be waste.
        QVERIFY(QFile::remove(path));
        tint.setLightness(0.4);
        QVERIFY(tint.isValid());
        QVERIFY(qAbs(tint.color().lightnessF() - 0.4) < 0.02);
    }

private:
    // An image provider is only reachable through the QML engine that owns it,
    // so these have to be the engine's own objects rather than stack ones.
    FaviconTint *createTint(QQmlEngine &engine, const QString &source)
    {
        const auto qml = QStringLiteral(
            "import Tanto\nFaviconTint { source: \"%1\" }").arg(source);
        auto *component = new QQmlComponent(&engine, &engine);
        component->setData(qml.toUtf8(), QUrl());
        if (component->isError()) {
            qWarning("%s", qPrintable(component->errorString()));
            return nullptr;
        }
        auto *tint = qobject_cast<FaviconTint *>(component->create());
        if (tint) {
            tint->setParent(&engine);
        }
        return tint;
    }
};

QTEST_MAIN(TestFaviconTint)

#include "tst_favicontint.moc"
