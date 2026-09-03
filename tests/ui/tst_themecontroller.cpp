#include "ThemeController.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QTest>

using omaweb::ThemeController;

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
