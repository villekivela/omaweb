#include "ThemeController.h"

#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace tanto {

ThemeController::ThemeController(QString themePath, QObject *parent)
    : QObject(parent)
    , m_themePath(std::move(themePath))
    , m_themeDirectory(QFileInfo(m_themePath).absolutePath())
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] {
        reload();
        refreshWatchPaths();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        reload();
        refreshWatchPaths();
    });
    refreshWatchPaths();
    reload();
}

QVariantMap ThemeController::palette() const
{
    return m_palette;
}

void ThemeController::reload()
{
    QFile file(m_themePath);
    if (!file.open(QIODevice::ReadOnly)) {
        const auto fallback = normalizedPalette(fallbackPalette());
        if (m_palette != fallback) {
            m_palette = fallback;
            emit paletteChanged();
        }
        return;
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return;
    }
    const auto next = normalizedPalette(document.object().toVariantMap());
    if (next != m_palette) {
        m_palette = next;
        emit paletteChanged();
    }
}

QVariantMap ThemeController::fallbackPalette() const
{
    return {
        {QStringLiteral("window"), QStringLiteral("#16151d")},
        {QStringLiteral("sidebar"), QStringLiteral("#1d1b29")},
        {QStringLiteral("overlay"), QStringLiteral("#282634")},
        {QStringLiteral("surface"), QStringLiteral("#302e3d")},
        {QStringLiteral("surfaceHover"), QStringLiteral("#3d394e")},
        {QStringLiteral("text"), QStringLiteral("#f3f1fa")},
        {QStringLiteral("mutedText"), QStringLiteral("#aaa5b7")},
        {QStringLiteral("accent"), QStringLiteral("#9b87ff")},
        {QStringLiteral("border"), QStringLiteral("#4a4658")},
        {QStringLiteral("privateAccent"), QStringLiteral("#dc6bce")},
        {QStringLiteral("privateWindow"), QStringLiteral("#481d50")},
        {QStringLiteral("privateSidebar"), QStringLiteral("#5b2456")},
        {QStringLiteral("privateSurface"), QStringLiteral("#5a3158")},
        {QStringLiteral("privateSurfaceHover"), QStringLiteral("#70406c")},
        {QStringLiteral("opacity"), defaultOpacity()},
    };
}

QVariantMap ThemeController::defaultOpacity()
{
    return {
        {QStringLiteral("sidebar"), 0.85},
        {QStringLiteral("overlay"), 0.96},
        {QStringLiteral("window"), 0.0},
    };
}

QVariantMap ThemeController::normalizedPalette(QVariantMap palette) const
{
    const auto fallback = fallbackPalette();
    for (auto it = fallback.cbegin(); it != fallback.cend(); ++it) {
        if (!palette.contains(it.key())) {
            palette.insert(it.key(), it.value());
        }
    }

    const auto enforceDifference = [&palette, &fallback](const QString &normalKey,
                                       const QString &privateKey) {
        const QColor normal(palette.value(normalKey).toString());
        QColor privateColor(palette.value(privateKey).toString());
        const auto distanceSquared = [&normal](const QColor &candidate) {
            const auto red = normal.red() - candidate.red();
            const auto green = normal.green() - candidate.green();
            const auto blue = normal.blue() - candidate.blue();
            return red * red + green * green + blue * blue;
        };
        constexpr auto minimumDistanceSquared = 48 * 48;
        if (normal.isValid() && privateColor.isValid()
            && distanceSquared(privateColor) >= minimumDistanceSquared) {
            return;
        }

        privateColor = QColor(fallback.value(privateKey).toString());
        if (normal.isValid() && privateColor.isValid()
            && distanceSquared(privateColor) >= minimumDistanceSquared) {
            palette.insert(privateKey, fallback.value(privateKey));
            return;
        }

        const QColor black(Qt::black);
        const QColor white(Qt::white);
        palette.insert(privateKey,
            distanceSquared(black) > distanceSquared(white)
                ? black.name(QColor::HexRgb)
                : white.name(QColor::HexRgb));
    };

    enforceDifference(QStringLiteral("window"), QStringLiteral("privateWindow"));
    enforceDifference(QStringLiteral("accent"), QStringLiteral("privateAccent"));

    // Semantic opacity is the single source of truth for how much of the desktop
    // shows through a Tanto-owned surface, so a theme that also bakes alpha into
    // the colour itself does not get to multiply the two. The opaque variants stay
    // available for the places that must hide whatever is behind them.
    auto opacity = defaultOpacity();
    const auto themeOpacity = palette.value(QStringLiteral("opacity")).toMap();
    for (auto it = themeOpacity.cbegin(); it != themeOpacity.cend(); ++it) {
        if (!opacity.contains(it.key())) {
            continue;
        }
        auto valid = false;
        const auto value = it.value().toDouble(&valid);
        if (valid) {
            opacity.insert(it.key(), std::clamp(value, 0.0, 1.0));
        }
    }
    palette.insert(QStringLiteral("opacity"), opacity);

    const auto withOpacity = [&palette](const QString &key, double alpha) {
        QColor color(palette.value(key).toString());
        if (!color.isValid()) {
            return;
        }
        palette.insert(key + QStringLiteral("Opaque"), color.name(QColor::HexRgb));
        color.setAlphaF(alpha);
        palette.insert(key, color.name(QColor::HexArgb));
    };

    const auto windowAlpha = opacity.value(QStringLiteral("window")).toDouble();
    const auto sidebarAlpha = opacity.value(QStringLiteral("sidebar")).toDouble();
    const auto overlayAlpha = opacity.value(QStringLiteral("overlay")).toDouble();
    withOpacity(QStringLiteral("window"), windowAlpha);
    withOpacity(QStringLiteral("privateWindow"), windowAlpha);
    withOpacity(QStringLiteral("sidebar"), sidebarAlpha);
    withOpacity(QStringLiteral("privateSidebar"), sidebarAlpha);
    withOpacity(QStringLiteral("overlay"), overlayAlpha);
    return palette;
}

void ThemeController::refreshWatchPaths()
{
    if (QFile::exists(m_themePath) && !m_watcher.files().contains(m_themePath)) {
        m_watcher.addPath(m_themePath);
    }
    if (QFileInfo::exists(m_themeDirectory) && !m_watcher.directories().contains(m_themeDirectory)) {
        m_watcher.addPath(m_themeDirectory);
    }
}

} // namespace tanto
