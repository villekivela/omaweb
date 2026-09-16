#include "FontSettings.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::FontSettings;

class FontSettingsTest final : public QObject {
    Q_OBJECT

private slots:
    void drawsTheInterfaceAtTheThemesSizeUntilTold();
    void stepsTheInterfaceSizeWithinItsRange();
    void followsAThemeChangeOnlyWhileNoOverrideStands();
    void keepsTheInterfaceSizeAcrossARestart();
    void offersAPageTheEnginesFontsUntilTold();
    void offersOnlyInstalledFamilies();
    void clampsThePageSizes();
    void keepsThePageFontsAcrossARestart();
    void readsAFileItCannotUseAsTheDefault_data();
    void readsAFileItCannotUseAsTheDefault();
    void writesNothingForAChoiceAlreadyMade();
};

namespace {

const QStringList installed {
    QStringLiteral("Inter"), QStringLiteral("JetBrains Mono"), QStringLiteral("Noto Serif")};

FontSettings::PageFonts engineFonts()
{
    return {QStringLiteral("DejaVu Sans"), QStringLiteral("DejaVu Sans Mono"), 16, 0};
}

} // namespace

// The theme sets the size Omaweb's own type grows from (ADR 0018), and a
// reader who has said nothing gets that size.
void FontSettingsTest::drawsTheInterfaceAtTheThemesSizeUntilTold()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    FontSettings fonts(root.filePath(QStringLiteral("config")), installed);
    QVERIFY(!fonts.interfaceFontSizeOverridden());
    fonts.setThemeFontSize(13);
    QCOMPARE(fonts.interfaceFontSize(), 13);
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/interface.json"))));
}

// A step up or down starts from the size on show, whichever of the theme and
// the reader chose it, and the ends of the range are ends: a press past them
// changes nothing and says nothing.
void FontSettingsTest::stepsTheInterfaceSizeWithinItsRange()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    FontSettings fonts(root.filePath(QStringLiteral("config")), installed);
    fonts.setThemeFontSize(12);
    QSignalSpy changed(&fonts, &FontSettings::interfaceFontSizeChanged);

    fonts.increaseInterfaceFontSize();
    QCOMPARE(fonts.interfaceFontSize(), 13);
    QVERIFY(fonts.interfaceFontSizeOverridden());
    fonts.decreaseInterfaceFontSize();
    fonts.decreaseInterfaceFontSize();
    QCOMPARE(fonts.interfaceFontSize(), 11);
    QCOMPARE(changed.count(), 3);

    fonts.setInterfaceFontSize(FontSettings::maximumInterfaceFontSize + 10);
    QCOMPARE(fonts.interfaceFontSize(), FontSettings::maximumInterfaceFontSize);
    fonts.increaseInterfaceFontSize();
    QCOMPARE(fonts.interfaceFontSize(), FontSettings::maximumInterfaceFontSize);
    fonts.setInterfaceFontSize(FontSettings::minimumInterfaceFontSize - 10);
    QCOMPARE(fonts.interfaceFontSize(), FontSettings::minimumInterfaceFontSize);
    fonts.decreaseInterfaceFontSize();
    QCOMPARE(fonts.interfaceFontSize(), FontSettings::minimumInterfaceFontSize);
    QCOMPARE(changed.count(), 5);

    fonts.resetInterfaceFontSize();
    QVERIFY(!fonts.interfaceFontSizeOverridden());
    QCOMPARE(fonts.interfaceFontSize(), 12);
    QCOMPARE(changed.count(), 6);
}

// An override is the reader's and a theme switch leaves it alone; without one,
// the new theme's size is what the interface takes.
void FontSettingsTest::followsAThemeChangeOnlyWhileNoOverrideStands()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    FontSettings fonts(root.filePath(QStringLiteral("config")), installed);
    fonts.setThemeFontSize(12);
    QSignalSpy changed(&fonts, &FontSettings::interfaceFontSizeChanged);
    QSignalSpy themeChanged(&fonts, &FontSettings::themeFontSizeChanged);

    fonts.setThemeFontSize(14);
    QCOMPARE(fonts.interfaceFontSize(), 14);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(themeChanged.count(), 1);

    fonts.setInterfaceFontSize(18);
    fonts.setThemeFontSize(10);
    QCOMPARE(fonts.interfaceFontSize(), 18);
    QCOMPARE(fonts.themeFontSize(), 10);
    // The theme moved under an override, which changes nothing on show but
    // is still a theme change to whoever names the theme's size.
    QCOMPARE(changed.count(), 2);
    QCOMPARE(themeChanged.count(), 2);

    fonts.resetInterfaceFontSize();
    QCOMPARE(fonts.interfaceFontSize(), 10);
}

void FontSettingsTest::keepsTheInterfaceSizeAcrossARestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto configRoot = root.filePath(QStringLiteral("config"));
    {
        FontSettings fonts(configRoot, installed);
        fonts.setThemeFontSize(12);
        fonts.setInterfaceFontSize(16);
    }
    {
        FontSettings restarted(configRoot, installed);
        restarted.setThemeFontSize(12);
        QVERIFY(restarted.interfaceFontSizeOverridden());
        QCOMPARE(restarted.interfaceFontSize(), 16);
        restarted.resetInterfaceFontSize();
    }
    FontSettings again(configRoot, installed);
    again.setThemeFontSize(12);
    QVERIFY(!again.interfaceFontSizeOverridden());
    QCOMPARE(again.interfaceFontSize(), 12);
}

// A page's fonts are the engine's until the reader names one, and naming one
// is an override rather than a new default: an empty family or a zero size
// hands the value back to the engine.
void FontSettingsTest::offersAPageTheEnginesFontsUntilTold()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    FontSettings fonts(root.filePath(QStringLiteral("config")), installed);
    fonts.setEngineFonts(engineFonts());
    QSignalSpy changed(&fonts, &FontSettings::pageFontsChanged);

    QCOMPARE(fonts.pageFonts().standardFamily, QStringLiteral("DejaVu Sans"));
    QCOMPARE(fonts.pageFonts().fixedFamily, QStringLiteral("DejaVu Sans Mono"));
    QCOMPARE(fonts.pageFonts().fontSize, 16);
    QCOMPARE(fonts.pageFonts().minimumFontSize, 0);
    QVERIFY(!fonts.pageFamilyOverridden(FontSettings::PageFamily::Standard));
    QVERIFY(!fonts.pageSizeOverridden(FontSettings::PageSize::Minimum));

    fonts.setPageFamily(FontSettings::PageFamily::Standard, QStringLiteral("Inter"));
    fonts.setPageFamily(FontSettings::PageFamily::Fixed, QStringLiteral("JetBrains Mono"));
    fonts.setPageSize(FontSettings::PageSize::Default, 18);
    fonts.setPageSize(FontSettings::PageSize::Minimum, 12);
    QCOMPARE(changed.count(), 4);
    QCOMPARE(fonts.pageFonts().standardFamily, QStringLiteral("Inter"));
    QCOMPARE(fonts.pageFonts().fixedFamily, QStringLiteral("JetBrains Mono"));
    QCOMPARE(fonts.pageFonts().fontSize, 18);
    QCOMPARE(fonts.pageFonts().minimumFontSize, 12);
    QVERIFY(fonts.pageFamilyOverridden(FontSettings::PageFamily::Fixed));
    QVERIFY(fonts.pageSizeOverridden(FontSettings::PageSize::Default));

    fonts.setPageFamily(FontSettings::PageFamily::Standard, QString());
    fonts.setPageSize(FontSettings::PageSize::Default, 0);
    QCOMPARE(fonts.pageFonts().standardFamily, QStringLiteral("DejaVu Sans"));
    QCOMPARE(fonts.pageFonts().fontSize, 16);
    QVERIFY(!fonts.pageFamilyOverridden(FontSettings::PageFamily::Standard));
    QVERIFY(!fonts.pageSizeOverridden(FontSettings::PageSize::Default));

    // The engine's own defaults arriving later reach every value the reader
    // has not overridden.
    fonts.setEngineFonts(
        {QStringLiteral("Liberation Serif"), QStringLiteral("Liberation Mono"), 15, 0});
    QCOMPARE(fonts.pageFonts().standardFamily, QStringLiteral("Liberation Serif"));
    QCOMPARE(fonts.pageFonts().fixedFamily, QStringLiteral("JetBrains Mono"));
    QCOMPARE(fonts.pageFonts().fontSize, 15);
    QCOMPARE(fonts.pageFonts().minimumFontSize, 12);
}

// A family the host does not have is not offered, and one the host has lost
// since it was chosen reads as the engine default rather than as a name Qt
// would substitute something for in silence.
void FontSettingsTest::offersOnlyInstalledFamilies()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto configRoot = root.filePath(QStringLiteral("config"));
    {
        FontSettings fonts(configRoot, installed);
        fonts.setEngineFonts(engineFonts());
        QCOMPARE(fonts.installedFamilies(), installed);
        fonts.setPageFamily(FontSettings::PageFamily::Standard, QStringLiteral("Noto Serif"));
        QCOMPARE(fonts.pageFonts().standardFamily, QStringLiteral("Noto Serif"));
        fonts.setPageFamily(FontSettings::PageFamily::Fixed, QStringLiteral("Comic Sans MS"));
        QCOMPARE(fonts.pageFonts().fixedFamily, QStringLiteral("DejaVu Sans Mono"));
        QVERIFY(!fonts.pageFamilyOverridden(FontSettings::PageFamily::Fixed));
    }
    FontSettings without(configRoot, {QStringLiteral("Inter")});
    without.setEngineFonts(engineFonts());
    QCOMPARE(without.pageFonts().standardFamily, QStringLiteral("DejaVu Sans"));
    QVERIFY(!without.pageFamilyOverridden(FontSettings::PageFamily::Standard));
}

// A default size is a size a page is read at and a minimum is a floor under
// one, so each has its own range, and a value past either end is the end.
void FontSettingsTest::clampsThePageSizes()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    FontSettings fonts(root.filePath(QStringLiteral("config")), installed);
    fonts.setEngineFonts(engineFonts());

    fonts.setPageSize(FontSettings::PageSize::Default, 200);
    QCOMPARE(fonts.pageFonts().fontSize, FontSettings::maximumPageFontSize);
    fonts.setPageSize(FontSettings::PageSize::Default, 1);
    QCOMPARE(fonts.pageFonts().fontSize, FontSettings::minimumPageFontSize);
    fonts.setPageSize(FontSettings::PageSize::Minimum, 200);
    QCOMPARE(fonts.pageFonts().minimumFontSize, FontSettings::maximumPageMinimumFontSize);
    fonts.setPageSize(FontSettings::PageSize::Minimum, -3);
    QCOMPARE(fonts.pageFonts().minimumFontSize, 0);
    QVERIFY(!fonts.pageSizeOverridden(FontSettings::PageSize::Minimum));
}

void FontSettingsTest::keepsThePageFontsAcrossARestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto configRoot = root.filePath(QStringLiteral("config"));
    {
        FontSettings fonts(configRoot, installed);
        fonts.setPageFamily(FontSettings::PageFamily::Standard, QStringLiteral("Inter"));
        fonts.setPageFamily(FontSettings::PageFamily::Fixed, QStringLiteral("JetBrains Mono"));
        fonts.setPageSize(FontSettings::PageSize::Default, 20);
        fonts.setPageSize(FontSettings::PageSize::Minimum, 10);
    }
    FontSettings restarted(configRoot, installed);
    restarted.setEngineFonts(engineFonts());
    QCOMPARE(restarted.pageFonts().standardFamily, QStringLiteral("Inter"));
    QCOMPARE(restarted.pageFonts().fixedFamily, QStringLiteral("JetBrains Mono"));
    QCOMPARE(restarted.pageFonts().fontSize, 20);
    QCOMPARE(restarted.pageFonts().minimumFontSize, 10);
}

void FontSettingsTest::readsAFileItCannotUseAsTheDefault_data()
{
    QTest::addColumn<QByteArray>("contents");
    QTest::newRow("not json") << QByteArray("font-size = 20");
    QTest::newRow("not an object") << QByteArray("[20]");
    QTest::newRow("not a number") << QByteArray(R"({"font-size": "large"})");
    QTest::newRow("page fonts not an object") << QByteArray(R"({"page-fonts": 20})");
    QTest::newRow("page size not a number")
        << QByteArray(R"({"page-fonts": {"font-size": "large", "standard-family": 3}})");
}

// A file the reader edited by hand and got wrong changes nothing: the theme
// and the engine answer until the file says otherwise in a form that can be
// read.
void FontSettingsTest::readsAFileItCannotUseAsTheDefault()
{
    QFETCH(QByteArray, contents);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath(QStringLiteral("config")));
    QFile file(root.filePath(QStringLiteral("config/interface.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
    file.close();
    FontSettings fonts(root.filePath(QStringLiteral("config")), installed);
    fonts.setThemeFontSize(12);
    fonts.setEngineFonts(engineFonts());
    QVERIFY(!fonts.interfaceFontSizeOverridden());
    QCOMPARE(fonts.interfaceFontSize(), 12);
    QCOMPARE(fonts.pageFonts().standardFamily, QStringLiteral("DejaVu Sans"));
    QCOMPARE(fonts.pageFonts().fontSize, 16);
}

void FontSettingsTest::writesNothingForAChoiceAlreadyMade()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    FontSettings fonts(root.filePath(QStringLiteral("config")), installed);
    fonts.setThemeFontSize(12);
    fonts.setEngineFonts(engineFonts());
    QSignalSpy interfaceChanged(&fonts, &FontSettings::interfaceFontSizeChanged);
    QSignalSpy pageChanged(&fonts, &FontSettings::pageFontsChanged);
    fonts.resetInterfaceFontSize();
    fonts.setPageFamily(FontSettings::PageFamily::Standard, QString());
    fonts.setPageSize(FontSettings::PageSize::Default, 0);
    QCOMPARE(interfaceChanged.count(), 0);
    QCOMPARE(pageChanged.count(), 0);
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/interface.json"))));
}

QTEST_GUILESS_MAIN(FontSettingsTest)

#include "tst_fontsettings.moc"
