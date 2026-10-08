#include "ThemeController.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <numbers>
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
    void drawsTheSidebarAtTheReadersOpacityOverTheThemes();
    void keepsTheReadersSidebarOpacityThroughAThemeReload();
    void handsTheSidebarBackToTheThemeWhenTheOverrideIsCleared();
    void givesFullPageSurfacesTheSidebarsColourAndTheirOwnTranslucency();
    void namesOneColourForSomethingBeingWrong();
    void keepsTheAgentAccentLegibleAndApartFromTheAccent();
    void fillsTheAgentAccentFromTheDesktopsCyan();
    void drawsTheSixSpaceColoursOmawebOwns();
    void keepsTheSpaceColoursApartInEveryStockTheme_data();
    void keepsTheSpaceColoursApartInEveryStockTheme();
    void movesASpaceColourJustClearOfAnAccent();
    void keepsTheTypedTextReadableOnTheOmnibarsGlassInEveryBundledTheme();
    void keepsQuietTextReadableOnEverySurfaceItIsDrawnOn();
    void keepsQuietTextReadableOnPrivateAndHoverSurfaces();
    void handsAPageTheQuietTextTheThemeNamed();
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
    void drawsPunctuationAboveACommentAndBothBelowTheCode();
    void liftsASyntaxColourTooDarkToReadWithoutChangingItsHue();
    void keepsAQuietColourAThemeChoseForItsComments();
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

namespace {

QString writeSidebarTheme(const QString &path, const QString &sidebar, double opacity)
{
    QFile theme(path);
    if (!theme.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    theme.write(QStringLiteral(R"JSON({
        "window": "#101010",
        "sidebar": "%1",
        "overlay": "#303030",
        "opacity": { "sidebar": %2, "sheet": 0.7, "overlay": 0.6, "window": 0.25 }
    })JSON")
            .arg(sidebar)
            .arg(opacity)
            .toUtf8());
    return path;
}

int alphaOf(const QVariantMap &palette, const char *key)
{
    return QColor(palette.value(QString::fromLatin1(key)).toString()).alpha();
}

} // namespace

void ThemeControllerTest::drawsTheSidebarAtTheReadersOpacityOverTheThemes()
{
    QTemporaryDir root;
    const auto path = writeSidebarTheme(
        root.filePath(QStringLiteral("theme.json")), QStringLiteral("#202020"), 0.8);
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);
    QCOMPARE(alphaOf(controller.palette(), "sidebar"), 204);
    QCOMPARE(controller.themeSidebarOpacity(), 0.8);
    const auto opaque = controller.palette().value(QStringLiteral("sidebarOpaque"));

    QSignalSpy changed(&controller, &ThemeController::paletteChanged);
    controller.setSidebarOpacity(0.5);

    const auto palette = controller.palette();
    QCOMPARE(changed.count(), 1);
    QCOMPARE(alphaOf(palette, "sidebar"), 128);
    QCOMPARE(alphaOf(palette, "privateSidebar"), 128);
    QCOMPARE(palette.value(QStringLiteral("sidebarOpaque")), opaque);
    // Only the sidebar is the reader's to set: the sheet, overlay and window keep the theme's.
    QCOMPARE(alphaOf(palette, "sheet"), 179);
    QCOMPARE(alphaOf(palette, "privateSheet"), 179);
    QCOMPARE(alphaOf(palette, "privateOverlay"), 153);
    QCOMPARE(alphaOf(palette, "overlay"), 153);
    QCOMPARE(alphaOf(palette, "window"), 64);
    QCOMPARE(alphaOf(palette, "privateWindow"), 64);
    QCOMPARE(controller.themeSidebarOpacity(), 0.8);
}

void ThemeControllerTest::keepsTheReadersSidebarOpacityThroughAThemeReload()
{
    QTemporaryDir root;
    const auto path = writeSidebarTheme(
        root.filePath(QStringLiteral("theme.json")), QStringLiteral("#202020"), 0.8);
    ThemeController controller(path);
    controller.setSidebarOpacity(0.5);

    writeSidebarTheme(path, QStringLiteral("#303030"), 0.6);
    controller.reload();

    QCOMPARE(alphaOf(controller.palette(), "sidebar"), 128);
    QCOMPARE(QColor(controller.palette().value(QStringLiteral("sidebarOpaque")).toString()),
        QColor(QStringLiteral("#303030")));
    QCOMPARE(controller.themeSidebarOpacity(), 0.6);
}

void ThemeControllerTest::handsTheSidebarBackToTheThemeWhenTheOverrideIsCleared()
{
    QTemporaryDir root;
    const auto path = writeSidebarTheme(
        root.filePath(QStringLiteral("theme.json")), QStringLiteral("#202020"), 0.8);
    ThemeController controller(path);
    controller.setSidebarOpacity(0.5);

    writeSidebarTheme(path, QStringLiteral("#202020"), 0.6);
    controller.reload();
    QCOMPARE(alphaOf(controller.palette(), "sidebar"), 128);
    controller.setSidebarOpacity(std::nullopt);

    QCOMPARE(alphaOf(controller.palette(), "sidebar"), 153);
    QCOMPARE(alphaOf(controller.palette(), "privateSidebar"), 153);
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

// OKLCH's hue, in degrees, and how far apart two hues are around the circle.
double hue(const QColor &colour)
{
    const auto value = oklab(colour);
    const auto degrees = std::atan2(value.blueYellow, value.greenRed) * 180.0 / std::numbers::pi;
    return degrees < 0.0 ? degrees + 360.0 : degrees;
}

double hueDistance(double one, double other)
{
    const auto apart = std::fmod(std::abs(one - other), 360.0);
    return std::min(apart, 360.0 - apart);
}

// The six in the order Settings offers them, and the gap ThemeController keeps
// between them and from the theme's urgent, Private and Agent colours. A grey
// has no hue to keep a gap from, so an accent below the chroma floor is exempt.
// The 20° gap is the one docs/product/requirements.md states for Space colours.
const QStringList spaceColourNames {QStringLiteral("orange"), QStringLiteral("yellow"),
    QStringLiteral("green"), QStringLiteral("teal"), QStringLiteral("blue"),
    QStringLiteral("violet")};
constexpr auto spaceHueGap = 20.0;
constexpr auto chromaticFloor = 0.03;

// What a Space colour is drawn on: every ground but a hover fill, a Private
// window's too.
const QStringList spaceGrounds {QStringLiteral("windowOpaque"), QStringLiteral("sidebarOpaque"),
    QStringLiteral("overlayOpaque"), QStringLiteral("sheetOpaque"),
    QStringLiteral("privateWindowOpaque"), QStringLiteral("privateSidebarOpaque"),
    QStringLiteral("privateOverlayOpaque"), QStringLiteral("privateSheetOpaque")};

// What a reader's machine renders from the template Omarchy ships, for a theme
// with these colours. A token the palette does not draw Spaces from is filled
// with the muted colour.
QString renderedOmarchyTheme(const QHash<QString, QString> &colours)
{
    QFile shipped(QStringLiteral(OMAWEB_OMARCHY_TEMPLATE_PATH));
    if (!shipped.open(QIODevice::ReadOnly)) {
        return {};
    }
    auto rendered = QString::fromUtf8(shipped.readAll());
    for (auto it = colours.cbegin(); it != colours.cend(); ++it) {
        rendered.replace(QStringLiteral("{{ %1 }}").arg(it.key()), it.value());
    }
    static const QRegularExpression token(QStringLiteral("\\{\\{ [a-z_]+ \\}\\}"));
    return rendered.replace(token, colours.value(QStringLiteral("muted")));
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
        // The bright half of the same palette. Omarchy keeps these in sync
        // with slots 9 to 14, and a theme that names none of them still has
        // them, so a template may spend one wherever an editor would.
        {QStringLiteral("bright_red"), QStringLiteral("#a04444")},
        {QStringLiteral("bright_yellow"), QStringLiteral("#ded8c8")},
        {QStringLiteral("bright_green"), QStringLiteral("#96aca4")},
        {QStringLiteral("bright_cyan"), QStringLiteral("#a4b6c6")},
        {QStringLiteral("bright_blue"), QStringLiteral("#9cb0c8")},
        {QStringLiteral("bright_magenta"), QStringLiteral("#a6737c")},
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

    // Omarchy renders colours and nothing else, so the template asks for type
    // by the name the desktop keeps the reader's own choice under: the
    // fontconfig alias `omarchy font set` writes. What the palette carries is
    // the family that alias stands for, because the kit is handed a face
    // rather than a name to resolve.
    const auto font = palette.value(QStringLiteral("font")).toMap();
    const auto family = font.value(QStringLiteral("family")).toString();
    QVERIFY2(QFontDatabase::hasFamily(family), qPrintable(family));
    QVERIFY2(family != QStringLiteral("monospace"), qPrintable(family));
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

// A page following the theme has one ground, so it takes the quiet text the
// theme named rather than the one floored for Omaweb's six; where the theme
// named none, the floored value is the only one there is.
void ThemeControllerTest::handsAPageTheQuietTextTheThemeNamed()
{
    QTemporaryDir root;
    QFile named(root.filePath(QStringLiteral("named.json")));
    QVERIFY(named.open(QIODevice::WriteOnly));
    named.write(R"JSON({
        "sidebar": "#101010",
        "overlay": "#101010",
        "surface": "#101010",
        "surfaceHover": "#65486f",
        "text": "#ffffff",
        "mutedText": "#888888"
    })JSON");
    named.close();
    const auto namedPalette = ThemeController(named.fileName()).palette();
    QCOMPARE(
        namedPalette.value(QStringLiteral("pageMutedText")).toString(), QStringLiteral("#888888"));
    QVERIFY(namedPalette.value(QStringLiteral("mutedText")).toString()
        != namedPalette.value(QStringLiteral("pageMutedText")).toString());

    QFile unnamed(root.filePath(QStringLiteral("unnamed.json")));
    QVERIFY(unnamed.open(QIODevice::WriteOnly));
    unnamed.write(R"JSON({
        "sidebar": "#101010",
        "overlay": "#101010",
        "surface": "#101010",
        "text": "#ffffff"
    })JSON");
    unnamed.close();
    const auto unnamedPalette = ThemeController(unnamed.fileName()).palette();
    QCOMPARE(unnamedPalette.value(QStringLiteral("pageMutedText")).toString(),
        unnamedPalette.value(QStringLiteral("mutedText")).toString());
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
    // "monospace" is a fontconfig alias rather than a family. macOS has no
    // such name at all and a bare container has no fonts to alias, so the
    // candidate is skipped there; a Linux host does report it, and answers
    // with the family it stands for. Either way the name a theme cannot draw
    // with does not reach the kit -- that is the substitution this test was
    // written to catch.
    QVERIFY2(family != QStringLiteral("monospace"), qPrintable(family));
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
    // A name that is not a colour is a token the theme did not name, so it is
    // derived rather than left for the frontend to draw in its own palette.
    QVERIFY(QColor(syntax.value(QStringLiteral("comment")).toString()).isValid());

    for (const auto &token : {"keyword", "string", "number", "comment", "tag", "attribute",
             "variable", "function", "type", "punctuation"}) {
        const QColor colour(syntax.value(QString::fromLatin1(token)).toString());
        QVERIFY2(colour.isValid(), token);
        // Code is read against a solid surface, so a token carries no alpha of
        // its own to blend the character it draws into the page behind it.
        QCOMPARE(colour.alpha(), 255);
    }
}

// The two quiet names are the theme's own text turned down, and how far down
// is what keeps them apart. A desktop palette has one quiet colour to give, so
// a theme asked for both spends it twice and every bracket in the inspector is
// drawn as dim as an aside.
void ThemeControllerTest::drawsPunctuationAboveACommentAndBothBelowTheCode()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#0e0e14",
        "sidebar": "#13141c",
        "surface": "#24283b",
        "text": "#a9b1d6",
        "mutedText": "#565f89"
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    const auto syntax = palette.value(QStringLiteral("syntax")).toMap();
    const QColor window(palette.value(QStringLiteral("windowOpaque")).toString());
    const QColor muted(palette.value(QStringLiteral("mutedText")).toString());
    const QColor comment(syntax.value(QStringLiteral("comment")).toString());
    const QColor punctuation(syntax.value(QStringLiteral("punctuation")).toString());

    QVERIFY(comment.isValid());
    QVERIFY(punctuation.isValid());
    QVERIFY(comment != punctuation);
    // Read against the window an aside recedes furthest, structure sits above
    // the quietest thing the interface asks anyone to read, and both stay
    // below the names between them.
    const QColor body(palette.value(QStringLiteral("text")).toString());
    QVERIFY(contrastRatio(comment, window) < contrastRatio(muted, window));
    QVERIFY(contrastRatio(muted, window) < contrastRatio(punctuation, window));
    QVERIFY(contrastRatio(punctuation, window) < contrastRatio(body, window));
}

// A slot the desktop never meant for code still has to be read. The colour
// keeps the hue the theme asked for and gives up only the lightness that made
// it unreadable, because a repaired token in a hue from nowhere is a worse
// answer than a dim one.
void ThemeControllerTest::liftsASyntaxColourTooDarkToReadWithoutChangingItsHue()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#0e0e14",
        "sidebar": "#0e0e14",
        "surface": "#0e0e14",
        "text": "#e8e8f0",
        "syntax": { "type": "#123423" }
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();
    const QColor window(palette.value(QStringLiteral("windowOpaque")).toString());
    const QColor type(
        palette.value(QStringLiteral("syntax")).toMap().value(QStringLiteral("type")).toString());

    QVERIFY(type.isValid());
    QVERIFY(contrastRatio(type, window) >= 4.5);
    QCOMPARE(qRound(type.hslHueF() * 360.0),
        qRound(QColor(QStringLiteral("#123423")).hslHueF() * 360.0));
}

// The floor is for a token that has become unreadable, not for one that is
// quiet by design. A theme that draws its comments at an aside's contrast
// meant to, and lifting them would draw every aside as loudly as the code.
void ThemeControllerTest::keepsAQuietColourAThemeChoseForItsComments()
{
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("theme.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(R"JSON({
        "window": "#0e0e14",
        "text": "#e8e8f0",
        "syntax": { "comment": "#3a3a44" }
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    QCOMPARE(QColor(controller.palette()
                     .value(QStringLiteral("syntax"))
                     .toMap()
                     .value(QStringLiteral("comment"))
                     .toString()),
        QColor(QStringLiteral("#3a3a44")));
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

// The Agent accent defaults to the terminal cyan Omaweb's own palette names,
// and on a light theme it darkens, keeping its hue, until the label written on
// it and the mark drawn on the sidebar read.
void ThemeControllerTest::keepsTheAgentAccentLegibleAndApartFromTheAccent()
{
    QTemporaryDir root;
    QFile dark(root.filePath(QStringLiteral("dark.json")));
    QVERIFY(dark.open(QIODevice::WriteOnly));
    dark.write(R"JSON({ "window": "#16151d", "sidebar": "#1d1b29", "text": "#f3f1fa" })JSON");
    dark.close();
    QCOMPARE(QColor(ThemeController(dark.fileName())
                     .palette()
                     .value(QStringLiteral("agentAccent"))
                     .toString()),
        QColor(QStringLiteral("#56b6c2")));

    QFile light(root.filePath(QStringLiteral("light.json")));
    QVERIFY(light.open(QIODevice::WriteOnly));
    light.write(R"JSON({
        "window": "#ffffff",
        "sidebar": "#f4f4f4",
        "text": "#1a1a1a",
        "accent": "#3b6fd6",
        "agentAccent": "#56b6c2"
    })JSON");
    light.close();
    const auto palette = ThemeController(light.fileName()).palette();
    const QColor agent(palette.value(QStringLiteral("agentAccent")).toString());
    for (const auto *key : {"window", "sidebar"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(contrastRatio(agent, ground) >= 4.5, key);
    }
    QVERIFY(std::abs(agent.hslHueF() - QColor(QStringLiteral("#56b6c2")).hslHueF()) < 0.02);

    // A theme whose accent is its cyan still tells the reader's selection from
    // an Agent's hands.
    QFile cyan(root.filePath(QStringLiteral("cyan.json")));
    QVERIFY(cyan.open(QIODevice::WriteOnly));
    cyan.write(R"JSON({
        "window": "#16151d",
        "sidebar": "#1d1b29",
        "text": "#f3f1fa",
        "accent": "#56b6c2",
        "agentAccent": "#56b6c2"
    })JSON");
    cyan.close();
    const auto cyanPalette = ThemeController(cyan.fileName()).palette();
    QVERIFY(QColor(cyanPalette.value(QStringLiteral("agentAccent")).toString())
        != QColor(cyanPalette.value(QStringLiteral("accent")).toString()));
}

// Omarchy renders the Agent accent from the theme's own terminal cyan, the
// colour the template names for it, so each desktop theme marks an Agent's
// work in a hue it already draws.
void ThemeControllerTest::fillsTheAgentAccentFromTheDesktopsCyan()
{
    QFile shipped(QStringLiteral(OMAWEB_OMARCHY_TEMPLATE_PATH));
    QVERIFY(shipped.open(QIODevice::ReadOnly));
    auto rendered = QString::fromUtf8(shipped.readAll());
    QVERIFY(rendered.contains(QStringLiteral("\"agentAccent\": \"{{ cyan }}\"")));
    rendered.replace(QStringLiteral("{{ cyan }}"), QStringLiteral("#2ac3de"));
    rendered.replace(QStringLiteral("{{ accent }}"), QStringLiteral("#7aa2f7"));
    rendered.replace(QStringLiteral("{{ darker_background }}"), QStringLiteral("#16161e"));
    rendered.replace(QStringLiteral("{{ dark_background }}"), QStringLiteral("#1a1b26"));
    rendered.replace(QStringLiteral("{{ foreground }}"), QStringLiteral("#c0caf5"));
    static const QRegularExpression token(QStringLiteral("\\{\\{ [a-z_]+ \\}\\}"));
    rendered.replace(token, QStringLiteral("#565f89"));

    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("omaweb.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(rendered.toUtf8());
    theme.close();
    QCOMPARE(QColor(ThemeController(theme.fileName())
                     .palette()
                     .value(QStringLiteral("agentAccent"))
                     .toString()),
        QColor(QStringLiteral("#2ac3de")));
}

// Omaweb owns the six, so a theme's own `spaces` are not read: a palette that
// names them gets the same six as one that does not. Settings offers them in
// hue order, and each is drawn at 3:1 on every ground a Space colour is on.
void ThemeControllerTest::drawsTheSixSpaceColoursOmawebOwns()
{
    QTemporaryDir root;
    const auto write = [&root](const QString &name, const QByteArray &json) {
        QFile file(root.filePath(name));
        if (!file.open(QIODevice::WriteOnly)) {
            return QString();
        }
        file.write(json);
        return file.fileName();
    };
    const auto plain = write(QStringLiteral("plain.json"),
        R"JSON({ "window": "#16151d", "sidebar": "#1d1b29", "text": "#f3f1fa" })JSON");
    const auto named = write(QStringLiteral("named.json"),
        R"JSON({ "window": "#16151d", "sidebar": "#1d1b29", "text": "#f3f1fa",
        "spaces": { "green": "#ff0000", "bright_blue": "#00ff00", "red": "#e06c75" } })JSON");
    const auto palette = ThemeController(plain).palette();
    QCOMPARE(palette.value(QStringLiteral("spaceColourNames")).toStringList(), spaceColourNames);
    const auto spaces = palette.value(QStringLiteral("spaces")).toMap();
    auto keys = spaces.keys();
    keys.sort();
    auto expected = spaceColourNames;
    expected.sort();
    QCOMPARE(keys, expected);
    QCOMPARE(ThemeController(named).palette().value(QStringLiteral("spaces")).toMap(), spaces);

    for (const auto &theme : {plain,
             write(QStringLiteral("light.json"),
                 R"JSON({ "window": "#ffffff", "sidebar": "#f4f4f4", "overlay": "#eeeeee",
                 "text": "#1a1a1a", "accent": "#3b6fd6" })JSON")}) {
        const auto drawn = ThemeController(theme).palette();
        const auto colours = drawn.value(QStringLiteral("spaces")).toMap();
        for (const auto &name : spaceColourNames) {
            const QColor colour(colours.value(name).toString());
            QVERIFY2(colour.isValid(), qPrintable(name));
            for (const auto &key : spaceGrounds) {
                const QColor ground(drawn.value(key).toString());
                QVERIFY2(ground.isValid(), qPrintable(key));
                QVERIFY2(contrastRatio(colour, ground) >= minimumGraphicContrast,
                    qPrintable(theme + u' ' + name + u' ' + key));
            }
        }
    }
}

// Every theme Omarchy ships, as its template renders it, gives six Space
// colours a reader can tell apart: each at 3:1 on the Space grounds, no two
// within the gap of each other, and none within the gap of urgent, Private or
// Agent. Copied from Omarchy 4.0.4's `themes/*/colors.toml`, the colours the
// template draws grounds, text and those three from.
void ThemeControllerTest::keepsTheSpaceColoursApartInEveryStockTheme_data()
{
    QTest::addColumn<QStringList>("colours");
    const QList<std::pair<const char *, QStringList>> themes {
        {"catppuccin",
            QStringList {QStringLiteral("#101019"), QStringLiteral("#161622"),
                QStringLiteral("#313244"), QStringLiteral("#585b70"), QStringLiteral("#cdd6f4"),
                QStringLiteral("#6c7086"), QStringLiteral("#89b4fa"), QStringLiteral("#f38ba8"),
                QStringLiteral("#f5c2e7"), QStringLiteral("#94e2d5")}},
        {"catppuccin-latte",
            QStringList {QStringLiteral("#d7d8dc"), QStringLiteral("#e3e4e8"),
                QStringLiteral("#dce0e8"), QStringLiteral("#acb0be"), QStringLiteral("#4c4f69"),
                QStringLiteral("#9ca0b0"), QStringLiteral("#1e66f5"), QStringLiteral("#d20f39"),
                QStringLiteral("#ea76cb"), QStringLiteral("#179299")}},
        {"ethereal",
            QStringList {QStringLiteral("#030610"), QStringLiteral("#040816"),
                QStringLiteral("#131a3a"), QStringLiteral("#6d7db6"), QStringLiteral("#ffcead"),
                QStringLiteral("#6d7db6"), QStringLiteral("#7d82d9"), QStringLiteral("#ed5b5a"),
                QStringLiteral("#c89dc1"), QStringLiteral("#a3bfd1")}},
        {"everforest",
            QStringList {QStringLiteral("#181d20"), QStringLiteral("#21272c"),
                QStringLiteral("#343f44"), QStringLiteral("#475258"), QStringLiteral("#d3c6aa"),
                QStringLiteral("#4f585e"), QStringLiteral("#7fbbb3"), QStringLiteral("#e67e80"),
                QStringLiteral("#d699b6"), QStringLiteral("#83c092")}},
        {"flexoki-light",
            QStringList {QStringLiteral("#e5e2d8"), QStringLiteral("#f2efe4"),
                QStringLiteral("#e6e4d9"), QStringLiteral("#b7b5ac"), QStringLiteral("#100f0f"),
                QStringLiteral("#878580"), QStringLiteral("#205ea6"), QStringLiteral("#d14d41"),
                QStringLiteral("#ce5d97"), QStringLiteral("#3aa99f")}},
        {"gruvbox",
            QStringList {QStringLiteral("#161616"), QStringLiteral("#1e1e1e"),
                QStringLiteral("#3c3836"), QStringLiteral("#665c54"), QStringLiteral("#d4be98"),
                QStringLiteral("#7c6f64"), QStringLiteral("#7daea3"), QStringLiteral("#ea6962"),
                QStringLiteral("#d3869b"), QStringLiteral("#89b482")}},
        {"hackerman",
            QStringList {QStringLiteral("#06060c"), QStringLiteral("#080910"),
                QStringLiteral("#151828"), QStringLiteral("#2d3450"), QStringLiteral("#ddf7ff"),
                QStringLiteral("#6a6e95"), QStringLiteral("#82fb9c"), QStringLiteral("#50f872"),
                QStringLiteral("#86a7df"), QStringLiteral("#7cf8f7")}},
        {"kanagawa",
            QStringList {QStringLiteral("#111116"), QStringLiteral("#17171e"),
                QStringLiteral("#223249"), QStringLiteral("#54546d"), QStringLiteral("#dcd7ba"),
                QStringLiteral("#727169"), QStringLiteral("#dcd7ba"), QStringLiteral("#c34043"),
                QStringLiteral("#957fb8"), QStringLiteral("#6a9589")}},
        {"last-horizon",
            QStringList {QStringLiteral("#060606"), QStringLiteral("#090809"),
                QStringLiteral("#0c0b0c"), QStringLiteral("#584e51"), QStringLiteral("#fafcfb"),
                QStringLiteral("#584e51"), QStringLiteral("#b59790"), QStringLiteral("#c38b7b"),
                QStringLiteral("#c4d8e2"), QStringLiteral("#a5a0b6")}},
        {"lumon",
            QStringList {QStringLiteral("#0b1216"), QStringLiteral("#101b21"),
                QStringLiteral("#1b2d40"), QStringLiteral("#304860"), QStringLiteral("#d6e2ee"),
                QStringLiteral("#4d86b0"), QStringLiteral("#8bc9eb"), QStringLiteral("#4d86b0"),
                QStringLiteral("#8bc9eb"), QStringLiteral("#b4e4f6")}},
        {"lupine",
            QStringList {QStringLiteral("#dedede"), QStringLiteral("#ececec"),
                QStringLiteral("#f5f5f5"), QStringLiteral("#9e9e9e"), QStringLiteral("#212121"),
                QStringLiteral("#757575"), QStringLiteral("#3264eb"), QStringLiteral("#c900c4"),
                QStringLiteral("#8a4ad7"), QStringLiteral("#0c67de")}},
        {"matte-black",
            QStringList {QStringLiteral("#090909"), QStringLiteral("#0d0d0d"),
                QStringLiteral("#1e1e1e"), QStringLiteral("#333333"), QStringLiteral("#bebebe"),
                QStringLiteral("#555555"), QStringLiteral("#e68e0d"), QStringLiteral("#d35f5f"),
                QStringLiteral("#d35f5f"), QStringLiteral("#bebebe")}},
        {"miasma",
            QStringList {QStringLiteral("#121212"), QStringLiteral("#191919"),
                QStringLiteral("#2c2c2c"), QStringLiteral("#666666"), QStringLiteral("#c2c2b0"),
                QStringLiteral("#555555"), QStringLiteral("#78824b"), QStringLiteral("#685742"),
                QStringLiteral("#bb7744"), QStringLiteral("#c9a554")}},
        {"nord",
            QStringList {QStringLiteral("#191c23"), QStringLiteral("#222730"),
                QStringLiteral("#3b4252"), QStringLiteral("#4c566a"), QStringLiteral("#d8dee9"),
                QStringLiteral("#667080"), QStringLiteral("#81a1c1"), QStringLiteral("#bf616a"),
                QStringLiteral("#b48ead"), QStringLiteral("#88c0d0")}},
        {"osaka-jade",
            QStringList {QStringLiteral("#090f0d"), QStringLiteral("#0c1512"),
                QStringLiteral("#23372b"), QStringLiteral("#53685b"), QStringLiteral("#c1c497"),
                QStringLiteral("#81b8a8"), QStringLiteral("#509475"), QStringLiteral("#ff5345"),
                QStringLiteral("#d2689c"), QStringLiteral("#2dd5b7")}},
        {"retro-82",
            {QStringLiteral("#020c17"), QStringLiteral("#031222"), QStringLiteral("#0a2540"),
                QStringLiteral("#2a6b78"), QStringLiteral("#f6dcac"), QStringLiteral("#3f8f8a"),
                QStringLiteral("#faa968"), QStringLiteral("#f85525"), QStringLiteral("#3f8f8a"),
                QStringLiteral("#8cbfb8")}},
        {"ristretto",
            QStringList {QStringLiteral("#181414"), QStringLiteral("#211b1b"),
                QStringLiteral("#3d2f2a"), QStringLiteral("#72696a"), QStringLiteral("#e6d9db"),
                QStringLiteral("#72696a"), QStringLiteral("#f38d70"), QStringLiteral("#fd6883"),
                QStringLiteral("#a8a9eb"), QStringLiteral("#85dacc")}},
        {"rose-pine",
            QStringList {QStringLiteral("#e1dbd5"), QStringLiteral("#ede7e1"),
                QStringLiteral("#f2e9e1"), QStringLiteral("#cecacd"), QStringLiteral("#575279"),
                QStringLiteral("#9893a5"), QStringLiteral("#56949f"), QStringLiteral("#b4637a"),
                QStringLiteral("#907aa9"), QStringLiteral("#d7827e")}},
        {"solitude",
            QStringList {QStringLiteral("#080a0b"), QStringLiteral("#0c0e10"),
                QStringLiteral("#101315"), QStringLiteral("#4b4e55"), QStringLiteral("#cacccc"),
                QStringLiteral("#4b4e55"), QStringLiteral("#798186"), QStringLiteral("#565d60"),
                QStringLiteral("#aeaeae"), QStringLiteral("#707070")}},
        {"tokyo-night",
            QStringList {QStringLiteral("#0e0e14"), QStringLiteral("#13141c"),
                QStringLiteral("#24283b"), QStringLiteral("#414868"), QStringLiteral("#a9b1d6"),
                QStringLiteral("#565f89"), QStringLiteral("#7aa2f7"), QStringLiteral("#f7768e"),
                QStringLiteral("#ad8ee6"), QStringLiteral("#449dab")}},
        {"vantablack",
            QStringList {QStringLiteral("#070707"), QStringLiteral("#090909"),
                QStringLiteral("#1a1a1a"), QStringLiteral("#7a7a7a"), QStringLiteral("#ffffff"),
                QStringLiteral("#505050"), QStringLiteral("#8d8d8d"), QStringLiteral("#a4a4a4"),
                QStringLiteral("#9b9b9b"), QStringLiteral("#b0b0b0")}},
        {"white",
            QStringList {QStringLiteral("#e8e8e8"), QStringLiteral("#f5f5f5"),
                QStringLiteral("#c0c0c0"), QStringLiteral("#808080"), QStringLiteral("#000000"),
                QStringLiteral("#c0c0c0"), QStringLiteral("#6e6e6e"), QStringLiteral("#2a2a2a"),
                QStringLiteral("#2e2e2e"), QStringLiteral("#3e3e3e")}},
    };
    for (const auto &[name, colours] : themes) {
        QTest::newRow(name) << colours;
    }
}

void ThemeControllerTest::keepsTheSpaceColoursApartInEveryStockTheme()
{
    QFETCH(QStringList, colours);
    const QStringList keys {QStringLiteral("darker_background"), QStringLiteral("dark_background"),
        QStringLiteral("lighter_background"), QStringLiteral("muted"), QStringLiteral("foreground"),
        QStringLiteral("dark_foreground"), QStringLiteral("accent"), QStringLiteral("red"),
        QStringLiteral("magenta"), QStringLiteral("cyan")};
    QCOMPARE(colours.size(), keys.size());
    QHash<QString, QString> terminal;
    for (qsizetype index = 0; index < keys.size(); ++index) {
        terminal.insert(keys.at(index), colours.at(index));
    }
    const auto rendered = renderedOmarchyTheme(terminal);
    QVERIFY(!rendered.isEmpty());
    QTemporaryDir root;
    QFile theme(root.filePath(QStringLiteral("omaweb.json")));
    QVERIFY(theme.open(QIODevice::WriteOnly));
    theme.write(rendered.toUtf8());
    theme.close();
    const auto palette = ThemeController(theme.fileName()).palette();
    const auto spaces = palette.value(QStringLiteral("spaces")).toMap();

    QList<double> accents;
    for (const auto *key : {"urgent", "privateAccent", "agentAccent"}) {
        const QColor accent(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(accent.isValid(), key);
        if (chroma(accent) >= chromaticFloor) {
            accents.append(hue(accent));
        }
    }
    QList<double> placed;
    for (const auto &name : spaceColourNames) {
        const QColor colour(spaces.value(name).toString());
        QVERIFY2(colour.isValid(), qPrintable(name));
        for (const auto &key : spaceGrounds) {
            const QColor ground(palette.value(key).toString());
            QVERIFY2(contrastRatio(colour, ground) >= minimumGraphicContrast,
                qPrintable(name + u' ' + key + u' ' + colour.name()));
        }
        const auto own = hue(colour);
        for (const auto accent : accents) {
            QVERIFY2(hueDistance(own, accent) >= spaceHueGap,
                qPrintable(QStringLiteral("%1 at %2 is near an accent at %3")
                        .arg(name)
                        .arg(own)
                        .arg(accent)));
        }
        for (const auto other : placed) {
            QVERIFY2(hueDistance(own, other) >= spaceHueGap,
                qPrintable(QStringLiteral("%1 at %2 is near another Space colour at %3")
                        .arg(name)
                        .arg(own)
                        .arg(other)));
        }
        placed.append(own);
    }
}

// A Space colour that lands inside the gap of an accent turns just far enough
// to clear it, and the others keep the hue they have without it.
void ThemeControllerTest::movesASpaceColourJustClearOfAnAccent()
{
    QTemporaryDir root;
    const auto spacesFor = [&root](const QString &privateAccent) {
        QFile file(root.filePath(privateAccent.mid(1) + QStringLiteral(".json")));
        if (!file.open(QIODevice::WriteOnly)) {
            return QVariantMap();
        }
        file.write(QString::fromUtf8(R"({ "window": "#16151d", "sidebar": "#1d1b29",
            "text": "#f3f1fa", "urgent": "#808080", "agentAccent": "#909090",
            "privateAccent": "%1" })")
                .arg(privateAccent)
                .toUtf8());
        file.close();
        return ThemeController(file.fileName()).palette().value(QStringLiteral("spaces")).toMap();
    };
    // A grey Private accent is no hue to keep clear of.
    const auto clear = spacesFor(QStringLiteral("#8a8a8a"));
    const auto violet = hue(QColor(clear.value(QStringLiteral("violet")).toString()));
    // A Private accent at violet's own hue.
    const auto clashing
        = spacesFor(QColor(clear.value(QStringLiteral("violet")).toString()).name(QColor::HexRgb));
    const auto moved = hue(QColor(clashing.value(QStringLiteral("violet")).toString()));
    QVERIFY2(hueDistance(moved, violet) >= spaceHueGap, qPrintable(QString::number(moved)));
    QVERIFY2(hueDistance(moved, violet) <= spaceHueGap + 3.0, qPrintable(QString::number(moved)));
    for (const auto &name : spaceColourNames) {
        if (name != QStringLiteral("violet")) {
            QCOMPARE(clashing.value(name), clear.value(name));
        }
    }
}

// The text a reader types into the Omnibar is read on its glass: the overlay
// at the glass's alpha over whatever is behind it, which on the Start page is
// the night road, anywhere from black to the sun. It clears 4.5:1 against the
// glass over both ends in every theme Omaweb bundles, the landing page's
// nine and the default, and in a light theme and a theme whose text is too
// close to its ground, which only the floor can rescue.
void ThemeControllerTest::keepsTheTypedTextReadableOnTheOmnibarsGlassInEveryBundledTheme()
{
    struct Theme {
        QString name;
        QString window;
        QString sidebar;
        QString text;
        QString accent;
    };
    QList<Theme> themes {
        {QStringLiteral("light"), QStringLiteral("#ffffff"), QStringLiteral("#f4f4f4"),
            QStringLiteral("#1a1a1a"), QStringLiteral("#3b6fd6")},
        {QStringLiteral("weak text"), QStringLiteral("#101010"), QStringLiteral("#222222"),
            QStringLiteral("#777777"), QStringLiteral("#3b6fd6")},
    };
    QFile css(QStringLiteral(OMAWEB_BUNDLED_THEMES_CSS));
    QVERIFY(css.open(QIODevice::ReadOnly));
    const auto sheet = QString::fromUtf8(css.readAll());
    QRegularExpression block(QStringLiteral("\\[data-theme=\"([^\"]+)\"\\] \\{([^}]*)\\}"));
    QRegularExpression role(QStringLiteral(R"RE(--(\w+): var\(--omaweb-\w+, (#[0-9a-f]{6})\))RE"));
    auto blocks = block.globalMatch(sheet);
    int bundled = 0;
    while (blocks.hasNext()) {
        const auto found = blocks.next();
        QHash<QString, QString> roles;
        auto each = role.globalMatch(found.captured(2));
        while (each.hasNext()) {
            const auto one = each.next();
            roles.insert(one.captured(1), one.captured(2));
        }
        themes.append({found.captured(1), roles.value(QStringLiteral("bg")),
            roles.value(QStringLiteral("sidebar")), roles.value(QStringLiteral("fg")),
            roles.value(QStringLiteral("accent"))});
        ++bundled;
    }
    QVERIFY2(bundled >= 9, "themes.css names fewer themes than the landing page offers");

    QTemporaryDir root;
    const auto over = [](const QColor &glass, double alpha, const QColor &behind) {
        return QColor::fromRgbF(glass.redF() * alpha + behind.redF() * (1 - alpha),
            glass.greenF() * alpha + behind.greenF() * (1 - alpha),
            glass.blueF() * alpha + behind.blueF() * (1 - alpha));
    };
    for (const auto &theme : themes) {
        QFile file(root.filePath(theme.name + QStringLiteral(".json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QStringLiteral(R"({ "window": "%1", "sidebar": "%2", "overlay": "%2",
            "text": "%3", "accent": "%4" })")
                .arg(theme.window, theme.sidebar, theme.text, theme.accent)
                .toUtf8());
        file.close();
        const auto palette = ThemeController(file.fileName()).palette();
        for (const auto &[field, overlay] :
            {std::pair {QStringLiteral("fieldText"), QStringLiteral("overlay")},
                std::pair {QStringLiteral("privateFieldText"), QStringLiteral("privateOverlay")}}) {
            const QColor text(palette.value(field).toString());
            const QColor glass(palette.value(overlay).toString());
            QVERIFY2(text.isValid(), qPrintable(theme.name + u' ' + field));
            const auto alpha = std::min<double>(glass.alphaF(), 0.8);
            for (const auto &behind : {QColor(Qt::black), QColor(Qt::white)}) {
                QVERIFY2(contrastRatio(text, over(glass, alpha, behind)) >= minimumContrast,
                    qPrintable(theme.name + u' ' + field + u' ' + behind.name()));
            }
        }
    }
}

// QFontDatabase needs a GUI application, so this suite is no longer guiless.
QTEST_MAIN(ThemeControllerTest)

#include "tst_themecontroller.moc"
