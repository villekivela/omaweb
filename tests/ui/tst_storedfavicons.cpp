#include "PrivateSessionStore.h"
#include "StoredFaviconProvider.h"
#include "StoredFavicons.h"

#include <QAtomicInt>
#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QNetworkAccessManager>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QtTest>

#include <memory>

using namespace omaweb;

namespace {

QImage filled(const QColor &colour)
{
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(colour);
    return image;
}

QByteArray png(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

// Every request the QML engine makes on anyone's behalf passes through here.
QAtomicInt networkRequests;

class CountingNetwork final : public QNetworkAccessManager {
public:
    using QNetworkAccessManager::QNetworkAccessManager;

protected:
    QNetworkReply *createRequest(
        Operation operation, const QNetworkRequest &request, QIODevice *data) override
    {
        networkRequests.fetchAndAddRelaxed(1);
        return QNetworkAccessManager::createRequest(operation, request, data);
    }
};

class CountingNetworkFactory final : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager *create(QObject *parent) override { return new CountingNetwork(parent); }
};

// A web engine's icon store, as the reader finds it on the QML engine.
class ColourIcons final : public QQuickImageProvider {
public:
    ColourIcons()
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

} // namespace

class StoredFaviconsTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void drawsAStoredFaviconWithoutANetworkRequest();
    void aPageWithNothingStoredLoadsAsNoArtwork();
    void readsAReportedIconFromTheEngineAndNotTheNetwork();
    void aWindowThatClosesMidLookupStillFinishesTheImage();

private:
    QUrl storedAddress(const QUrl &pageUrl) const;
    QQuickItem *image(const QUrl &source);

    std::unique_ptr<CountingNetworkFactory> m_network;
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<PrivateSessionStore> m_store;
    QObject m_window;
    QString m_source;
    std::unique_ptr<QObject> m_item;
};

void StoredFaviconsTest::init()
{
    networkRequests.storeRelaxed(0);
    m_network = std::make_unique<CountingNetworkFactory>();
    m_engine = std::make_unique<QQmlEngine>();
    m_engine->setNetworkAccessManagerFactory(m_network.get());
    m_engine->addImageProvider(QStringLiteral("colour"), new ColourIcons);
    installStoredFavicons(*m_engine);
    m_store = std::make_unique<PrivateSessionStore>(QSharedPointer<QHash<QString, int>>::create());
    // The lookup is asked from the image loader's thread and carried to the
    // window's, as a window's own lookup is.
    m_source = storedfavicons::addSource([this](const QString &spaceId, const QUrl &pageUrl,
                                             storedfavicons::Answer answer) {
        QMetaObject::invokeMethod(
            &m_window,
            [this, spaceId, pageUrl, answer] { m_store->findFavicon(spaceId, pageUrl, answer); },
            Qt::QueuedConnection);
    });
}

void StoredFaviconsTest::cleanup()
{
    m_item.reset();
    storedfavicons::removeSource(m_source);
    m_engine.reset();
    m_store.reset();
    m_network.reset();
}

QUrl StoredFaviconsTest::storedAddress(const QUrl &pageUrl) const
{
    return storedfavicons::address(m_source, QStringLiteral("space"), pageUrl);
}

QQuickItem *StoredFaviconsTest::image(const QUrl &source)
{
    QQmlComponent component(m_engine.get());
    component.setData(QByteArrayLiteral("import QtQuick\nImage { asynchronous: true }"), QUrl());
    m_item.reset(component.create());
    auto *item = qobject_cast<QQuickItem *>(m_item.get());
    if (item) {
        item->setProperty("source", source);
    }
    return item;
}

void StoredFaviconsTest::drawsAStoredFaviconWithoutANetworkRequest()
{
    QVERIFY(m_store->recordFavicon(QStringLiteral("space"),
        QUrl(QStringLiteral("https://a.example/page")), png(filled(QColor(0, 160, 80)))));

    auto *item = image(storedAddress(QUrl(QStringLiteral("https://a.example/elsewhere"))));
    QVERIFY(item);
    QTRY_COMPARE(item->property("status").toInt(), 1); // Image.Ready
    QCOMPARE(item->implicitWidth(), 16.0);
    QCOMPARE(networkRequests.loadRelaxed(), 0);
}

// Nothing stored loads as a single transparent pixel, which a tile reads as no
// artwork, rather than as an error the image would log for every tab.
void StoredFaviconsTest::aPageWithNothingStoredLoadsAsNoArtwork()
{
    auto *item = image(storedAddress(QUrl(QStringLiteral("https://never.example/"))));
    QVERIFY(item);
    QTRY_COMPARE(item->property("status").toInt(), 1); // Image.Ready
    QCOMPARE(item->implicitWidth(), 1.0);
    // An address naming a window that has gone answers the same way.
    storedfavicons::removeSource(m_source);
    item->setProperty("source", storedAddress(QUrl(QStringLiteral("https://again.example/"))));
    QTRY_COMPARE(item->property("status").toInt(), 1);
    QCOMPARE(item->implicitWidth(), 1.0);
    QCOMPARE(networkRequests.loadRelaxed(), 0);
}

// What a page reported is read from the engine that already holds it, never
// fetched: an icon only the network could supply reads as nothing.
void StoredFaviconsTest::readsAReportedIconFromTheEngineAndNotTheNetwork()
{
    QObject context;
    QByteArray fromEngine;
    bool engineAnswered = false;
    storedfavicons::read(
        QUrl(QStringLiteral("image://colour/#2060d0")), &context, [&](const QByteArray &image) {
            fromEngine = image;
            engineAnswered = true;
        });
    QTRY_VERIFY(engineAnswered);
    const auto decoded = QImage::fromData(fromEngine);
    QCOMPARE(decoded.size(), QSize(16, 16));
    QCOMPARE(decoded.pixelColor(8, 8), QColor(0x20, 0x60, 0xd0));

    QByteArray fromNetwork = QByteArrayLiteral("unanswered");
    storedfavicons::read(QUrl(QStringLiteral("https://a.example/favicon.ico")), &context,
        [&](const QByteArray &image) { fromNetwork = image; });
    QTRY_VERIFY(fromNetwork.isEmpty());
    QCOMPARE(networkRequests.loadRelaxed(), 0);
}

// A window can close with a lookup still on its way to it. The image waiting
// on that lookup finishes with nothing rather than loading forever.
void StoredFaviconsTest::aWindowThatClosesMidLookupStillFinishesTheImage()
{
    auto window = std::make_unique<QObject>();
    const auto closing = storedfavicons::addSource(
        [context = window.get()](const QString &, const QUrl &, storedfavicons::Answer answer) {
            QMetaObject::invokeMethod(
                context, [answer] { answer(QByteArrayLiteral("never")); }, Qt::QueuedConnection);
        });
    auto *item = image(storedfavicons::address(
        closing, QStringLiteral("space"), QUrl(QStringLiteral("https://closing.example/"))));
    QVERIFY(item);
    QTRY_VERIFY(item->property("status").toInt() != 0);
    storedfavicons::removeSource(closing);
    window.reset();
    QTRY_COMPARE(item->property("status").toInt(), 1); // Image.Ready
    QCOMPARE(item->implicitWidth(), 1.0);
}

QTEST_MAIN(StoredFaviconsTest)
#include "tst_storedfavicons.moc"
