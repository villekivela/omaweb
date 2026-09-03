#include "ThemeController.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace omaweb {
namespace {

// WCAG relative luminance and contrast ratio. Omaweb needs them for one
// decision — whether text can be read on the surface it is drawn on — and the
// theme palette is the only place that decision is made, so they live here
// rather than becoming a colour utility of their own.
double relativeLuminance(const QColor &colour)
{
    const auto channel = [](double value) {
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(colour.redF()) + 0.7152 * channel(colour.greenF())
        + 0.0722 * channel(colour.blueF());
}

double contrastRatio(const QColor &one, const QColor &other)
{
    const auto first = relativeLuminance(one);
    const auto second = relativeLuminance(other);
    return (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
}

QColor blended(const QColor &from, const QColor &to, double amount)
{
    const auto channel = [amount](int start, int end) {
        return qRound(start + (end - start) * amount);
    };
    return QColor::fromRgb(channel(from.red(), to.red()), channel(from.green(), to.green()),
        channel(from.blue(), to.blue()));
}

} // namespace

ThemeController::ThemeController(QString themePath, QObject *parent)
    : ThemeController(QStringList{std::move(themePath)}, parent)
{
}

ThemeController::ThemeController(QStringList themePaths, QObject *parent)
    : QObject(parent)
    , m_themePaths(std::move(themePaths))
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
    for (const auto &path : m_themePaths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const auto document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) {
            if (!m_palette.isEmpty()) {
                // A theme manager rewriting the file in place is momentarily
                // half-written. The palette on show stays until it reads
                // whole, rather than flashing whatever ranks below it.
                return;
            }
            // Nothing is on show yet, so there is nothing to protect and a
            // file that never reads whole must not leave the interface with no
            // colours at all.
            continue;
        }
        apply(normalizedPalette(document.object().toVariantMap()));
        return;
    }
    apply(normalizedPalette(fallbackPalette()));
}

void ThemeController::apply(QVariantMap palette)
{
    if (palette == m_palette) {
        return;
    }
    m_palette = std::move(palette);
    emit paletteChanged();
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
// them in these. Nine names rather than one per construct any one language has:
// a terminal palette has six hues to give, and a theme derived from one has to
// be able to fill every name here without inventing colour it does not have.
// There is deliberately no name for an attribute's value — it is a string, and
// an editor draws it as one.
QVariantMap ThemeController::defaultSyntax()
{
    return {
        {QStringLiteral("keyword"), QStringLiteral("#c678dd")},
        {QStringLiteral("string"), QStringLiteral("#98c379")},
        {QStringLiteral("number"), QStringLiteral("#d19a66")},
        {QStringLiteral("comment"), QStringLiteral("#7f7a8c")},
        {QStringLiteral("tag"), QStringLiteral("#e06c75")},
        {QStringLiteral("attribute"), QStringLiteral("#e5c07b")},
        {QStringLiteral("variable"), QStringLiteral("#e06c75")},
        {QStringLiteral("function"), QStringLiteral("#61afef")},
        {QStringLiteral("type"), QStringLiteral("#56b6c2")},
    };
}

QVariantMap ThemeController::defaultOpacity()
{
    return {
        {QStringLiteral("sidebar"), 0.95},
        // A sheet is read against a webpage rather than against the desktop,
        // and a page's own contrast is unknown, so it lets more through than
        // the sidebar does: at the sidebar's own value a dark page shows as
        // nothing at all and the surface reads as solid.
        {QStringLiteral("sheet"), 0.92},
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
    // A host with no fixed-pitch family is not one Omaweb can be picky on.
    const auto installed = QFontDatabase::families();
    return installed.isEmpty() ? QString{} : installed.constFirst();
}

QVariantMap ThemeController::normalizedPalette(QVariantMap palette) const
{
    // Asked before the defaults are filled in, because a colour Omaweb
    // supplied is not a colour the theme asked for.
    const auto themeNamedMutedText = palette.contains(QStringLiteral("mutedText"));
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

    // A Omaweb surface that takes the whole page area — the shortcut sheet
    // summoned over a page, the settings page — is the same material as the
    // sidebar and takes its colour. Not its translucency: the sidebar is read
    // against the desktop and these are read against a webpage. A theme that
    // wants a colour of its own for them names one, and inheriting rather than
    // falling back to Omaweb's own dark is what keeps a light theme light.
    const auto inheritSidebarColor = [&palette](const QString &sheetKey,
                                         const QString &sidebarKey) {
        if (!palette.contains(sheetKey)) {
            palette.insert(sheetKey, palette.value(sidebarKey));
        }
    };
    inheritSidebarColor(QStringLiteral("sheet"), QStringLiteral("sidebar"));
    inheritSidebarColor(QStringLiteral("privateSheet"), QStringLiteral("privateSidebar"));

    // Muted text is not decoration. It is a tab's title, a Space's letters,
    // the footer's controls — content the reader has to read, drawn quieter
    // than the rest so the rest can lead. A palette derived from a terminal's
    // sixteen colours has nothing to say about it: Omarchy offers
    // `dark_foreground`, which its own templates spend on
    // `disabledForeground`, and for a theme that names none of the extended
    // colours that is ANSI bright black — the value Omaweb also draws its
    // borders in. Drawn as text on the sidebar it lands below the disabled
    // rendering of ordinary text, so everything quiet reads as unavailable and
    // the two states the interface most needs to keep apart collapse into one.
    //
    // So the theme's colour is a preference and legibility is Omaweb's. The
    // named colour is taken toward `text` until it clears 4.5:1 — WCAG AA for
    // body text — against every Omaweb surface muted text is read on. A theme
    // whose colour already reads keeps it exactly; the rest keep their hue and
    // lose only the part that was unreadable. A theme that names no muted text
    // starts from the surface instead of from `text`, so what it gets is the
    // quietest tint of its own text colour that still reads, which is dark on
    // a light theme and light on a dark one without either being special-cased.
    //
    // A disabled control is `text` at 0.35 alpha, which cannot reach 3:1
    // against the ground it is composited over, so anything clearing this
    // floor also stays clearly ahead of disabled. One threshold does both jobs.
    const QColor text(palette.value(QStringLiteral("text")).toString());
    QList<QColor> grounds;
    for (const auto &key : {QStringLiteral("sidebar"), QStringLiteral("surface"),
             QStringLiteral("overlay"), QStringLiteral("sheet")}) {
        const QColor ground(palette.value(key).toString());
        if (ground.isValid()) {
            grounds.append(ground);
        }
    }
    const QColor named(palette.value(QStringLiteral("mutedText")).toString());
    const QColor quietest(themeNamedMutedText && named.isValid()
            ? named
            : QColor(palette.value(QStringLiteral("sidebar")).toString()));
    if (text.isValid() && quietest.isValid() && !grounds.isEmpty()) {
        constexpr auto minimumContrast = 4.5;
        constexpr auto steps = 32;
        const auto reads = [&grounds](const QColor &candidate) {
            return std::all_of(grounds.cbegin(), grounds.cend(), [&candidate](const QColor &g) {
                return contrastRatio(candidate, g) >= minimumContrast;
            });
        };
        // Ending on `text` itself: a theme whose ordinary text cannot be read
        // on its own surfaces has a problem no muted colour can fix, and
        // drawing the quiet parts in the same colour as the rest at least
        // stops them being the quietest thing on a page nobody can read.
        auto resolved = text;
        for (auto step = 0; step <= steps; ++step) {
            const auto candidate = blended(quietest, text, static_cast<double>(step) / steps);
            if (reads(candidate)) {
                resolved = candidate;
                break;
            }
        }
        palette.insert(QStringLiteral("mutedText"), resolved.name(QColor::HexRgb));
    }

    // Semantic opacity is the single source of truth for how much of the desktop
    // shows through a Omaweb-owned surface, so a theme that also bakes alpha into
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
    // whole of Omaweb's type contract.
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
    // beside Omaweb's. Alpha is dropped rather than honoured — code is read
    // against a solid surface, and a translucent character reads as a faded
    // one.
    auto syntaxDefaults = defaultSyntax();
    // Punctuation is structure rather than content. An editor draws brackets,
    // separators and quotes quieter than the names between them, and that
    // contrast is most of what makes code read as code, so unless the theme
    // names a colour for it, it is the interface's own muted text.
    syntaxDefaults.insert(QStringLiteral("punctuation"),
        palette.value(QStringLiteral("mutedText")));
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
    // A palette compiled into the binary never changes, and the file system
    // has nothing to say about a resource path.
    const auto watchable = [](const QString &path) {
        return !path.isEmpty() && !path.startsWith(QLatin1Char(':'))
            && QFileInfo::exists(path);
    };
    const auto watchDirectory = [this, &watchable](const QString &directory) {
        if (directory != QDir::homePath() && watchable(directory)
            && !m_watcher.directories().contains(directory)) {
            m_watcher.addPath(directory);
        }
    };

    for (const auto &path : m_themePaths) {
        if (path.startsWith(QLatin1Char(':'))) {
            continue;
        }
        // A watch resolves through symlinks, so it follows the inode the path
        // pointed at when it was added. A desktop that switches themes by
        // relinking a directory leaves that watch on the theme the reader has
        // just left: the file it holds never changes again, and the palette at
        // the same name is a different file nobody is watching. So the target
        // is remembered, and a watch whose target moved is taken off and put
        // back on the name rather than the file it used to be.
        const auto canonical = QFileInfo(path).canonicalFilePath();
        const auto previous = m_watchedTargets.value(path);
        if (!previous.isEmpty() && previous != canonical) {
            m_watcher.removePath(path);
            m_watchedTargets.remove(path);
        }
        if (watchable(path) && !m_watcher.files().contains(path)) {
            m_watcher.addPath(path);
            m_watchedTargets.insert(path, canonical);
        }

        // Neither a candidate nor the directory a theme manager renders it
        // into need exist yet. The deepest directory that does is watched, so
        // each level appearing arms the one below it and the palette is picked
        // up whenever it finally lands. The reader's home is where that stops:
        // watching it would reload the palette on every file any tool writes.
        auto directory = QFileInfo(path).absolutePath();
        const auto home = QDir::homePath();
        while (!watchable(directory) && directory != home) {
            const auto parent = QFileInfo(directory).absolutePath();
            if (parent == directory || parent.isEmpty()) {
                break;
            }
            directory = parent;
        }
        watchDirectory(directory);

        // The relink itself happens in the directory that holds the link, and
        // nothing inside the directory it points at moves. That parent is
        // watched too -- but only where a link is actually in play, because
        // the ordinary case is a configuration directory whose parent is
        // `~/.config`, which every tool on the machine writes to.
        if (watchable(directory)
            && QFileInfo(directory).canonicalFilePath() != QDir(directory).absolutePath()) {
            watchDirectory(QFileInfo(directory).absolutePath());
        }
    }
}

} // namespace omaweb
