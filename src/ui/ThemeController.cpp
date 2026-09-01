#include "ThemeController.h"

#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QFontDatabase>
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
        // What the reader has to be told rather than shown: a binding that
        // could not be honoured, a surface that failed. Distinct from the
        // private accent, which says whose window this is and not that
        // something is wrong.
        {QStringLiteral("urgent"), QStringLiteral("#e06c75")},
        {QStringLiteral("privateAccent"), QStringLiteral("#c678dd")},
        {QStringLiteral("privateWindow"), QStringLiteral("#362640")},
        {QStringLiteral("privateSidebar"), QStringLiteral("#3f2c4c")},
        {QStringLiteral("privateSurface"), QStringLiteral("#4a3057")},
        {QStringLiteral("privateSurfaceHover"), QStringLiteral("#5f3b6e")},
        {QStringLiteral("font"), defaultFont()},
        {QStringLiteral("opacity"), defaultOpacity()},
        {QStringLiteral("syntax"), defaultSyntax()},
    };
}

// The engine's inspector draws source, markup and stylesheets, and it draws
// them in these. Ten names rather than one per construct any one language has:
// a terminal palette has six hues to give, and a theme derived from one has to
// be able to fill every name here without inventing colour it does not have.
QVariantMap ThemeController::defaultSyntax()
{
    return {
        {QStringLiteral("keyword"), QStringLiteral("#c678dd")},
        {QStringLiteral("string"), QStringLiteral("#98c379")},
        {QStringLiteral("number"), QStringLiteral("#d19a66")},
        {QStringLiteral("comment"), QStringLiteral("#7f7a8c")},
        {QStringLiteral("tag"), QStringLiteral("#e06c75")},
        {QStringLiteral("attribute"), QStringLiteral("#e5c07b")},
        {QStringLiteral("value"), QStringLiteral("#98c379")},
        {QStringLiteral("variable"), QStringLiteral("#e06c75")},
        {QStringLiteral("function"), QStringLiteral("#61afef")},
        {QStringLiteral("type"), QStringLiteral("#56b6c2")},
    };
}

QVariantMap ThemeController::defaultOpacity()
{
    return {
        {QStringLiteral("sidebar"), 0.85},
        // A sheet is read against a webpage rather than against the desktop,
        // and a page's own contrast is unknown, so it lets more through than
        // the sidebar does: at the sidebar's own value a dark page shows as
        // nothing at all and the surface reads as solid.
        {QStringLiteral("sheet"), 0.82},
        {QStringLiteral("overlay"), 0.96},
        {QStringLiteral("window"), 0.0},
    };
}

QVariantMap ThemeController::defaultFont()
{
    // Deliberately no "monospace" tail: that is a fontconfig alias rather than
    // a family, and the host's own fixed-pitch family is a better last resort
    // than asking Qt to substitute for a name it cannot find.
    return {
        {QStringLiteral("families"),
            QStringList{QStringLiteral("JetBrains Mono"), QStringLiteral("SF Mono"),
                QStringLiteral("Menlo"), QStringLiteral("DejaVu Sans Mono")}},
        {QStringLiteral("size"), 12},
    };
}

QString ThemeController::installedFamily(const QStringList &candidates)
{
    for (const auto &candidate : candidates) {
        if (QFontDatabase::hasFamily(candidate)) {
            return candidate;
        }
    }
    const auto fixed = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    if (QFontDatabase::hasFamily(fixed)) {
        return fixed;
    }
    // A host with no fixed-pitch family is not one Tanto can be picky on.
    const auto installed = QFontDatabase::families();
    return installed.isEmpty() ? QString{} : installed.constFirst();
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

    // A Tanto surface that takes the whole page area — the shortcut sheet
    // summoned over a page, the settings page — is the same material as the
    // sidebar and takes its colour. Not its translucency: the sidebar is read
    // against the desktop and these are read against a webpage. A theme that
    // wants a colour of its own for them names one, and inheriting rather than
    // falling back to Tanto's own dark is what keeps a light theme light.
    const auto inheritSidebarColor = [&palette](const QString &sheetKey,
                                         const QString &sidebarKey) {
        if (!palette.contains(sheetKey)) {
            palette.insert(sheetKey, palette.value(sidebarKey));
        }
    };
    inheritSidebarColor(QStringLiteral("sheet"), QStringLiteral("sidebar"));
    inheritSidebarColor(QStringLiteral("privateSheet"), QStringLiteral("privateSidebar"));

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

    // The theme names the families it prefers and the size the type scale
    // grows from; the resolved family is what the interface actually draws
    // with. Everything above the base size is derived from it, so this is the
    // whole of Tanto's type contract.
    const auto defaults = defaultFont();
    const auto themeFont = palette.value(QStringLiteral("font")).toMap();
    auto families = themeFont.value(QStringLiteral("families")).toStringList();
    families.removeAll(QString{});
    if (families.isEmpty()) {
        families = defaults.value(QStringLiteral("families")).toStringList();
    }
    auto validSize = false;
    const auto requestedSize = themeFont.value(QStringLiteral("size")).toInt(&validSize);
    // Only a sanity floor, as the kit's own scale has: a theme that wants very
    // large type is entitled to ship it.
    const auto size = (validSize && requestedSize >= 1)
        ? requestedSize
        : defaults.value(QStringLiteral("size")).toInt();
    palette.insert(QStringLiteral("font"),
        QVariantMap{
            {QStringLiteral("families"), families},
            {QStringLiteral("family"), installedFamily(families)},
            {QStringLiteral("size"), size},
        });

    // Every token is named, whether the theme named it or not, and every one
    // is a colour: the inspector is handed these directly, and a name it
    // cannot parse leaves that token drawn in the frontend's own palette
    // beside Tanto's. Alpha is dropped rather than honoured — code is read
    // against a solid surface, and a translucent character reads as a faded
    // one.
    const auto syntaxDefaults = defaultSyntax();
    const auto themeSyntax = palette.value(QStringLiteral("syntax")).toMap();
    QVariantMap syntax;
    for (auto it = syntaxDefaults.cbegin(); it != syntaxDefaults.cend(); ++it) {
        const QColor named(themeSyntax.value(it.key()).toString());
        syntax.insert(it.key(),
            named.isValid() ? named.name(QColor::HexRgb) : it.value().toString());
    }
    palette.insert(QStringLiteral("syntax"), syntax);

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
    const auto sheetAlpha = opacity.value(QStringLiteral("sheet")).toDouble();
    withOpacity(QStringLiteral("window"), windowAlpha);
    withOpacity(QStringLiteral("privateWindow"), windowAlpha);
    withOpacity(QStringLiteral("sidebar"), sidebarAlpha);
    withOpacity(QStringLiteral("privateSidebar"), sidebarAlpha);
    withOpacity(QStringLiteral("sheet"), sheetAlpha);
    withOpacity(QStringLiteral("privateSheet"), sheetAlpha);
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
