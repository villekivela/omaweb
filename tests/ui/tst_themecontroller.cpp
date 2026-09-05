#include "ThemeController.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMap>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <utility>

using omaweb::ThemeController;

class ThemeControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void enforcesDistinctPrivateColors();
    void tintsTheThemesOwnSurfacesTowardsThePrivateAccent();
    void tintsAPrivateWindowHarderWhenTheAccentBarelyRegisters();
    void keepsThePrivateGroundsTheSpacingTheThemeGaveTheOrdinaryOnes();
    void drawsThePrivatePaletteTheOmarchyTemplateRenders();
    void appliesSemanticOpacityToChromeSurfaces();
    void givesFullPageSurfacesTheSidebarsColourAndTheirOwnTranslucency();
    void namesOneColourForSomethingBeingWrong();
    void keepsQuietTextReadableOnEverySurfaceItIsDrawnOn();
    void keepsQuietTextReadableOnPrivateAndHoverSurfaces();
    void keepsQuietTextAheadOfADisabledControl();
    void quietensATextColourAThemeNamesNoMutedTextFor();
    void keepsAMutedColourAThemeGotRight();
    void preservesTheHueOfAMutedColourItRepairs();
    void keepsBordersVisibleOnEverySurfaceTheySeparate();
    void preservesTheHueOfABorderItRepairs();
    void keepsABorderAThemeAlsoNamedAsItsHoverFill();
    void drawsARuleAsQuietlyAsTheBarDraws();
    void keepsAPaletteWhoseSurfacesCannotShareReadableRoles();
    void keepsTheDesktopsOwnColoursWhenAPrivateSurfaceIsAnAccent();
    void reportsAThemeReloadWhenTheNormalizedPaletteDoesNotChange();
    void keepsASaturatedPaletteWhenBlackCanSupplyUnnamedRoles();
    void resolvesTheFirstInstalledTypeFamily();
    void fallsBackToAFamilyTheHostActuallyHas();
    void keepsTheTypeBaseSizeUsable();
    void namesTheColoursCodeIsReadIn();
    void readsTheFirstPaletteOfferedThatIsThere();
    void followsADesktopPaletteThatAppearsAfterStartup();
    void followsADesktopPaletteWhoseDirectoryAppearsAfterStartup();
    void followsADesktopThatSwitchesThemeByRelinking();
};

void ThemeControllerTest::enforcesDistinctPrivateColors()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#222222",
        "sidebar": "#333333",
        "surface": "#444444",
        "surfaceHover": "#555555",
        "text": "#eeeeee",
        "mutedText": "#aaaaaa",
        "accent": "#777777",
        "border": "#666666",
        "privateWindow": "#222222",
        "privateSidebar": "#333333",
        "privateSurface": "#444444",
        "privateSurfaceHover": "#555555",
        "privateAccent": "#777777"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    QVERIFY(QColor(palette.value(QStringLiteral("privateWindow")).toString())
        != QColor(palette.value(QStringLiteral("window")).toString()));
    QVERIFY(QColor(palette.value(QStringLiteral("privateAccent")).toString())
        != QColor(palette.value(QStringLiteral("accent")).toString()));
}

void ThemeControllerTest::appliesSemanticOpacityToChromeSurfaces()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#101010",
        "sidebar": "#ff202020",
        "overlay": "#303030",
        "opacity": { "sidebar": 0.5, "overlay": 2.0, "window": 0.25 }
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    QCOMPARE(QColor(palette.value(QStringLiteral("window")).toString()).alpha(), 64);
    QCOMPARE(QColor(palette.value(QStringLiteral("windowOpaque")).toString()),
        QColor(QStringLiteral("#101010")));
    // Alpha baked into the colour loses to the semantic value rather than compounding.
    QCOMPARE(QColor(palette.value(QStringLiteral("sidebar")).toString()).alpha(), 128);
    // Out-of-range opacity clamps instead of producing an invalid surface.
    QCOMPARE(QColor(palette.value(QStringLiteral("overlay")).toString()).alpha(), 255);
    QCOMPARE(palette.value(QStringLiteral("opacity"))
                 .toMap()
                 .value(QStringLiteral("overlay"))
                 .toDouble(),
        1.0);
}

// The contrast floor Omaweb holds muted text to, and the disabled treatment it
// has to stay ahead of, in one place: a disabled control is `text` at this
// alpha, and every muted-text test here reasons against the same numbers the
// interface draws with.
namespace {

constexpr auto minimumContrast = 4.5;
constexpr auto minimumGraphicContrast = 3.0;
constexpr auto disabledOpacity = 0.35;

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

// OKLab, and the two measurements the private palette is held to: how far
// apart two colours are, and how much colour a surface carries. The palette
// measures a private surface against its ordinary counterpart perceptually --
// an RGB measure barely counts a cast of colour near a desktop's black -- so
// the tests measure it the same way. The floor below is ThemeController's own.
constexpr auto minimumPrivateDifference = 0.04;

struct Oklab {
    double lightness = 0.0;
    double greenRed = 0.0;
    double blueYellow = 0.0;
};

Oklab oklab(const QColor &colour)
{
    const auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    const auto red = linear(colour.redF());
    const auto green = linear(colour.greenF());
    const auto blue = linear(colour.blueF());
    const auto long_ = std::cbrt(0.4122214708 * red + 0.5363325363 * green + 0.0514459929 * blue);
    const auto medium = std::cbrt(0.2119034982 * red + 0.6806995451 * green + 0.1073969566 * blue);
    const auto short_ = std::cbrt(0.0883024619 * red + 0.2817188376 * green + 0.6299787005 * blue);
    return {
        0.2104542553 * long_ + 0.7936177850 * medium - 0.0040720468 * short_,
        1.9779984951 * long_ - 2.4285922050 * medium + 0.4505937099 * short_,
        0.0259040371 * long_ + 0.7827717662 * medium - 0.8086757660 * short_,
    };
}

double perceptualDistance(const QColor &one, const QColor &other)
{
    const auto first = oklab(one);
    const auto second = oklab(other);
    const auto lightness = first.lightness - second.lightness;
    const auto greenRed = first.greenRed - second.greenRed;
    const auto blueYellow = first.blueYellow - second.blueYellow;
    return std::sqrt(lightness * lightness + greenRed * greenRed + blueYellow * blueYellow);
}

double chroma(const QColor &colour)
{
    const auto value = oklab(colour);
    return std::sqrt(value.greenRed * value.greenRed + value.blueYellow * value.blueYellow);
}

// What the reader actually sees where a control is drawn at reduced opacity:
// the colour composited over the surface behind it.
QColor composited(const QColor &colour, double alpha, const QColor &ground)
{
    const auto channel
        = [alpha](int over, int under) { return qRound(alpha * over + (1.0 - alpha) * under); };
    return QColor::fromRgb(channel(colour.red(), ground.red()),
        channel(colour.green(), ground.green()), channel(colour.blue(), ground.blue()));
}

} // namespace

// A theme names the colour that says a window is private; the grounds that
// colour is cast over are Omaweb's to derive, and each one is the ordinary
// ground it stands in for with the cast on it. So a private window is the
// reader's own chrome recognisably tinted rather than a palette of its own,
// which is what keeps a dark desktop's private window dark: derived by mixing
// the window towards the accent instead, every ground climbs towards the
// accent's own lightness and a near-black desktop gets a browser several
// shades paler than everything around it.
void ThemeControllerTest::tintsTheThemesOwnSurfacesTowardsThePrivateAccent()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#16151d",
        "sidebar": "#1d1b29",
        "overlay": "#282634",
        "surface": "#302e3d",
        "surfaceHover": "#3d394e",
        "text": "#f3f1fa",
        "accent": "#9b87ff",
        "privateAccent": "#c678dd"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    const auto colour = [&palette](const char *key) {
        return QColor(palette.value(QString::fromLatin1(key)).toString());
    };
    QCOMPARE(colour("privateWindowOpaque"), QColor(QStringLiteral("#291f32")));
    QCOMPARE(colour("privateSidebarOpaque"), QColor(QStringLiteral("#30243f")));
    QCOMPARE(colour("privateSurface"), QColor(QStringLiteral("#413651")));
    QCOMPARE(colour("privateSurfaceHover"), QColor(QStringLiteral("#4d4061")));

    const QColor privateAccent(palette.value(QStringLiteral("privateAccent")).toString());
    const QList<std::pair<const char *, const char *>> pairs {
        {"windowOpaque", "privateWindowOpaque"}, {"sidebarOpaque", "privateSidebarOpaque"},
        {"surface", "privateSurface"}, {"surfaceHover", "privateSurfaceHover"}};
    // The window is what the reader recognises the whole window by, so it is
    // the ground held to the difference. The surfaces inside it are never
    // seen beside their ordinary counterparts and only have to belong to the
    // same tinted family.
    QVERIFY(perceptualDistance(colour("windowOpaque"), colour("privateWindowOpaque"))
        >= minimumPrivateDifference);
    for (const auto &[ordinaryKey, privateKey] : pairs) {
        const auto ordinary = colour(ordinaryKey);
        const auto tinted = colour(privateKey);
        // Nearer its counterpart than the accent it was cast with: the
        // theme's own colour is what shows, and the accent is what tints it.
        QVERIFY2(perceptualDistance(ordinary, tinted) < perceptualDistance(tinted, privateAccent),
            privateKey);
        QVERIFY2(
            perceptualDistance(tinted, privateAccent) < perceptualDistance(ordinary, privateAccent),
            privateKey);
        // A ground is a ground: the alpha a surface is drawn at is the
        // semantic opacity's to say, and a derived colour carries none.
        QCOMPARE(tinted.alpha(), 255);
    }

    // A theme that does name a ground keeps it. The tint fills what is
    // missing rather than overruling what is there.
    QFile named(root.filePath(QStringLiteral("named.json")));
    QVERIFY(named.open(QIODevice::WriteOnly));
    named.write(R"JSON({
        "window": "#16151d",
        "text": "#f3f1fa",
        "accent": "#9b87ff",
        "privateAccent": "#c678dd",
        "privateSurface": "#503a20"
    })JSON");
    named.close();
    ThemeController namedColours(named.fileName());
    QCOMPARE(QColor(namedColours.palette().value(QStringLiteral("privateSurface")).toString()),
        QColor(QStringLiteral("#503a20")));
}

// A desktop whose private accent is barely off its own background has a cast
// nobody can see at the strength the rest of the palette is tinted at, so the
// tint strengthens until the two windows are different windows. It still
// comes from the desktop's own colour: a private window drawn in something
// Omaweb brought with it does not read as this desktop's private window.
void ThemeControllerTest::tintsAPrivateWindowHarderWhenTheAccentBarelyRegisters()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#12141a",
        "sidebar": "#181b22",
        "overlay": "#181b22",
        "surface": "#20242d",
        "surfaceHover": "#2c313c",
        "text": "#d8dbe3",
        "accent": "#7d8590",
        "privateAccent": "#211a24"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    const QColor window(palette.value(QStringLiteral("windowOpaque")).toString());
    const QColor privateWindow(palette.value(QStringLiteral("privateWindowOpaque")).toString());
    QVERIFY(perceptualDistance(window, privateWindow) >= minimumPrivateDifference);
    // Not black, not white, and not Omaweb's own private purple.
    QVERIFY(privateWindow != QColor(Qt::black));
    QVERIFY(privateWindow != QColor(Qt::white));
    QVERIFY(chroma(privateWindow) > chroma(window));
}

// The private grounds are the ordinary grounds tinted, so whatever the theme
// drew between a surface and the fill over it survives the tint. A hover fill
// the reader cannot tell from the surface under it is the symptom the palette
// is here to avoid.
void ThemeControllerTest::keepsThePrivateGroundsTheSpacingTheThemeGaveTheOrdinaryOnes()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#090a0e",
        "sidebar": "#0d0f14",
        "overlay": "#0d0f14",
        "surface": "#1a1d26",
        "surfaceHover": "#7e8892",
        "text": "#d8dbe3",
        "accent": "#9aa3ad",
        "privateAccent": "#8a5a62"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    const auto colour = [&palette](const char *key) {
        return QColor(palette.value(QString::fromLatin1(key)).toString());
    };
    const QList<std::pair<const char *, const char *>> pairs {
        {"windowOpaque", "privateWindowOpaque"}, {"sidebarOpaque", "privateSidebarOpaque"},
        {"surface", "privateSurface"}, {"surfaceHover", "privateSurfaceHover"}};
    for (auto index = 1; index < pairs.size(); ++index) {
        const auto [previousOrdinary, previousPrivate] = pairs.at(index - 1);
        const auto [ordinaryKey, privateKey] = pairs.at(index);
        // Every step the theme drew between two of its own grounds is still
        // there between their private counterparts, and in the same
        // direction, so the private chrome reads as the same chrome.
        const auto ordinaryStep
            = oklab(colour(ordinaryKey)).lightness - oklab(colour(previousOrdinary)).lightness;
        const auto privateStep
            = oklab(colour(privateKey)).lightness - oklab(colour(previousPrivate)).lightness;
        QVERIFY2(ordinaryStep * privateStep > 0.0, privateKey);
        QVERIFY2(std::abs(privateStep) >= std::abs(ordinaryStep) * 0.5, privateKey);
    }
}

// The palette an Omarchy desktop actually hands Omaweb, rendered from the
// template the repository ships through the colours of a real theme. The
// template is substituted here rather than mocked: a colour name Omarchy does
// not define leaves its own token in the output, and a palette that is not
// JSON is a browser that has stopped following the desktop.
void ThemeControllerTest::drawsThePrivatePaletteTheOmarchyTemplateRenders()
{
    QFile shipped(QStringLiteral(OMAWEB_OMARCHY_TEMPLATE_PATH));
    QVERIFY(shipped.open(QIODevice::ReadOnly));
    auto rendered = QString::fromUtf8(shipped.readAll());
    shipped.close();

    // Omarchy's own `colors.toml` names, with the values of a muted desktop
    // theme. Every name a template may spend is defined here, so a token left
    // in the output is a template naming something the desktop does not.
    const QMap<QString, QString> colours {
        {QStringLiteral("accent"), QStringLiteral("#9aa3ad")},
        {QStringLiteral("selection"), QStringLiteral("#2a3038")},
        {QStringLiteral("muted"), QStringLiteral("#7e8892")},
        {QStringLiteral("background"), QStringLiteral("#12141a")},
        {QStringLiteral("dark_background"), QStringLiteral("#0d0f14")},
        {QStringLiteral("darker_background"), QStringLiteral("#090a0e")},
        {QStringLiteral("lighter_background"), QStringLiteral("#1a1d26")},
        {QStringLiteral("foreground"), QStringLiteral("#d8dbe3")},
        {QStringLiteral("dark_foreground"), QStringLiteral("#929ca6")},
        {QStringLiteral("light_foreground"), QStringLiteral("#e4e7ee")},
        {QStringLiteral("bright_foreground"), QStringLiteral("#f0f2f6")},
        {QStringLiteral("cursor"), QStringLiteral("#d8dbe3")},
        {QStringLiteral("red"), QStringLiteral("#7a2e2e")},
        {QStringLiteral("orange"), QStringLiteral("#8a6a5c")},
        {QStringLiteral("yellow"), QStringLiteral("#c5c0b4")},
        {QStringLiteral("green"), QStringLiteral("#7a8f88")},
        {QStringLiteral("cyan"), QStringLiteral("#8a9aaa")},
        {QStringLiteral("blue"), QStringLiteral("#8296ac")},
        {QStringLiteral("magenta"), QStringLiteral("#8a5a62")},
        {QStringLiteral("brown"), QStringLiteral("#4a4040")},
        {QStringLiteral("font_family"), QStringLiteral("CaskaydiaMono Nerd Font")},
    };
    for (auto it = colours.cbegin(); it != colours.cend(); ++it) {
        rendered.replace(QStringLiteral("{{ %1 }}").arg(it.key()), it.value());
    }
    QVERIFY2(!rendered.contains(QStringLiteral("{{")),
        qPrintable(
            QStringLiteral("the template names a colour Omarchy does not: %1").arg(rendered)));

    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("omaweb.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(rendered.toUtf8());
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    // The desktop's own colours, which is the evidence the palette parsed at
    // all: a template that renders to something else leaves Omaweb's built-in
    // palette on show.
    QCOMPARE(QColor(palette.value(QStringLiteral("windowOpaque")).toString()),
        QColor(QStringLiteral("#090a0e")));

    // No surface carries an alpha of its own. A colour with an eight-digit
    // suffix is read as `#AARRGGBB`, so a template writing one gets neither
    // the alpha it asked for nor the colour it named.
    const QColor privateAccent(palette.value(QStringLiteral("privateAccent")).toString());
    QCOMPARE(privateAccent, QColor(QStringLiteral("#8a5a62")));
    for (const auto &key : {"surface", "surfaceHover", "privateSurface", "privateSurfaceHover",
             "windowOpaque", "sidebarOpaque", "privateWindowOpaque", "privateSidebarOpaque"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(ground.isValid(), key);
        QCOMPARE(ground.alpha(), 255);
    }

    // And every private ground is the desktop's own ground with the
    // desktop's own private accent cast over it, so the roles resolved
    // against them are not compromising between two unrelated hues.
    const QList<std::pair<const char *, const char *>> tintedPairs {
        {"windowOpaque", "privateWindowOpaque"}, {"sidebarOpaque", "privateSidebarOpaque"},
        {"surface", "privateSurface"}, {"surfaceHover", "privateSurfaceHover"}};
    QVERIFY(perceptualDistance(QColor(palette.value(QStringLiteral("windowOpaque")).toString()),
                QColor(palette.value(QStringLiteral("privateWindowOpaque")).toString()))
        >= minimumPrivateDifference);
    for (const auto &[ordinaryKey, privateKey] : tintedPairs) {
        const QColor ordinary(palette.value(QString::fromLatin1(ordinaryKey)).toString());
        const QColor tinted(palette.value(QString::fromLatin1(privateKey)).toString());
        QVERIFY2(perceptualDistance(ordinary, tinted) < perceptualDistance(tinted, privateAccent),
            privateKey);
        QVERIFY2(
            perceptualDistance(tinted, privateAccent) < perceptualDistance(ordinary, privateAccent),
            privateKey);
    }
    // Quiet text reads on every private ground at rest. The hover fill is
    // left out of this one because this desktop spends its own `muted` on it:
    // the ordinary palette has the same mid-grey fill over the same near-black
    // sidebar, so no colour is 4.5:1 against both, and the role takes the
    // compromise the palette promises rather than the floor.
    const QColor privateMuted(palette.value(QStringLiteral("privateMutedText")).toString());
    for (const auto &key : {"privateWindowOpaque", "privateSidebarOpaque", "privateSurface",
             "privateOverlayOpaque", "privateSheetOpaque"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(contrastRatio(privateMuted, ground) >= minimumContrast, key);
    }
    // A border is drawn on the surfaces at rest rather than on a hover fill,
    // which is the only ground it is not asked to clear.
    const QColor privateBorder(palette.value(QStringLiteral("privateBorder")).toString());
    for (const auto &key : {"privateWindowOpaque", "privateSidebarOpaque", "privateSurface",
             "privateOverlayOpaque", "privateSheetOpaque"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(contrastRatio(privateBorder, ground) >= minimumGraphicContrast, key);
    }
}

// Muted text is content — tab titles, Space letters, the footer's controls —
// so it holds WCAG AA for body text against every surface it is drawn on, not
// only against the one the theme's author happened to look at. A palette
// derived from a terminal offers ANSI bright black for it, which is a border
// colour, and this is the theme in the report that prompted the floor.
void ThemeControllerTest::keepsQuietTextReadableOnEverySurfaceItIsDrawnOn()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#0a0a0f",
        "sidebar": "#0e0e16",
        "overlay": "#0e0e16",
        "surface": "#13131d",
        "text": "#c8c8c8",
        "mutedText": "#434353"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    const QColor muted(palette.value(QStringLiteral("mutedText")).toString());
    QVERIFY(muted.isValid());
    // `surface` carries no semantic opacity, so it has no opaque variant to
    // read; the surfaces that do are checked underneath their translucency,
    // which is the colour the text is actually drawn over.
    for (const auto &key : {"sidebarOpaque", "surface", "overlayOpaque", "sheetOpaque"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(ground.isValid(), key);
        QVERIFY2(contrastRatio(muted, ground) >= minimumContrast, key);
    }
    // Quieter than the text it was taken from, or the floor has cost the
    // palette the distinction it exists to draw.
    QVERIFY(contrastRatio(muted, QColor(palette.value(QStringLiteral("sidebarOpaque")).toString()))
        < contrastRatio(QColor(palette.value(QStringLiteral("text")).toString()),
            QColor(palette.value(QStringLiteral("sidebarOpaque")).toString())));
}

void ThemeControllerTest::keepsQuietTextReadableOnPrivateAndHoverSurfaces()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "sidebar": "#101010",
        "overlay": "#101010",
        "surface": "#101010",
        "surfaceHover": "#65486f",
        "text": "#ffffff",
        "mutedText": "#888888",
        "privateSidebar": "#65486f",
        "privateSurface": "#65486f",
        "privateSurfaceHover": "#765780"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    const QColor muted(palette.value(QStringLiteral("mutedText")).toString());
    for (const auto &key :
        {"sidebarOpaque", "surface", "surfaceHover", "overlayOpaque", "sheetOpaque"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(ground.isValid(), key);
        QVERIFY2(contrastRatio(muted, ground) >= minimumContrast, key);
    }
    const QColor privateMuted(palette.value(QStringLiteral("privateMutedText")).toString());
    for (const auto &key : {"privateSidebarOpaque", "privateSurface", "privateSurfaceHover",
             "privateOverlayOpaque", "privateSheetOpaque"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(ground.isValid(), key);
        QVERIFY2(contrastRatio(privateMuted, ground) >= minimumContrast, key);
    }
}

// The defect the floor exists to prevent: muted text drawn fainter than the
// disabled rendering of ordinary text, so every quiet label reads as a control
// the reader cannot use. Checked on the theme that showed it and on a light
// theme, because the disabled composite moves the other way there.
void ThemeControllerTest::keepsQuietTextAheadOfADisabledControl()
{
    const QList<QPair<QByteArray, QByteArray>> themes {
        {QByteArrayLiteral("dark"), QByteArrayLiteral(R"JSON({
            "sidebar": "#0e0e16", "overlay": "#0e0e16", "surface": "#13131d",
            "text": "#c8c8c8", "mutedText": "#434353"
        })JSON")},
        {QByteArrayLiteral("light"), QByteArrayLiteral(R"JSON({
            "sidebar": "#f5f5f5", "overlay": "#f5f5f5", "surface": "#c0c0c0",
            "text": "#000000", "mutedText": "#c0c0c0"
        })JSON")},
    };

    for (const auto &[name, contents] : themes) {
        QTemporaryDir root;
        QFile theme(root.filePath(QStringLiteral("theme.json")));
        QVERIFY(theme.open(QIODevice::WriteOnly));
        theme.write(contents);
        theme.close();

        ThemeController controller(theme.fileName());
        const auto palette = controller.palette();
        const QColor ground(palette.value(QStringLiteral("sidebarOpaque")).toString());
        const QColor muted(palette.value(QStringLiteral("mutedText")).toString());
        const auto disabled = composited(
            QColor(palette.value(QStringLiteral("text")).toString()), disabledOpacity, ground);
        QVERIFY2(contrastRatio(muted, ground) > contrastRatio(disabled, ground), name.constData());
    }
}

// A theme that names no muted text is not owed Omaweb's own: it gets the
// quietest tint of its own text colour that still reads, which keeps a light
// theme's quiet text dark without either theme being special-cased.
void ThemeControllerTest::quietensATextColourAThemeNamesNoMutedTextFor()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#ffffff",
        "sidebar": "#f5f5f5",
        "overlay": "#f5f5f5",
        "surface": "#eeeeee",
        "text": "#101010"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    const QColor muted(palette.value(QStringLiteral("mutedText")).toString());
    const QColor ground(palette.value(QStringLiteral("sidebarOpaque")).toString());
    QVERIFY(contrastRatio(muted, ground) >= minimumContrast);
    // Derived from the theme's text rather than from the palette Omaweb ships:
    // dark on a light theme, and quieter than the text it came from.
    QVERIFY(relativeLuminance(muted) < relativeLuminance(ground));
    QVERIFY(contrastRatio(muted, ground)
        < contrastRatio(QColor(palette.value(QStringLiteral("text")).toString()), ground));
}

// The floor repairs; it does not redecorate. A theme whose muted text already
// reads keeps the exact colour it named, hue and all.
void ThemeControllerTest::keepsAMutedColourAThemeGotRight()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#080e18",
        "sidebar": "#080e18",
        "overlay": "#080e18",
        "surface": "#101820",
        "text": "#e8eef7",
        "mutedText": "#b3bdcc"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    QCOMPARE(QColor(controller.palette().value(QStringLiteral("mutedText")).toString()),
        QColor(QStringLiteral("#b3bdcc")));
}

void ThemeControllerTest::preservesTheHueOfAMutedColourItRepairs()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "sidebar": "#101010",
        "overlay": "#101010",
        "surface": "#181818",
        "surfaceHover": "#202020",
        "text": "#d8e8c0",
        "mutedText": "#35105f",
        "privateSidebar": "#181818",
        "privateSurface": "#202020",
        "privateSurfaceHover": "#282828"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const QColor named(QStringLiteral("#35105f"));
    const QColor repaired(controller.palette().value(QStringLiteral("mutedText")).toString());
    QVERIFY(contrastRatio(repaired, QColor(QStringLiteral("#202020"))) >= minimumContrast);
    QVERIFY(std::abs(repaired.hslHueF() - named.hslHueF()) < 0.01);
}

void ThemeControllerTest::keepsBordersVisibleOnEverySurfaceTheySeparate()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#0a0a0f",
        "sidebar": "#0e0e16",
        "overlay": "#0e0e16",
        "surface": "#13131d",
        "surfaceHover": "#1b1b27",
        "text": "#d8d8df",
        "border": "#333333",
        "privateWindow": "#21172a",
        "privateSidebar": "#2b1d36",
        "privateSurface": "#35223f",
        "privateSurfaceHover": "#40294c"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    const QColor border(palette.value(QStringLiteral("border")).toString());
    // No hover fill among them: a rule or a frame is drawn on a surface at
    // rest, and the edge a control grows under the pointer is the kit's.
    for (const auto &key :
        {"windowOpaque", "sidebarOpaque", "surface", "overlayOpaque", "sheetOpaque"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(ground.isValid(), key);
        QVERIFY2(contrastRatio(border, ground) >= minimumGraphicContrast, key);
    }
    const QColor privateBorder(palette.value(QStringLiteral("privateBorder")).toString());
    for (const auto &key : {"privateWindowOpaque", "privateSidebarOpaque", "privateSurface",
             "privateOverlayOpaque", "privateSheetOpaque"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(ground.isValid(), key);
        QVERIFY2(contrastRatio(privateBorder, ground) >= minimumGraphicContrast, key);
    }
}

void ThemeControllerTest::preservesTheHueOfABorderItRepairs()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#101010",
        "sidebar": "#101010",
        "overlay": "#101010",
        "surface": "#181818",
        "surfaceHover": "#202020",
        "text": "#00ff00",
        "border": "#000080",
        "privateWindow": "#181818",
        "privateSidebar": "#181818",
        "privateSurface": "#202020",
        "privateSurfaceHover": "#282828"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const QColor named(QStringLiteral("#000080"));
    const QColor repaired(controller.palette().value(QStringLiteral("border")).toString());
    QVERIFY(contrastRatio(repaired, QColor(QStringLiteral("#181818"))) >= minimumGraphicContrast);
    QVERIFY(std::abs(repaired.hslHueF() - named.hslHueF()) < 0.01);
}

// The template Omarchy renders from names the desktop's one muted colour for
// both the hover fill and the border, and no colour is 3:1 against itself. A
// border asked to clear the fill it can never clear left every such theme
// drawing its rules and frames in near-white.
void ThemeControllerTest::keepsABorderAThemeAlsoNamedAsItsHoverFill()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#010304",
        "sidebar": "#030607",
        "overlay": "#030607",
        "surface": "#0e1719",
        "surfaceHover": "#617877",
        "text": "#c3d2d0",
        "border": "#617877",
        "privateWindow": "#030607",
        "privateSidebar": "#b58c99",
        "privateSurface": "#4a3d42",
        "privateSurfaceHover": "#617877"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    QCOMPARE(
        controller.palette().value(QStringLiteral("border")).toString(), QStringLiteral("#617877"));
}

// A divider is not a frame. The kit draws its panel separators as the
// foreground colour at a low alpha, so a rule in Omaweb's own chrome has to be
// that quiet too — in the border colour it read as the loudest thing on a
// sidebar whose whole point is the page beside it.
void ThemeControllerTest::drawsARuleAsQuietlyAsTheBarDraws()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#010304",
        "sidebar": "#030607",
        "surface": "#0e1719",
        "surfaceHover": "#617877",
        "text": "#c3d2d0",
        "border": "#617877"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const QColor separator(controller.palette().value(QStringLiteral("separator")).toString());
    QVERIFY(separator.isValid());
    QCOMPARE(separator.rgb(), QColor(QStringLiteral("#c3d2d0")).rgb());
    QCOMPARE(separator.alpha(), 31);

    // Quieter than the border it replaced, once each is drawn on the surface
    // they share. Alpha is the whole of the difference, so the comparison has
    // to be made on what the reader actually sees.
    const QColor sidebar(QStringLiteral("#030607"));
    const auto drawnOnTheSidebar = [&sidebar](const QColor &rule) {
        const auto amount = rule.alphaF();
        const auto channel
            = [amount](int over, int under) { return qRound(under + (over - under) * amount); };
        return QColor::fromRgb(channel(rule.red(), sidebar.red()),
            channel(rule.green(), sidebar.green()), channel(rule.blue(), sidebar.blue()));
    };
    const QColor border(controller.palette().value(QStringLiteral("border")).toString());
    QVERIFY(contrastRatio(drawnOnTheSidebar(separator), sidebar)
        < contrastRatio(drawnOnTheSidebar(border), sidebar));

    // A theme that has a rule colour of its own keeps it, alpha and all.
    QFile named(root.filePath(QStringLiteral("named.json")));
    QVERIFY(named.open(QIODevice::WriteOnly));
    named.write(R"JSON({
        "sidebar": "#030607",
        "text": "#c3d2d0",
        "separator": "#33ff8800"
    })JSON");
    named.close();

    ThemeController namedController(named.fileName());
    QCOMPARE(namedController.palette().value(QStringLiteral("separator")).toString(),
        QStringLiteral("#33ff8800"));
}

// Some palettes have no colour to give a role: nothing reads at 4.5:1 on both
// a black sidebar and a mid grey surface. The floor is a repair, not a
// gatekeeper, so the theme the reader chose stays on screen and the role takes
// whichever colour reads best on the surface it reads worst on.
void ThemeControllerTest::keepsAPaletteWhoseSurfacesCannotShareReadableRoles()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#010101",
        "sidebar": "#000000",
        "overlay": "#000000",
        "surface": "#777777",
        "surfaceHover": "#777777",
        "text": "#ffffff",
        "mutedText": "#800000"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    QCOMPARE(QColor(palette.value(QStringLiteral("windowOpaque")).toString()),
        QColor(QStringLiteral("#010101")));
    const QColor muted(palette.value(QStringLiteral("mutedText")).toString());
    QVERIFY(muted.isValid());
    // Better on the surface it reads worst on than the colour the theme named,
    // which is the whole of what an unsatisfiable floor can promise.
    const auto worst = [&muted](const QColor &named) {
        const QColor sidebar(QStringLiteral("#000000"));
        const QColor surface(QStringLiteral("#777777"));
        return std::min(contrastRatio(named, sidebar), contrastRatio(named, surface))
            < std::min(contrastRatio(muted, sidebar), contrastRatio(muted, surface));
    };
    QVERIFY(worst(QColor(QStringLiteral("#800000"))));
}

// A theme is entitled to draw its private windows in its accent outright, and
// then no colour is 3:1 against both a near-black window and that sidebar.
// That is a palette to honour rather than a broken theme: the desktop's own
// colours have to survive it, or the palette lands on Omaweb's built-in one
// and the browser stops following the desktop at all.
void ThemeControllerTest::keepsTheDesktopsOwnColoursWhenAPrivateSurfaceIsAnAccent()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#161616",
        "sidebar": "#1e1e1e",
        "overlay": "#1e1e1e",
        "surface": "#3c3836",
        "surfaceHover": "#665c54",
        "text": "#d4be98",
        "mutedText": "#7c6f64",
        "accent": "#7daea3",
        "border": "#665c54",
        "privateAccent": "#d3869b",
        "privateWindow": "#1e1e1e",
        "privateSidebar": "#d3869b",
        "privateSurface": "#d3869b",
        "privateSurfaceHover": "#dc93a6"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    QCOMPARE(QColor(palette.value(QStringLiteral("windowOpaque")).toString()),
        QColor(QStringLiteral("#161616")));
    QCOMPARE(QColor(palette.value(QStringLiteral("sidebarOpaque")).toString()),
        QColor(QStringLiteral("#1e1e1e")));
    QCOMPARE(QColor(palette.value(QStringLiteral("surface")).toString()),
        QColor(QStringLiteral("#3c3836")));
    QCOMPARE(QColor(palette.value(QStringLiteral("text")).toString()),
        QColor(QStringLiteral("#d4be98")));
    QCOMPARE(QColor(palette.value(QStringLiteral("accent")).toString()),
        QColor(QStringLiteral("#7daea3")));
    // The roles the floor governs still read everywhere they can: the public
    // palette is satisfiable, so nothing there is allowed to be a compromise.
    const QColor muted(palette.value(QStringLiteral("mutedText")).toString());
    for (const auto &key :
        {"sidebarOpaque", "surface", "surfaceHover", "overlayOpaque", "sheetOpaque"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(contrastRatio(muted, ground) >= minimumContrast, key);
    }
    QVERIFY(QColor(palette.value(QStringLiteral("privateMutedText")).toString()).isValid());
    QVERIFY(QColor(palette.value(QStringLiteral("privateBorder")).toString()).isValid());
}

// The kit reads the desktop's `shell.toml` when Omaweb tells it to, and what
// tells it is a reload rather than a colour moving: two themes can carry the
// same palette and different control chrome, and the desktop still switched.
void ThemeControllerTest::reportsAThemeReloadWhenTheNormalizedPaletteDoesNotChange()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#101010",
        "sidebar": "#181818",
        "overlay": "#181818",
        "surface": "#202020",
        "text": "#f0f0f0",
        "mutedText": "#9a9a9a"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto before = controller.palette();
    QSignalSpy paletteChanges(&controller, &ThemeController::paletteChanged);
    QSignalSpy themeReloads(&controller, &ThemeController::themeReloaded);

    // The same colours, written in a different order. A theme manager that
    // re-renders the palette for a theme whose colours happen to match leaves
    // Omaweb nothing to see, and the kit still has to be told.
    QVERIFY(theme.open(QIODevice::WriteOnly | QIODevice::Truncate));
    theme.write(R"JSON({
        "sidebar": "#181818",
        "window": "#101010",
        "text": "#f0f0f0",
        "surface": "#202020",
        "mutedText": "#9a9a9a",
        "overlay": "#181818"
    })JSON");
    theme.close();
    controller.reload();

    QCOMPARE(controller.palette(), before);
    QCOMPARE(paletteChanges.count(), 0);
    QCOMPARE(themeReloads.count(), 1);
}

void ThemeControllerTest::keepsASaturatedPaletteWhenBlackCanSupplyUnnamedRoles()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#00ff00",
        "sidebar": "#00ff00",
        "overlay": "#00ff00",
        "surface": "#00ff00",
        "surfaceHover": "#00ee00",
        "text": "#ffffff",
        "privateWindow": "#00aa00",
        "privateSidebar": "#00aa00",
        "privateSurface": "#00aa00",
        "privateSurfaceHover": "#009900"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    QCOMPARE(QColor(controller.palette().value(QStringLiteral("windowOpaque")).toString()),
        QColor(QStringLiteral("#00ff00")));
}

// The theme palette names the type families it prefers; only one of them is
// installed here, and that is the one the palette has to resolve to. Handing
// Qt a family the host does not have costs a font-alias sweep and draws in
// whatever face Qt picks instead.
// A surface that takes the whole page area is the sidebar's material, so a
// theme names its colour once. It is read against a webpage rather than against
// the desktop, so it does not inherit the sidebar's translucency: at that value
// a dark page shows through as nothing and the surface reads as solid.
void ThemeControllerTest::givesFullPageSurfacesTheSidebarsColourAndTheirOwnTranslucency()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#101010",
        "sidebar": "#f0f0f0",
        "overlay": "#e8e8e8",
        "surface": "#e8e8e8",
        "surfaceHover": "#e0e0e0",
        "privateSidebar": "#800080",
        "opacity": { "sidebar": 0.9, "sheet": 0.6 }
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();

    // A light theme's sheet is light: inheriting the theme's own sidebar rather
    // than falling back to Omaweb's dark is what makes that true.
    QCOMPARE(QColor(palette.value(QStringLiteral("sheetOpaque")).toString()),
        QColor(QStringLiteral("#f0f0f0")));
    QCOMPARE(QColor(palette.value(QStringLiteral("sheet")).toString()).alpha(), 153);
    QCOMPARE(QColor(palette.value(QStringLiteral("sidebar")).toString()).alpha(), 230);
    QCOMPARE(QColor(palette.value(QStringLiteral("privateSheetOpaque")).toString()),
        QColor(QStringLiteral("#800080")));
    QCOMPARE(QColor(palette.value(QStringLiteral("privateSheet")).toString()).alpha(), 153);

    // Naming one takes precedence over inheriting it.
    QFile named(root.filePath(QStringLiteral("named.json")));
    QVERIFY(named.open(QIODevice::WriteOnly));
    named.write(R"JSON({
        "sidebar": "#f0f0f0",
        "overlay": "#e8e8e8",
        "surface": "#e8e8e8",
        "surfaceHover": "#e0e0e0",
        "sheet": "#d8d8d8",
        "opacity": { "sheet": 0.6 }
    })JSON");
    named.close();

    ThemeController namedController(named.fileName());
    QCOMPARE(QColor(namedController.palette().value(QStringLiteral("sheetOpaque")).toString()),
        QColor(QStringLiteral("#d8d8d8")));
}

// The colour a notice and the mark that leads to it are both drawn in. It is
// not the private accent: that says whose window this is, not that something
// needs attention, and a theme is free to make them the same only on purpose.
void ThemeControllerTest::namesOneColourForSomethingBeingWrong()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({ "window": "#101010", "urgent": "#ff8800" })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    QCOMPARE(QColor(controller.palette().value(QStringLiteral("urgent")).toString()),
        QColor(QStringLiteral("#ff8800")));

    // A theme that names none still gets one, distinct from the private accent.
    QFile bare(root.filePath(QStringLiteral("bare.json")));
    QVERIFY(bare.open(QIODevice::WriteOnly));
    bare.write(R"JSON({ "window": "#101010" })JSON");
    bare.close();

    ThemeController fallback(bare.fileName());
    const auto palette = fallback.palette();
    const QColor urgent(palette.value(QStringLiteral("urgent")).toString());
    QVERIFY(urgent.isValid());
    QVERIFY(urgent != QColor(palette.value(QStringLiteral("privateAccent")).toString()));
    QVERIFY(urgent != QColor(palette.value(QStringLiteral("accent")).toString()));
}

void ThemeControllerTest::resolvesTheFirstInstalledTypeFamily()
{
    const auto installed = QFontDatabase::families();
    QVERIFY(!installed.isEmpty());
    const auto present = installed.constFirst();

    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(QStringLiteral(R"JSON({
        "font": { "families": ["No Such Family Ships With Anything", "%1"], "size": 13 }
    })JSON")
            .arg(present)
            .toUtf8());
    theme.close();

    ThemeController controller(theme.fileName());
    const auto font = controller.palette().value(QStringLiteral("font")).toMap();
    QCOMPARE(font.value(QStringLiteral("family")).toString(), present);
    QCOMPARE(font.value(QStringLiteral("size")).toInt(), 13);
}

void ThemeControllerTest::fallsBackToAFamilyTheHostActuallyHas()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "font": { "families": ["monospace", ""] }
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto font = controller.palette().value(QStringLiteral("font")).toMap();
    const auto family = font.value(QStringLiteral("family")).toString();
    // Whatever the fallback lands on, it is a family the host has.
    QVERIFY(QFontDatabase::hasFamily(family));
    // "monospace" is a fontconfig alias. macOS has no such family and a bare
    // container has no fonts to alias, so there the name must not survive as
    // the answer -- that is the substitution this test was written to catch.
    // A Linux host with fonts installed does report it as a family, and
    // honouring what the theme asked for is then the right answer.
    if (!QFontDatabase::hasFamily(QStringLiteral("monospace"))) {
        QVERIFY(family != QStringLiteral("monospace"));
    }
    // An empty candidate is not a family, and it must not become the answer.
    QCOMPARE(font.value(QStringLiteral("families")).toStringList(),
        QStringList {QStringLiteral("monospace")});
}

void ThemeControllerTest::keepsTheTypeBaseSizeUsable()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({ "font": { "size": 0 } })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto font = controller.palette().value(QStringLiteral("font")).toMap();
    QCOMPARE(font.value(QStringLiteral("size")).toInt(), 12);
    // A theme that says nothing about type still names a family to draw with.
    QVERIFY(!font.value(QStringLiteral("families")).toStringList().isEmpty());
}

// The inspector the engine supplies draws source, markup and stylesheets, and
// it draws them in Omaweb's colours rather than Chromium's. A theme that says
// nothing about code still names every one of them, because a token left
// unnamed would come back in whatever the frontend ships.
void ThemeControllerTest::namesTheColoursCodeIsReadIn()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#101010",
        "syntax": { "string": "#00ff00", "comment": "not a colour" }
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto syntax = controller.palette().value(QStringLiteral("syntax")).toMap();
    QCOMPARE(QColor(syntax.value(QStringLiteral("string")).toString()),
        QColor(QStringLiteral("#00ff00")));
    // A name that is not a colour is not a colour the inspector can be handed.
    QCOMPARE(QColor(syntax.value(QStringLiteral("comment")).toString()),
        QColor(QStringLiteral("#7f7a8c")));
    // Structure is quieter than the names between it, and the interface
    // already names how quiet that is.
    QCOMPARE(QColor(syntax.value(QStringLiteral("punctuation")).toString()),
        QColor(controller.palette().value(QStringLiteral("mutedText")).toString()));

    for (const auto &token : {"keyword", "string", "number", "comment", "tag", "attribute",
             "variable", "function", "type", "punctuation"}) {
        const QColor colour(syntax.value(QString::fromLatin1(token)).toString());
        QVERIFY2(colour.isValid(), token);
        // Code is read against a solid surface, so a token carries no alpha of
        // its own to blend the character it draws into the page behind it.
        QCOMPARE(colour.alpha(), 255);
    }
}

// Omaweb has more than one place a palette may come from -- an override, the
// reader's own configuration directory, the desktop's rendered theme, and the
// built-in -- and they are offered in that order rather than resolved once.
void ThemeControllerTest::readsTheFirstPaletteOfferedThatIsThere()
{
    QTemporaryDir root;
    const auto absent = root.filePath(QStringLiteral("missing/theme.json"));
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({ "window": "#101010", "opacity": { "window": 1.0 } })JSON");
    theme.close();

    ThemeController controller(QStringList {absent, theme.fileName()});
    QCOMPARE(QColor(controller.palette().value(QStringLiteral("window")).toString()),
        QColor(QStringLiteral("#101010")));
}

// A theme switch renders the desktop's palette while Omaweb is already
// running, and on an Omarchy machine the first render lands moments after
// startup. The palette a restart would have found has to arrive without one.
void ThemeControllerTest::followsADesktopPaletteThatAppearsAfterStartup()
{
    QTemporaryDir root;
    const auto desktop = root.filePath(QStringLiteral("desktop/theme.json"));
    QVERIFY(QDir().mkpath(QFileInfo(desktop).absolutePath()));
    QFile builtIn(root.filePath(QStringLiteral("built-in.json")));
    QVERIFY(builtIn.open(QIODevice::WriteOnly));
    builtIn.write(R"JSON({ "window": "#101010", "opacity": { "window": 1.0 } })JSON");
    builtIn.close();

    ThemeController controller(QStringList {desktop, builtIn.fileName()});
    QCOMPARE(QColor(controller.palette().value(QStringLiteral("window")).toString()),
        QColor(QStringLiteral("#101010")));

    QFile rendered(desktop);
    QVERIFY(rendered.open(QIODevice::WriteOnly));
    rendered.write(R"JSON({ "window": "#202020", "opacity": { "window": 1.0 } })JSON");
    rendered.close();

    QTRY_COMPARE(QColor(controller.palette().value(QStringLiteral("window")).toString()),
        QColor(QStringLiteral("#202020")));
}

// A desktop theme is rendered into a directory the theme manager creates, and
// on a machine that has never rendered Omaweb's palette that directory is not
// there when the browser starts. The deepest directory that does exist is
// watched, so each level appearing arms the one below it.
void ThemeControllerTest::followsADesktopPaletteWhoseDirectoryAppearsAfterStartup()
{
    QTemporaryDir root;
    const auto desktop = root.filePath(QStringLiteral("state/current/theme/theme.json"));
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("state"))));
    QFile builtIn(root.filePath(QStringLiteral("built-in.json")));
    QVERIFY(builtIn.open(QIODevice::WriteOnly));
    builtIn.write(R"JSON({ "window": "#101010", "opacity": { "window": 1.0 } })JSON");
    builtIn.close();

    ThemeController controller(QStringList {desktop, builtIn.fileName()});
    QCOMPARE(QColor(controller.palette().value(QStringLiteral("window")).toString()),
        QColor(QStringLiteral("#101010")));

    QVERIFY(QDir().mkpath(QFileInfo(desktop).absolutePath()));
    QFile rendered(desktop);
    QVERIFY(rendered.open(QIODevice::WriteOnly));
    rendered.write(R"JSON({ "window": "#303030", "opacity": { "window": 1.0 } })JSON");
    rendered.close();

    QTRY_COMPARE_WITH_TIMEOUT(
        QColor(controller.palette().value(QStringLiteral("window")).toString()),
        QColor(QStringLiteral("#303030")), 10000);
}

// How Omarchy switches themes: `current/theme` is a symlink, and `theme set`
// points it at another directory. A watch resolves through a symlink, so
// watching the rendered palette leaves the watch on the theme the reader just
// left -- the file it holds never changes again, and the palette under the same
// name is a different file. This is the website's own claim: run
// `omarchy theme set` and the browser changes colour.
void ThemeControllerTest::followsADesktopThatSwitchesThemeByRelinking()
{
    QTemporaryDir root;
    const auto first = root.filePath(QStringLiteral("themes/first"));
    const auto second = root.filePath(QStringLiteral("themes/second"));
    QVERIFY(QDir().mkpath(first));
    QVERIFY(QDir().mkpath(second));
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("current"))));

    const auto palette = [](const QString &directory, const QByteArray &window) {
        QFile file(QDir(directory).filePath(QStringLiteral("omaweb.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ \"window\": \"" + window + "\", \"opacity\": { \"window\": 1.0 } }");
    };
    palette(first, "#101010");
    palette(second, "#303030");

    const auto link = root.filePath(QStringLiteral("current/theme"));
    QVERIFY(QFile::link(first, link));

    QFile builtIn(root.filePath(QStringLiteral("built-in.json")));
    QVERIFY(builtIn.open(QIODevice::WriteOnly));
    builtIn.write(R"JSON({ "window": "#000000", "opacity": { "window": 1.0 } })JSON");
    builtIn.close();

    ThemeController controller(
        QStringList {QDir(link).filePath(QStringLiteral("omaweb.json")), builtIn.fileName()});
    QCOMPARE(QColor(controller.palette().value(QStringLiteral("window")).toString()),
        QColor(QStringLiteral("#101010")));

    QVERIFY(QFile::remove(link));
    QVERIFY(QFile::link(second, link));

    QTRY_COMPARE_WITH_TIMEOUT(
        QColor(controller.palette().value(QStringLiteral("window")).toString()),
        QColor(QStringLiteral("#303030")), 10000);
}

// QFontDatabase needs a GUI application, so this suite is no longer guiless.
QTEST_MAIN(ThemeControllerTest)

#include "tst_themecontroller.moc"
