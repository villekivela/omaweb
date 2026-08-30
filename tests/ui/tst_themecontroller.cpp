#include "ThemeController.h"

#include <QColor>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using tanto::ThemeController;

class ThemeControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void enforcesDistinctPrivateColors();
    void appliesSemanticOpacityToChromeSurfaces();
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

QTEST_GUILESS_MAIN(ThemeControllerTest)

#include "tst_themecontroller.moc"
