#include "ThemeController.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

using omaweb::ThemeController;

class ThemeControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void enforcesDistinctPrivateColors();
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
    void rejectsAPaletteWhoseSurfacesCannotShareReadableRoles();
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
    QCOMPARE(palette.value(QStringLiteral("opacity")).toMap()
                 .value(QStringLiteral("overlay")).toDouble(), 1.0);
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

// What the reader actually sees where a control is drawn at reduced opacity:
// the colour composited over the surface behind it.
QColor composited(const QColor &colour, double alpha, const QColor &ground)
{
    const auto channel = [alpha](int over, int under) {
        return qRound(alpha * over + (1.0 - alpha) * under);
    };
    return QColor::fromRgb(channel(colour.red(), ground.red()),
        channel(colour.green(), ground.green()), channel(colour.blue(), ground.blue()));
}

} // namespace

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
    for (const auto &key : {
             "sidebarOpaque", "surface", "surfaceHover", "overlayOpaque", "sheetOpaque"}) {
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
    const QList<QPair<QByteArray, QByteArray>> themes{
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
    for (const auto &key : {"windowOpaque", "sidebarOpaque", "surface", "surfaceHover",
             "overlayOpaque", "sheetOpaque"}) {
        const QColor ground(palette.value(QString::fromLatin1(key)).toString());
        QVERIFY2(ground.isValid(), key);
        QVERIFY2(contrastRatio(border, ground) >= minimumGraphicContrast, key);
    }
    const QColor privateBorder(palette.value(QStringLiteral("privateBorder")).toString());
    for (const auto &key : {"privateWindowOpaque", "privateSidebarOpaque", "privateSurface",
             "privateSurfaceHover", "privateOverlayOpaque", "privateSheetOpaque"}) {
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
    QVERIFY(contrastRatio(repaired, QColor(QStringLiteral("#202020")))
        >= minimumGraphicContrast);
    QVERIFY(std::abs(repaired.hslHueF() - named.hslHueF()) < 0.01);
}

void ThemeControllerTest::rejectsAPaletteWhoseSurfacesCannotShareReadableRoles()
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
    QCOMPARE(QColor(controller.palette().value(QStringLiteral("windowOpaque")).toString()),
        QColor(QStringLiteral("#16151d")));
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
    })JSON").arg(present).toUtf8());
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
        QStringList{QStringLiteral("monospace")});
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

    ThemeController controller(QStringList{absent, theme.fileName()});
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

    ThemeController controller(QStringList{desktop, builtIn.fileName()});
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

    ThemeController controller(QStringList{desktop, builtIn.fileName()});
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
        QStringList{QDir(link).filePath(QStringLiteral("omaweb.json")), builtIn.fileName()});
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
