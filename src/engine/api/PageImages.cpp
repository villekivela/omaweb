#include "PageImages.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QUuid>

namespace omaweb {

PageImages::PageImages(QObject *parent)
    : QObject(parent)
{
}

QString PageImages::reserveStrip() const
{
    const QDir temporary(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    return temporary.filePath(QStringLiteral("omaweb-strip-%1.png")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

bool PageImages::join(const QStringList &strips, const QVariantList &tops, qreal pageHeight,
    qreal viewportHeight, const QString &path) const
{
    QList<QImage> images;
    for (const auto &strip : strips) {
        images.append(QImage(strip));
        QFile::remove(strip);
    }
    if (path.isEmpty() || images.isEmpty() || images.size() != tops.size() || pageHeight <= 0
        || viewportHeight <= 0) {
        return false;
    }
    const auto &first = images.first();
    if (first.isNull()) {
        return false;
    }
    const auto scale = first.height() / viewportHeight;
    const auto height = qRound(pageHeight * scale);
    if (height <= 0 || height > kHeightLimit) {
        return false;
    }
    QImage page(first.width(), height, QImage::Format_ARGB32_Premultiplied);
    if (page.isNull()) {
        return false;
    }
    page.fill(Qt::transparent);
    {
        QPainter painter(&page);
        for (qsizetype index = 0; index < images.size(); ++index) {
            auto strip = images.at(index);
            if (strip.isNull()) {
                return false;
            }
            // The page is laid out in the image's own pixels, so each strip is
            // drawn one to one whatever pixel ratio it was saved with.
            strip.setDevicePixelRatio(1);
            painter.drawImage(0, qRound(tops.at(index).toReal() * scale), strip);
        }
    }
    if (!page.save(path, "PNG")) {
        QFile::remove(path);
        return false;
    }
    return true;
}

void registerPageImages()
{
    qmlRegisterSingletonType<PageImages>("Omaweb.Engine", 1, 0, "PageImages",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new PageImages; });
}

} // namespace omaweb
