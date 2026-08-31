#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <optional>

class QImage;
class QQuickImageResponse;

namespace tanto {

// The hue a favicon reads as, in [0, 1), or nothing when the icon has no
// colour to give. Pixels that are near-transparent, near-neutral or nearly
// black are discarded first: most favicons are a mark on a white or empty
// plate, and averaging the plate in would wash every site into the same grey.
// The surviving pixels vote for their hue, weighted by how saturated and how
// opaque they are, so the mark decides even when it is small.
std::optional<qreal> faviconHue(const QImage &icon);

// A site's own colour, for the chip that stands in for its favicon. The hue
// comes from the icon, the saturation and lightness from the theme, so a chip
// stays in the sidebar's palette exactly as a hashed one does.
//
// The icon is read from wherever the QML engine can already reach it: a local
// file, a resource, or an image provider such as the one a web engine
// registers for its icon store. Nothing is fetched over the network, and
// nothing is remembered between sources: a colour read from a private
// window's icon goes when the tile that asked for it does.
class FaviconTint : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
    Q_PROPERTY(qreal lightness READ lightness WRITE setLightness NOTIFY lightnessChanged)
    Q_PROPERTY(QColor color READ color NOTIFY colorChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY colorChanged)

public:
    explicit FaviconTint(QObject *parent = nullptr);
    ~FaviconTint() override;

    QUrl source() const;
    void setSource(const QUrl &source);

    qreal saturation() const;
    void setSaturation(qreal saturation);

    qreal lightness() const;
    void setLightness(qreal lightness);

    // An invalid colour while the icon is unknown or offers no hue. Callers
    // draw their own neutral rather than being handed one.
    QColor color() const;
    bool isValid() const;

signals:
    void sourceChanged();
    void saturationChanged();
    void lightnessChanged();
    void colorChanged();

private:
    void resolve();
    void applyHue(std::optional<qreal> hue);
    void abandonPendingRequest();

    QUrl m_source;
    qreal m_saturation = 0.32;
    qreal m_lightness = 0.62;
    std::optional<qreal> m_hue;
    QPointer<QQuickImageResponse> m_response;
};

// Makes `FaviconTint` available to QML as `import Tanto`. Call once per
// process, before loading QML that uses it.
void registerFaviconTint();

} // namespace tanto
