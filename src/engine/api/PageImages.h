#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace omaweb {

// The picture of a whole page, put together from the strips a view grabs of
// it one screenful at a time. A view can only draw what fits in it, so a page
// taller than the view is captured by scrolling it and grabbing each part.
class PageImages final : public QObject {
    Q_OBJECT
    // The tallest page a full-page screenshot holds, in device pixels. A PNG
    // taller than this is refused rather than cut short, because a picture
    // that stops part way down reads as the whole page.
    Q_PROPERTY(int heightLimit READ heightLimit CONSTANT)

public:
    explicit PageImages(QObject *parent = nullptr);

    static constexpr int kHeightLimit = 32767;
    int heightLimit() const { return kHeightLimit; }

    // Where the page is scrolled for each strip, in the page's own pixels: a
    // screenful at a time from the top, the last only as far as the page
    // goes, so it overlaps the one before it. Empty for a view with no height.
    Q_INVOKABLE QVariantList stripTops(qreal pageHeight, qreal viewportHeight) const;

    // Why a page `pageHeight` tall, drawn at `ratio` image pixels to one of
    // its own, is too tall for a screenshot, naming the height and the limit;
    // empty when it fits.
    Q_INVOKABLE QString heightRefusal(qreal pageHeight, qreal ratio) const;

    // A file for one strip to be written to. The image a grab hands to
    // JavaScript does not outlive the grab, so each strip is kept as a file
    // until the page is joined.
    Q_INVOKABLE QString reserveStrip() const;

    // Draws each strip, an image a view grabbed of one screenful, at the top
    // the page was scrolled to for it, onto one image of the whole page, and
    // writes that to `path` as a PNG. Tops and heights are the page's own
    // pixels; the first strip says how many of the image's pixels one of the
    // page's came to, whatever the zoom and the display made of it. A later
    // strip is drawn over an earlier one where the two overlap, which is how
    // the last strip, scrolled to the bottom, meets the one above it. Nothing
    // is left at `path` when the image cannot be made or written, or would be
    // taller than the limit. The strips are files from `reserveStrip`, and
    // are removed whatever the answer.
    Q_INVOKABLE bool join(const QStringList &strips, const QVariantList &tops, qreal pageHeight,
        qreal viewportHeight, const QString &path) const;
};

// Makes `PageImages` available to QML as `import Omaweb.Engine`. Call once per
// process, before loading QML that uses it.
void registerPageImages();

} // namespace omaweb
