#include "StoredFaviconProvider.h"

#include "FaviconTint.h"
#include "StoredFavicons.h"

#include <QBuffer>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickImageProvider>

#include <memory>

namespace omaweb {
namespace {

    // The largest icon worth keeping. A sidebar tile draws at 20 pixels, and
    // a page that declares a large touch icon would otherwise store it whole.
    constexpr int keptIconSize = 64;

    QByteArray encode(const QImage &icon)
    {
        if (icon.isNull()) {
            return {};
        }
        const auto kept = icon.width() > keptIconSize || icon.height() > keptIconSize
            ? icon.scaled(keptIconSize, keptIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            : icon;
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::WriteOnly) || !kept.save(&buffer, "PNG")) {
            return {};
        }
        return bytes;
    }

    class StoredFaviconResponse final : public QQuickImageResponse {
    public:
        // The store answers on a thread of its own, possibly after the image
        // that asked has been let go of. The answer reaches the response only
        // through this, which the response empties as it goes.
        struct Delivery {
            QMutex mutex;
            StoredFaviconResponse *response = nullptr;
        };

        StoredFaviconResponse()
            : m_delivery(std::make_shared<Delivery>())
        {
            m_delivery->response = this;
        }

        ~StoredFaviconResponse() override
        {
            const QMutexLocker locker(&m_delivery->mutex);
            m_delivery->response = nullptr;
        }

        std::shared_ptr<Delivery> delivery() const { return m_delivery; }

        // Called with the delivery locked, from whichever thread answered.
        // Finishing is posted to the response's own thread, and dropped there
        // if the response is gone by then.
        void deliver(const QByteArray &image)
        {
            m_image = QImage::fromData(image);
            // Nothing stored is answered as one transparent pixel rather than
            // an error, which the image would log for every tab and row that
            // asked. A tile reads a pixel as no artwork and draws the host code.
            if (m_image.isNull()) {
                m_image = QImage(1, 1, QImage::Format_ARGB32);
                m_image.fill(Qt::transparent);
            }
            QMetaObject::invokeMethod(this, [this] { emit finished(); }, Qt::QueuedConnection);
        }

        QQuickTextureFactory *textureFactory() const override
        {
            return QQuickTextureFactory::textureFactoryForImage(m_image);
        }

    private:
        std::shared_ptr<Delivery> m_delivery;
        QImage m_image;
    };

    class StoredFaviconProvider final : public QQuickAsyncImageProvider {
    public:
        QQuickImageResponse *requestImageResponse(const QString &id, const QSize &) override
        {
            auto *response = new StoredFaviconResponse;
            storedfavicons::find(id, [delivery = response->delivery()](const QByteArray &image) {
                const QMutexLocker locker(&delivery->mutex);
                if (delivery->response) {
                    delivery->response->deliver(image);
                }
            });
            return response;
        }
    };

} // namespace

void installStoredFavicons(QQmlEngine &engine)
{
    engine.addImageProvider(QLatin1String(storedfavicons::providerId), new StoredFaviconProvider);
    storedfavicons::setReader([engine = QPointer<QQmlEngine>(&engine)](const QUrl &iconUrl,
                                  QObject *context, storedfavicons::Answer answer) {
        if (!engine) {
            answer({});
            return;
        }
        readIcon(engine, iconUrl, context,
            [answer = std::move(answer)](const QImage &icon) { answer(encode(icon)); });
    });
    // An engine that goes takes its reader with it, unless another engine has
    // installed its own since.
    static QQmlEngine *readingEngine = nullptr;
    readingEngine = &engine;
    QObject::connect(&engine, &QObject::destroyed, [gone = &engine] {
        if (readingEngine == gone) {
            readingEngine = nullptr;
            storedfavicons::setReader({});
        }
    });
}

} // namespace omaweb
