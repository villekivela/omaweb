#include "ThemeController.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

#include <algorithm>
#include <cmath>
#include <limits>

namespace omaweb {
namespace {

    // sRGB's transfer function, both ways. Every derivation below reasons about
    // light rather than about the numbers a hex colour is written in, so this is
    // where the encoding stops.
    double toLinear(double channel)
    {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    }

    // Back to an eight-bit channel rather than to a float, because a palette is
    // hex colours in a JSON file and every derivation here has to land on the same
    // value the importer's does. QColor's own float constructor quantises through
    // sixteen bits and can round a channel a step past where it belongs.
    int fromLinear(double channel)
    {
        const auto encoded
            = channel <= 0.0031308 ? 12.92 * channel : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
        return qRound(std::clamp(encoded, 0.0, 1.0) * 255.0);
    }

    // WCAG relative luminance and contrast ratio. The theme palette decides
    // whether text can be read and borders can be seen on their surfaces, so the
    // calculation lives here rather than becoming a general colour utility.
    double relativeLuminance(const QColor &colour)
    {
        return 0.2126 * toLinear(colour.redF()) + 0.7152 * toLinear(colour.greenF())
            + 0.0722 * toLinear(colour.blueF());
    }

    double contrastRatio(const QColor &one, const QColor &other)
    {
        const auto first = relativeLuminance(one);
        const auto second = relativeLuminance(other);
        return (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
    }

    QColor blended(const QColor &from, const QColor &to, double amount)
    {
        const auto channel
            = [amount](int start, int end) { return qRound(start + (end - start) * amount); };
        return QColor::fromRgb(channel(from.red(), to.red()), channel(from.green(), to.green()),
            channel(from.blue(), to.blue()));
    }

    // OKLab, which the private grounds are derived in rather than in sRGB: an
    // even step towards a colour has to look like an even step, and the chroma a
    // mix loses has to be nameable to put some of it back. The transform is
    // Ottosson's, and `scripts/import_terminal_theme.py` builds a theme from a
    // terminal's colours through the same transform, and takes the same rungs — so
    // a palette imported once and a palette rendered on every theme switch climb
    // alike. Only the climbing below is Omaweb's own, because only Omaweb knows
    // whether the first rung landed far enough from the window it was measured
    // from.
    struct Oklab {
        double lightness = 0.0;
        double greenRed = 0.0;
        double blueYellow = 0.0;
    };

    Oklab toOklab(const QColor &colour)
    {
        const auto red = toLinear(colour.redF());
        const auto green = toLinear(colour.greenF());
        const auto blue = toLinear(colour.blueF());
        const auto long_
            = std::cbrt(0.4122214708 * red + 0.5363325363 * green + 0.0514459929 * blue);
        const auto medium
            = std::cbrt(0.2119034982 * red + 0.6806995451 * green + 0.1073969566 * blue);
        const auto short_
            = std::cbrt(0.0883024619 * red + 0.2817188376 * green + 0.6299787005 * blue);
        return {
            0.2104542553 * long_ + 0.7936177850 * medium - 0.0040720468 * short_,
            1.9779984951 * long_ - 2.4285922050 * medium + 0.4505937099 * short_,
            0.0259040371 * long_ + 0.7827717662 * medium - 0.8086757660 * short_,
        };
    }

    QColor fromOklab(const Oklab &colour)
    {
        const auto cubed = [](double value) { return value * value * value; };
        const auto long_ = cubed(
            colour.lightness + 0.3963377774 * colour.greenRed + 0.2158037573 * colour.blueYellow);
        const auto medium = cubed(
            colour.lightness - 0.1055613458 * colour.greenRed - 0.0638541728 * colour.blueYellow);
        const auto short_ = cubed(
            colour.lightness - 0.0894841775 * colour.greenRed - 1.2914855480 * colour.blueYellow);
        // Out of gamut clamps per channel, as an sRGB display does with it.
        return QColor::fromRgb(
            fromLinear(4.0767416621 * long_ - 3.3077115913 * medium + 0.2309699292 * short_),
            fromLinear(-1.2684380046 * long_ + 2.6097574011 * medium - 0.3413193965 * short_),
            fromLinear(-0.0041960863 * long_ - 0.7034186147 * medium + 1.7076147010 * short_));
    }

    QColor mixedPerceptually(const QColor &from, const QColor &to, double amount)
    {
        const auto start = toOklab(from);
        const auto end = toOklab(to);
        const auto channel
            = [amount](double one, double other) { return one + (other - one) * amount; };
        return fromOklab({ channel(start.lightness, end.lightness),
            channel(start.greenRed, end.greenRed), channel(start.blueYellow, end.blueYellow) });
    }

    QColor scaledChroma(const QColor &colour, double gain)
    {
        const auto oklab = toOklab(colour);
        return fromOklab({ oklab.lightness, oklab.greenRed * gain, oklab.blueYellow * gain });
    }

    // How far apart two colours are as the palette measures a private surface
    // against its ordinary counterpart: the straight line between them in OKLab,
    // so a cast of colour at the same lightness counts for what the reader
    // actually sees. Measured in RGB it barely counts at all near a desktop's
    // black, where the only way to move far enough is to make the private window
    // paler than anything else in the theme.
    double perceptualDistance(const QColor &one, const QColor &other)
    {
        const auto first = toOklab(one);
        const auto second = toOklab(other);
        const auto lightness = first.lightness - second.lightness;
        const auto greenRed = first.greenRed - second.greenRed;
        const auto blueYellow = first.blueYellow - second.blueYellow;
        return std::sqrt(lightness * lightness + greenRed * greenRed + blueYellow * blueYellow);
    }

    // A glance has to tell a private window from an ordinary one, so this is how
    // far its colour has to be from its counterpart before the two are different
    // windows rather than the same window in two lights. Deliberately a low bar:
    // the ground is not the only thing saying which window this is — the private
    // accent, the mask on the sidebar and the window's own title say it too — and
    // a floor high enough to carry that on its own would have to make the private
    // chrome paler than anything else in the reader's theme.
    constexpr auto minimumPrivateDifference = 0.04;

    // A private window is the reader's own chrome with a private cast on it
    // rather than a palette of its own, so every ground it is drawn on is the
    // ordinary ground it stands in for, pulled towards the colour that says the
    // window is private. That is what keeps a dark desktop's private window dark
    // and a light one light. Deriving each ground by mixing the window towards
    // the accent instead — a ladder of its own — climbs towards the accent's
    // lightness rather than the theme's, and hands a desktop whose window is
    // nearly black a browser several shades paler than everything around it.
    //
    // A theme is not asked for the grounds: a palette rendered from a desktop's
    // sixteen terminal colours has nothing to say about the surfaces under its
    // magenta, and a desktop that named none used to be handed Omaweb's own
    // purple, which is a browser that has stopped following the desktop on the
    // windows the reader most wants to recognise. A theme that does name one
    // keeps it.
    QColor privateTinted(const QColor &ground, const QColor &accent, double amount)
    {
        // A straight mix lands greyer than either end, so the result keeps a
        // little more chroma than the mix gives it, or a muted desktop's cast
        // washes out to the grey it was mixed from.
        constexpr auto chromaGain = 1.15;
        return scaledChroma(mixedPerceptually(ground, accent, amount), chromaGain);
    }

    // One amount for every ground, so the private surfaces keep the spacing the
    // theme gave the ordinary ones. This is the strength a private window is
    // tinted at: enough to be seen as a cast rather than as a rendering fault,
    // and light enough that the chrome keeps the darkness the theme drew it in.
    // Pulling harder lifts every ground towards the accent's own lightness, and a
    // private window as bright as the desktop behind it stops reading as a window
    // at all.
    constexpr auto privateTintStrength = 0.12;

    // Where a theme's private accent is so close to its window that the tint
    // cannot be seen at all, the tint strengthens until it can — as far as the
    // accent itself, past which there is nothing left to pull towards. A palette
    // with no private hue to speak of has the difference enforced after this
    // instead.
    double privateTintAmount(const QColor &window, const QColor &accent)
    {
        if (perceptualDistance(window, privateTinted(window, accent, privateTintStrength))
            >= minimumPrivateDifference) {
            return privateTintStrength;
        }
        constexpr auto steps = 256;
        for (auto step = 1; step <= steps; ++step) {
            const auto amount = privateTintStrength + (1.0 - privateTintStrength) * step / steps;
            if (perceptualDistance(window, privateTinted(window, accent, amount))
                >= minimumPrivateDifference) {
                return amount;
            }
        }
        return 1.0;
    }

    // The colour a role settles on: what the theme named where that already
    // reads on every surface the role is drawn on, and otherwise the nearest
    // colour that does. Some palettes have no such colour to give — nothing is
    // 3:1 against both a near-black window and a saturated sidebar — and there
    // the role takes whichever candidate reads best on the surface it reads worst
    // on. A palette is never rejected over this. A theme that cannot be repaired
    // exactly is still the theme the reader chose, and dropping every colour it
    // named to reach a floor on one role costs the reader more than the role is
    // worth: it is the difference between quiet text a shade faint and a browser
    // that has stopped following the desktop.
    QColor adjustedForContrast(const QColor &preferred, const QColor &fallback,
        const QList<QColor> &grounds, double minimumContrast, bool preserveHue)
    {
        const auto worstContrast = [&grounds](const QColor &candidate) {
            auto worst = std::numeric_limits<double>::max();
            for (const auto &ground : grounds) {
                worst = std::min(worst, contrastRatio(candidate, ground));
            }
            return worst;
        };
        if (grounds.isEmpty() || worstContrast(preferred) >= minimumContrast) {
            return preferred;
        }

        // Both answers are carried through the search, because which one is
        // wanted is not known until it ends: the clearing candidate that moved
        // least from what the theme asked for, and — for a palette where nothing
        // clears — the one that reads best on its worst surface. How far a
        // candidate moved is the search's own measure, so each search below hands
        // it whatever "least moved" means for the candidates it generates.
        auto best = preferred;
        auto bestWorst = worstContrast(preferred);
        QColor repaired;
        auto repairedDistance = std::numeric_limits<double>::max();
        const auto consider = [&](const QColor &candidate, double distance) {
            if (!candidate.isValid()) {
                return;
            }
            const auto worst = worstContrast(candidate);
            if (worst >= minimumContrast && distance < repairedDistance) {
                repaired = candidate;
                repairedDistance = distance;
            }
            if (worst > bestWorst) {
                bestWorst = worst;
                best = candidate;
            }
        };

        constexpr auto steps = 256;
        if (preserveHue && preferred.hslSaturationF() > 0.0) {
            for (auto step = 1; step < steps; ++step) {
                const auto lightness = static_cast<double>(step) / steps;
                const QColor candidate(
                    QColor::fromHslF(preferred.hslHueF(), preferred.hslSaturationF(), lightness)
                        .name(QColor::HexRgb));
                consider(candidate, std::abs(lightness - preferred.lightnessF()));
            }
            return repaired.isValid() ? repaired : best;
        }

        // Towards the colour the role belongs with first, and only then towards
        // black or white. The offset keeps every blend of the pair ahead of every
        // blend of an endpoint, however far each had to travel.
        for (auto step = 1; step <= steps; ++step) {
            const auto amount = static_cast<double>(step) / steps;
            consider(blended(preferred, fallback, amount), amount);
        }
        for (auto step = 1; step <= steps; ++step) {
            const auto amount = static_cast<double>(step) / steps;
            for (const auto &endpoint : { QColor(Qt::black), QColor(Qt::white) }) {
                consider(blended(preferred, endpoint, amount), 1.0 + amount);
            }
        }
        return repaired.isValid() ? repaired : best;
    }

} // namespace

ThemeController::ThemeController(QString themePath, QObject *parent)
    : ThemeController(QStringList { std::move(themePath) }, parent)
{
}

ThemeController::ThemeController(QStringList themePaths, QObject *parent)
    : QObject(parent)
    , m_themePaths(std::move(themePaths))
{
    const auto sourceChanged = [this] {
        reload();
        refreshWatchPaths();
        m_sourceState = themeSourceState();
    };
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, sourceChanged);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, sourceChanged);
    refreshWatchPaths();
    reload();
    m_sourceState = themeSourceState();

    // QFileSystemWatcher can miss directory creation and symlink replacement
    // on some hosts. Polling metadata keeps live theme changes reliable while
    // file-system events remain the fast path.
    m_sourcePoll.setInterval(500);
    connect(&m_sourcePoll, &QTimer::timeout, this, [this, sourceChanged] {
        if (const auto state = themeSourceState(); state != m_sourceState) {
            sourceChanged();
        }
    });
    m_sourcePoll.start();
}

QVariantMap ThemeController::palette() const { return m_palette; }

QStringList ThemeController::themeSourceState() const
{
    QStringList state;
    state.reserve(m_themePaths.size());
    for (const auto &path : m_themePaths) {
        const QFileInfo info(path);
        state.append(QStringLiteral("%1\n%2\n%3\n%4")
                .arg(info.exists() ? QStringLiteral("1") : QStringLiteral("0"),
                    info.canonicalFilePath(), QString::number(info.size()),
                    QString::number(info.lastModified().toMSecsSinceEpoch())));
    }
    return state;
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
        emit themeReloaded();
        return;
    }
    apply(normalizedPalette(fallbackPalette()));
    emit themeReloaded();
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
        { QStringLiteral("window"), QStringLiteral("#16151d") },
        { QStringLiteral("sidebar"), QStringLiteral("#1d1b29") },
        { QStringLiteral("overlay"), QStringLiteral("#282634") },
        { QStringLiteral("surface"), QStringLiteral("#302e3d") },
        { QStringLiteral("surfaceHover"), QStringLiteral("#3d394e") },
        { QStringLiteral("text"), QStringLiteral("#f3f1fa") },
        { QStringLiteral("mutedText"), QStringLiteral("#aaa5b7") },
        { QStringLiteral("accent"), QStringLiteral("#9b87ff") },
        { QStringLiteral("border"), QStringLiteral("#4a4658") },
        // What the reader has to be told rather than shown: a binding that
        // could not be honoured, a surface that failed. Distinct from the
        // private accent, which says whose window this is and not that
        // something is wrong.
        { QStringLiteral("urgent"), QStringLiteral("#e06c75") },
        // The one private colour named here. The grounds it is drawn on are
        // the climb's, from this and the window above, so a palette that
        // names a private accent and no grounds — every palette a desktop
        // renders — gets the same private window this one does.
        { QStringLiteral("privateAccent"), QStringLiteral("#c678dd") },
        { QStringLiteral("font"), defaultFont() },
        { QStringLiteral("opacity"), defaultOpacity() },
        { QStringLiteral("syntax"), defaultSyntax() },
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
        { QStringLiteral("keyword"), QStringLiteral("#c678dd") },
        { QStringLiteral("string"), QStringLiteral("#98c379") },
        { QStringLiteral("number"), QStringLiteral("#d19a66") },
        { QStringLiteral("comment"), QStringLiteral("#7f7a8c") },
        { QStringLiteral("tag"), QStringLiteral("#e06c75") },
        { QStringLiteral("attribute"), QStringLiteral("#e5c07b") },
        { QStringLiteral("variable"), QStringLiteral("#e06c75") },
        { QStringLiteral("function"), QStringLiteral("#61afef") },
        { QStringLiteral("type"), QStringLiteral("#56b6c2") },
    };
}

QVariantMap ThemeController::defaultOpacity()
{
    return {
        { QStringLiteral("sidebar"), 0.95 },
        // A sheet is read against a webpage rather than against the desktop,
        // and a page's own contrast is unknown, so it lets more through than
        // the sidebar does: at the sidebar's own value a dark page shows as
        // nothing at all and the surface reads as solid.
        { QStringLiteral("sheet"), 0.92 },
        { QStringLiteral("overlay"), 0.96 },
        { QStringLiteral("window"), 0.0 },
    };
}

QVariantMap ThemeController::defaultFont()
{
    // Deliberately no "monospace" tail: that is a fontconfig alias rather than
    // a family, and the host's own fixed-pitch family is a better last resort
    // than asking Qt to substitute for a name it cannot find.
    return {
        { QStringLiteral("families"),
            QStringList { QStringLiteral("JetBrains Mono"), QStringLiteral("SF Mono"),
                QStringLiteral("Menlo"), QStringLiteral("DejaVu Sans Mono") } },
        { QStringLiteral("size"), 12 },
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
    return installed.isEmpty() ? QString {} : installed.constFirst();
}

QVariantMap ThemeController::normalizedPalette(QVariantMap palette) const
{
    // Asked before the defaults are filled in, because a colour Omaweb
    // supplied is not a colour the theme asked for.
    const auto themeNamedMutedText = palette.contains(QStringLiteral("mutedText"));
    const auto themeNamedBorder = palette.contains(QStringLiteral("border"));
    const auto themeNamedSeparator = palette.contains(QStringLiteral("separator"));
    const auto themeNamedSurfaceHover = palette.contains(QStringLiteral("surfaceHover"));
    const auto themeNamedPrivateWindow = palette.contains(QStringLiteral("privateWindow"));
    const auto themeNamedPrivateSidebar = palette.contains(QStringLiteral("privateSidebar"));
    const auto themeNamedPrivateSurface = palette.contains(QStringLiteral("privateSurface"));
    const auto themeNamedPrivateSurfaceHover
        = palette.contains(QStringLiteral("privateSurfaceHover"));
    const auto fallback = fallbackPalette();
    for (auto it = fallback.cbegin(); it != fallback.cend(); ++it) {
        if (!palette.contains(it.key())) {
            palette.insert(it.key(), it.value());
        }
    }

    // A theme is entitled to name its own private colours, and one that names
    // a private window its ordinary window is a theme whose private windows
    // cannot be recognised. So a colour too close to its counterpart is
    // replaced by the nearest thing Omaweb has that is far enough: the colour
    // this palette would have derived, and where even that cannot be told
    // apart — a private accent that is the ordinary window's own colour, so
    // there is no cast to be had -- that colour taken towards black or white
    // only as far as it has to go. Not to black or white outright: a palette
    // with no private hue to offer still has a lightness of its own, and a
    // white window on a dark desktop is not the reader's theme any more.
    const auto enforceDifference
        = [&palette](const QString &normalKey, const QString &privateKey, const QColor &derived) {
              const QColor normal(palette.value(normalKey).toString());
              const QColor named(palette.value(privateKey).toString());
              const auto clears = [&normal](const QColor &candidate) {
                  return normal.isValid() && candidate.isValid()
                      && perceptualDistance(normal, candidate) >= minimumPrivateDifference;
              };
              if (clears(named)) {
                  return;
              }
              if (clears(derived)) {
                  palette.insert(privateKey, derived.name(QColor::HexRgb));
                  return;
              }

              const auto from = derived.isValid() ? derived : named;
              if (!from.isValid() || !normal.isValid()) {
                  return;
              }
              constexpr auto steps = 256;
              for (auto step = 1; step <= steps; ++step) {
                  const auto amount = static_cast<double>(step) / steps;
                  for (const auto &endpoint : { QColor(Qt::black), QColor(Qt::white) }) {
                      const auto candidate = blended(from, endpoint, amount);
                      if (clears(candidate)) {
                          palette.insert(privateKey, candidate.name(QColor::HexRgb));
                          return;
                      }
                  }
              }
          };

    // Before the tint, which pulls towards the private accent: a private
    // window cast in the colour the ordinary chrome is already accented with
    // says nothing about which window it is.
    enforceDifference(QStringLiteral("accent"), QStringLiteral("privateAccent"),
        QColor(fallback.value(QStringLiteral("privateAccent")).toString()));

    // Resolved here rather than with the other surfaces below, because the
    // private hover fill is this colour tinted.
    if (!themeNamedSurfaceHover) {
        palette.insert(QStringLiteral("surfaceHover"), palette.value(QStringLiteral("surface")));
    }

    const QColor window(palette.value(QStringLiteral("window")).toString());
    const QColor privateAccent(palette.value(QStringLiteral("privateAccent")).toString());
    const auto tint = (window.isValid() && privateAccent.isValid())
        ? privateTintAmount(window, privateAccent)
        : 0.0;
    const auto tinted = [&palette, &privateAccent, tint](const QString &ordinaryKey) {
        const QColor ground(palette.value(ordinaryKey).toString());
        if (tint <= 0.0 || !ground.isValid() || !privateAccent.isValid()) {
            return QColor {};
        }
        return privateTinted(ground, privateAccent, tint);
    };
    const auto derivePrivateGround
        = [&palette, &tinted](const QString &privateKey, const QString &ordinaryKey) {
              const auto ground = tinted(ordinaryKey);
              if (ground.isValid()) {
                  palette.insert(privateKey, ground.name(QColor::HexRgb));
              }
          };
    if (!themeNamedPrivateWindow) {
        derivePrivateGround(QStringLiteral("privateWindow"), QStringLiteral("window"));
    }
    if (!themeNamedPrivateSidebar) {
        derivePrivateGround(QStringLiteral("privateSidebar"), QStringLiteral("sidebar"));
    }
    if (!themeNamedPrivateSurface) {
        derivePrivateGround(QStringLiteral("privateSurface"), QStringLiteral("surface"));
    }

    enforceDifference(QStringLiteral("window"), QStringLiteral("privateWindow"),
        tinted(QStringLiteral("window")));

    // A Omaweb surface that takes the whole page area — the shortcut sheet
    // summoned over a page, the settings page — is the same material as the
    // sidebar and takes its colour. Not its translucency: the sidebar is read
    // against the desktop and these are read against a webpage. A theme that
    // wants a colour of its own for them names one, and inheriting rather than
    // falling back to Omaweb's own dark is what keeps a light theme light.
    const auto inheritSidebarColor
        = [&palette](const QString &sheetKey, const QString &sidebarKey) {
              if (!palette.contains(sheetKey)) {
                  palette.insert(sheetKey, palette.value(sidebarKey));
              }
          };
    inheritSidebarColor(QStringLiteral("sheet"), QStringLiteral("sidebar"));
    inheritSidebarColor(QStringLiteral("privateSheet"), QStringLiteral("privateSidebar"));
    inheritSidebarColor(QStringLiteral("privateOverlay"), QStringLiteral("privateSidebar"));
    if (!themeNamedPrivateSurfaceHover) {
        // A theme that named the private surface but not the fill over it
        // gets the surface, as the ordinary pair does. A theme that named
        // neither gets the ordinary fill tinted, which keeps the difference
        // the theme itself drew between a surface and the fill over it.
        if (themeNamedPrivateSurface) {
            palette.insert(QStringLiteral("privateSurfaceHover"),
                palette.value(QStringLiteral("privateSurface")));
        } else {
            derivePrivateGround(
                QStringLiteral("privateSurfaceHover"), QStringLiteral("surfaceHover"));
        }
    }

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
    // named colour changes only in lightness until it clears 4.5:1 — WCAG AA
    // for body text — against every Omaweb surface muted text is read on. A
    // theme whose colour already reads keeps it exactly; the rest keep their
    // hue and lose only the part that was unreadable. A theme that names no
    // muted text starts from the surface instead of from `text`, so what it
    // gets is the quietest tint of its own text colour that still reads, which
    // is dark on a light theme and light on a dark one without either being
    // special-cased.
    //
    // A disabled control is `text` at 0.35 alpha, which cannot reach 3:1
    // against the ground it is composited over, so anything clearing this
    // floor also stays clearly ahead of disabled. One threshold does both jobs.
    const QColor text(palette.value(QStringLiteral("text")).toString());
    const auto coloursFor = [&palette](std::initializer_list<QString> keys) {
        QList<QColor> colours;
        for (const auto &key : keys) {
            const QColor colour(palette.value(key).toString());
            if (colour.isValid()) {
                colours.append(colour);
            }
        }
        return colours;
    };
    const auto grounds = coloursFor({ QStringLiteral("sidebar"), QStringLiteral("surface"),
        QStringLiteral("surfaceHover"), QStringLiteral("overlay"), QStringLiteral("sheet") });
    const auto privateGrounds = coloursFor({ QStringLiteral("privateSidebar"),
        QStringLiteral("privateSurface"), QStringLiteral("privateSurfaceHover"),
        QStringLiteral("privateOverlay"), QStringLiteral("privateSheet") });
    const QColor named(palette.value(QStringLiteral("mutedText")).toString());
    const auto hasNamedMutedText = themeNamedMutedText && named.isValid();
    const QColor quietest(
        hasNamedMutedText ? named : QColor(palette.value(QStringLiteral("sidebar")).toString()));
    if (text.isValid() && quietest.isValid() && !grounds.isEmpty()) {
        constexpr auto minimumContrast = 4.5;
        const auto resolved
            = adjustedForContrast(quietest, text, grounds, minimumContrast, hasNamedMutedText);
        palette.insert(QStringLiteral("mutedText"), resolved.name(QColor::HexRgb));

        const QColor privateQuietest(hasNamedMutedText
                ? named
                : QColor(palette.value(QStringLiteral("privateSidebar")).toString()));
        const auto privateResolved = adjustedForContrast(
            privateQuietest, text, privateGrounds, minimumContrast, hasNamedMutedText);
        palette.insert(QStringLiteral("privateMutedText"), privateResolved.name(QColor::HexRgb));
    }

    // The grounds a border is actually drawn on, which is every Omaweb surface
    // except a hover fill: the rules and frames this role paints sit on a
    // surface at rest, and a control that does have an edge while the pointer
    // is over it takes that edge from the kit's own text-and-accent spec
    // rather than from here. Asking the role to clear a hover fill as well
    // costs the theme its colour for nothing — a palette naming one colour for
    // both, as the template Omarchy renders does, can never be 3:1 against
    // itself, so the repair walked every such border up to near-white.
    const auto borderedSurfaces = coloursFor({ QStringLiteral("window"), QStringLiteral("sidebar"),
        QStringLiteral("surface"), QStringLiteral("overlay"), QStringLiteral("sheet") });
    const auto privateBorderedSurfaces = coloursFor({ QStringLiteral("privateWindow"),
        QStringLiteral("privateSidebar"), QStringLiteral("privateSurface"),
        QStringLiteral("privateOverlay"), QStringLiteral("privateSheet") });
    const QColor namedBorder(palette.value(QStringLiteral("border")).toString());
    const auto hasNamedBorder = themeNamedBorder && namedBorder.isValid();
    const QColor faintestBorder(
        hasNamedBorder ? namedBorder : QColor(palette.value(QStringLiteral("sidebar")).toString()));
    if (text.isValid() && faintestBorder.isValid() && !borderedSurfaces.isEmpty()) {
        constexpr auto minimumBorderContrast = 3.0;
        const auto resolved = adjustedForContrast(
            faintestBorder, text, borderedSurfaces, minimumBorderContrast, hasNamedBorder);
        palette.insert(QStringLiteral("border"), resolved.name(QColor::HexRgb));

        const QColor faintestPrivateBorder(hasNamedBorder
                ? namedBorder
                : QColor(palette.value(QStringLiteral("privateSidebar")).toString()));
        const auto privateResolved = adjustedForContrast(faintestPrivateBorder, text,
            privateBorderedSurfaces, minimumBorderContrast, hasNamedBorder);
        palette.insert(QStringLiteral("privateBorder"), privateResolved.name(QColor::HexRgb));
    }

    // A rule inside a surface is not a frame around one, and the kit does not
    // draw them alike: its panel dividers are the foreground colour at a low
    // alpha, while a control's edge is that colour at a much higher one. That
    // is why the bar's separators read quieter than anything the border role
    // can give, and Omaweb's hairlines are those same dividers — the seam
    // down the sidebar, the rule above a browsing identity, the bands in a
    // panel. They take the kit's grammar rather than the border colour, and
    // the strength mirrors `strength` in
    // third_party/omarchy-shell/qs/Ui/PanelSeparator.qml.
    //
    // Deliberately below every contrast floor above: a divider that clears
    // 3:1 is a frame, and the reader ends up with a browser drawn in boxes.
    // A theme that names its own rule colour keeps it, alpha and all.
    if (text.isValid() && !themeNamedSeparator) {
        constexpr auto separatorStrength = 0.12;
        auto separator = text;
        separator.setAlphaF(separatorStrength);
        palette.insert(QStringLiteral("separator"), separator.name(QColor::HexArgb));
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
    families.removeAll(QString {});
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
        QVariantMap {
            { QStringLiteral("families"), families },
            { QStringLiteral("family"), installedFamily(families) },
            { QStringLiteral("size"), size },
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
    syntaxDefaults.insert(
        QStringLiteral("punctuation"), palette.value(QStringLiteral("mutedText")));
    const auto themeSyntax = palette.value(QStringLiteral("syntax")).toMap();
    QVariantMap syntax;
    for (auto it = syntaxDefaults.cbegin(); it != syntaxDefaults.cend(); ++it) {
        const QColor named(themeSyntax.value(it.key()).toString());
        syntax.insert(
            it.key(), named.isValid() ? named.name(QColor::HexRgb) : it.value().toString());
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
    withOpacity(QStringLiteral("privateOverlay"), overlayAlpha);
    return palette;
}

void ThemeController::refreshWatchPaths()
{
    // A palette compiled into the binary never changes, and the file system
    // has nothing to say about a resource path.
    const auto watchable = [](const QString &path) {
        return !path.isEmpty() && !path.startsWith(QLatin1Char(':')) && QFileInfo::exists(path);
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
