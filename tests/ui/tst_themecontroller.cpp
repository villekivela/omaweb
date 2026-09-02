#include "ThemeController.h"

#include <QColor>
#include <QFile>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QTest>

using tanto::ThemeController;

class ThemeControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void enforcesDistinctPrivateColors();
    void appliesSemanticOpacityToChromeSurfaces();
    void givesFullPageSurfacesTheSidebarsColourAndTheirOwnTranslucency();
    void namesOneColourForSomethingBeingWrong();
    void resolvesTheFirstInstalledTypeFamily();
    void fallsBackToAFamilyTheHostActuallyHas();
    void keepsTheTypeBaseSizeUsable();
    void namesTheColoursCodeIsReadIn();
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
        "privateSidebar": "#800080",
        "opacity": { "sidebar": 0.9, "sheet": 0.6 }
    })JSON");
    theme.close();

    ThemeController controller(theme.fileName());
    const auto palette = controller.palette();

    // A light theme's sheet is light: inheriting the theme's own sidebar rather
    // than falling back to Tanto's dark is what makes that true.
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
        "sheet": "#123456",
        "opacity": { "sheet": 0.6 }
    })JSON");
    named.close();

    ThemeController namedController(named.fileName());
    QCOMPARE(QColor(namedController.palette().value(QStringLiteral("sheetOpaque")).toString()),
        QColor(QStringLiteral("#123456")));
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
    // Whatever the fallback lands on, it is a family the host has. A
    // fontconfig alias is not: "monospace" does not exist on macOS.
    QVERIFY(QFontDatabase::hasFamily(family));
    QVERIFY(family != QStringLiteral("monospace"));
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
// it draws them in Tanto's colours rather than Chromium's. A theme that says
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

// QFontDatabase needs a GUI application, so this suite is no longer guiless.
QTEST_MAIN(ThemeControllerTest)

#include "tst_themecontroller.moc"
