#include "FaviconTint.h"

#include <QImage>
#include <QSize>
#include <QImageReader>
#include <QPixmap>
#include <QQmlEngine>
#include <QQmlFile>
#include <QQuickImageProvider>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>

namespace omaweb {
namespace {

// Enough of an icon to find its mark in. Real favicons are 16 to 64 pixels
// square; anything larger is sampled rather than scaled, because smooth
// scaling blends the transparent padding into the mark's edges.
constexpr int maximumSamples = 48;
constexpr int hueBuckets = 36;
// A pixel this transparent is padding, not the mark.
constexpr int opaqueEnough = 128;
// Below this the pixel is a shade of grey and names no hue.
constexpr qreal colourfulEnough = 0.2;
// Near-black pixels do carry a hue, but only as rounding noise.
constexpr qreal brightEnough = 0.16;
// About one fully saturated, fully opaque pixel. Less than that is a stray.
constexpr qreal enoughEvidence = 1.0;

// What an image provider is asked for. Providers are free to hand back
// whatever they have, so this is a hint, not a promise.
constexpr QSize requestedIconSize{maximumSamples, maximumSamples};

} // namespace

std::optional<qreal> faviconHue(const QImage &icon)
{
    if (icon.isNull()) {
        return std::nullopt;
    }
    const auto pixels = icon.convertToFormat(QImage::Format_ARGB32);
    const auto stride = std::max(1,
        (std::max(pixels.width(), pixels.height()) + maximumSamples - 1) / maximumSamples);

    std::array<qreal, hueBuckets> weights{};
    std::array<qreal, hueBuckets> horizontal{};
    std::array<qreal, hueBuckets> vertical{};
    for (int y = 0; y < pixels.height(); y += stride) {
        const auto *row = reinterpret_cast<const QRgb *>(pixels.constScanLine(y));
        for (int x = 0; x < pixels.width(); x += stride) {
            const auto pixel = row[x];
            const auto alpha = qAlpha(pixel);
            if (alpha < opaqueEnough) {
                continue;
            }
            const QColor colour(qRed(pixel), qGreen(pixel), qBlue(pixel));
            const auto saturation = colour.hsvSaturationF();
            if (saturation < colourfulEnough || colour.valueF() < brightEnough) {
                continue;
            }
            const auto hue = colour.hueF();
            const auto weight = saturation * (alpha / 255.0);
            const auto angle = hue * 2.0 * std::numbers::pi;
            const auto bucket = std::min(hueBuckets - 1, static_cast<int>(hue * hueBuckets));
            weights[bucket] += weight;
            horizontal[bucket] += weight * std::cos(angle);
            vertical[bucket] += weight * std::sin(angle);
        }
    }

    // Each bucket is scored with its two neighbours, and the buckets wrap: a
    // red mark sits either side of hue zero, and reading the buckets as a line
    // rather than a circle would split its vote in half and elect neither.
    const auto neighbour = [](int bucket) { return (bucket + hueBuckets) % hueBuckets; };
    auto winner = 0;
    auto best = -1.0;
    for (int bucket = 0; bucket < hueBuckets; ++bucket) {
        const auto score = weights[neighbour(bucket - 1)] + weights[bucket]
            + weights[neighbour(bucket + 1)];
        if (score > best) {
            best = score;
            winner = bucket;
        }
    }
    if (best < enoughEvidence) {
        return std::nullopt;
    }
    // The mean of the winning window rather than its centre, so a hue is not
    // rounded to the nearest ten degrees on its way to the chip.
    const auto sumOf = [&winner, &neighbour](const std::array<qreal, hueBuckets> &axis) {
        return axis[neighbour(winner - 1)] + axis[winner] + axis[neighbour(winner + 1)];
    };
    const auto mean
        = std::atan2(sumOf(vertical), sumOf(horizontal)) / (2.0 * std::numbers::pi);
    return mean < 0.0 ? mean + 1.0 : mean;
}

FaviconTint::FaviconTint(QObject *parent)
    : QObject(parent)
{
}

FaviconTint::~FaviconTint()
{
    abandonPendingRequest();
}

QUrl FaviconTint::source() const
{
    return m_source;
}

void FaviconTint::setSource(const QUrl &source)
{
    if (m_source == source) {
        return;
    }
    m_source = source;
    emit sourceChanged();
    resolve();
}

qreal FaviconTint::saturation() const
{
    return m_saturation;
}

void FaviconTint::setSaturation(qreal saturation)
{
    if (qFuzzyCompare(m_saturation, saturation)) {
        return;
    }
    m_saturation = saturation;
    emit saturationChanged();
    emit colorChanged();
}

qreal FaviconTint::lightness() const
{
    return m_lightness;
}

void FaviconTint::setLightness(qreal lightness)
{
    if (qFuzzyCompare(m_lightness, lightness)) {
        return;
    }
    m_lightness = lightness;
    emit lightnessChanged();
    emit colorChanged();
}

QColor FaviconTint::color() const
{
    if (!m_hue.has_value()) {
        return {};
    }
    return QColor::fromHslF(*m_hue, std::clamp(m_saturation, 0.0, 1.0),
        std::clamp(m_lightness, 0.0, 1.0));
}

bool FaviconTint::isValid() const
{
    return m_hue.has_value();
}

void FaviconTint::abandonPendingRequest()
{
    if (!m_response) {
        return;
    }
    auto *response = m_response.data();
    m_response = nullptr;
    disconnect(response, nullptr, this, nullptr);
    // The response still owns itself until it finishes, and deleting it early
    // races the provider's worker thread.
    connect(response, &QQuickImageResponse::finished, response, &QObject::deleteLater);
}

void FaviconTint::resolve()
{
    abandonPendingRequest();
    if (m_source.isEmpty()) {
        applyHue(std::nullopt);
        return;
    }

    if (m_source.scheme() != QLatin1String("image")) {
        // A local file or a resource. Anything else — an http icon Omaweb would
        // have to go and fetch — is left uncoloured on purpose.
        const auto path = QQmlFile::urlToLocalFileOrQrc(m_source);
        if (path.isEmpty()) {
            applyHue( std::nullopt);
            return;
        }
        QImageReader reader(path);
        reader.setAutoTransform(true);
        applyHue( faviconHue(reader.read()));
        return;
    }

    // An image provider the QML engine already has — a web engine's icon
    // store, most of the time. Reading it costs no request the engine has not
    // already made.
    auto *engine = qmlEngine(this);
    auto *provider = engine ? engine->imageProvider(m_source.host()) : nullptr;
    if (!provider) {
        applyHue( std::nullopt);
        return;
    }
    const auto identifier
        = m_source.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority).mid(1);
    switch (provider->imageType()) {
    case QQmlImageProviderBase::Image: {
        QSize size;
        applyHue(
            faviconHue(static_cast<QQuickImageProvider *>(provider)->requestImage(
                identifier, &size, requestedIconSize)));
        return;
    }
    case QQmlImageProviderBase::Pixmap: {
        QSize size;
        applyHue(
            faviconHue(static_cast<QQuickImageProvider *>(provider)
                    ->requestPixmap(identifier, &size, requestedIconSize)
                    .toImage()));
        return;
    }
    case QQmlImageProviderBase::ImageResponse: {
        auto *response = static_cast<QQuickAsyncImageProvider *>(provider)
                             ->requestImageResponse(identifier, requestedIconSize);
        if (!response) {
            applyHue( std::nullopt);
            return;
        }
        m_response = response;
        // Until it answers, the chip has no colour of its own. Holding the
        // previous site's would paint this one in a colour it never chose.
        applyHue(std::nullopt);
        connect(response, &QQuickImageResponse::finished, this, [this, response] {
            if (m_response == response) {
                m_response = nullptr;
                QImage image;
                if (response->errorString().isEmpty()) {
                    // The factory is the caller's to delete once it has been
                    // asked for its image.
                    const std::unique_ptr<QQuickTextureFactory> factory(
                        response->textureFactory());
                    if (factory) {
                        image = factory->image();
                    }
                }
                applyHue( faviconHue(image));
            }
            response->deleteLater();
        });
        return;
    }
    default:
        applyHue( std::nullopt);
        return;
    }
}

void FaviconTint::applyHue(std::optional<qreal> hue)
{
    if (m_hue == hue) {
        return;
    }
    m_hue = hue;
    emit colorChanged();
}

void registerFaviconTint()
{
    qmlRegisterType<FaviconTint>("Omaweb", 1, 0, "FaviconTint");
}

} // namespace omaweb
