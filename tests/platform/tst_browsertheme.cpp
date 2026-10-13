#include "BrowserTheme.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::BrowserTheme;
using omaweb::BrowserThemePaths;
using omaweb::OmarchyColours;

namespace {

QString fixture(const QString &name)
{
    return QDir(QStringLiteral(OMAWEB_BROWSER_THEME_FIXTURES)).filePath(name);
}

QByteArray contentsOf(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

// The colours Omarchy resolved for Catppuccin Latte, as `omarchy-theme-color
// --all` printed them.
OmarchyColours latte()
{
    OmarchyColours colours;
    for (const auto &line :
        contentsOf(fixture(QStringLiteral("catppuccin-latte.colours"))).split('\n')) {
        const auto tab = line.indexOf('\t');
        if (tab > 0) {
            colours.insert(QString::fromUtf8(line.left(tab)), QString::fromUtf8(line.mid(tab + 1)));
        }
    }
    return colours;
}

// A home with Omarchy's two theme folders, the desktop on Tokyo Night, and
// Omaweb's state beside them.
struct Home {
    QTemporaryDir root;
    BrowserThemePaths paths;
    // The `colors.toml` files the colour source was asked about, from
    // whichever thread asked.
    QStringList asked;
    QMutex askedLock;

    Home()
    {
        paths.userThemes = root.filePath(QStringLiteral(".config/omarchy/themes"));
        paths.systemThemes = root.filePath(QStringLiteral("usr/share/omarchy/themes"));
        paths.userTemplate
            = root.filePath(QStringLiteral(".config/omarchy/themed/omaweb.json.tpl"));
        paths.shippedTemplate = QStringLiteral(OMAWEB_OMARCHY_TEMPLATE_PATH);
        paths.desktopTheme = root.filePath(QStringLiteral(".local/state/omarchy/current"));
        paths.rendered
            = root.filePath(QStringLiteral(".local/share/omaweb/browser-theme/omaweb.json"));
        QDir().mkpath(paths.userThemes);
        QDir().mkpath(paths.systemThemes);
        write(QDir(paths.desktopTheme).filePath(QStringLiteral("theme.name")), "tokyo-night\n");
    }

    static void write(const QString &path, const QByteArray &contents)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    QString install(const QString &themes, const QString &folder, const QByteArray &colours = "x")
    {
        const auto path = QDir(themes).filePath(folder + QStringLiteral("/colors.toml"));
        write(path, colours);
        return path;
    }

    // Stands in for `omarchy-theme-color`: any `colors.toml` that is there
    // reads as Catppuccin Latte, with its accent swapped for the file's first
    // line when that line is a colour, so a test can tell an edit happened.
    BrowserTheme::ColourSource colours()
    {
        return [this](const QString &file) {
            {
                const QMutexLocker locker(&askedLock);
                asked.append(file);
            }
            const auto text = contentsOf(file);
            if (!QFileInfo::exists(file)) {
                return OmarchyColours {};
            }
            auto resolved = latte();
            if (text.startsWith('#')) {
                resolved.insert(QStringLiteral("accent"), QString::fromUtf8(text.trimmed()));
            }
            return resolved;
        };
    }

    QJsonObject rendered() const
    {
        return QJsonDocument::fromJson(contentsOf(paths.rendered)).object();
    }
};

} // namespace

class BrowserThemeTest final : public QObject {
    Q_OBJECT

private slots:
    void fillsATemplateAsOmarchyDoes();
    void ranksThePickedThemeBetweenTheReadersAndTheDesktops();
    void letsTheEnvironmentOverrideStandAlone();
    void rendersAPickedThemeThroughTheShippedTemplate();
    void leavesNoRenderWhileFollowingTheDesktop();
    void startsWithoutTheRenderOfAnEarlierPick();
    void rendersThroughTheReadersTemplate();
    void prefersTheReadersCopyOfATheme();
    void takesAPaletteTheThemeShips();
    void namesAThemeAsOmarchysMenuDoes();
    void rendersAgainWhenThePickedThemesColoursChange();
    void followsTheDesktopWhileThePickedThemeIsMissing();
    void returnsToAThemeInstalledAgain();
    void listsEveryInstalledThemeByName();
    void namesAndColoursTheDesktopTheme();
    void namesTheFileThatOverridesIt();
    void readsOnlyAFolderInTheThemesPlaces();
    void leavesTheDesktopsThemeAlone();
    void rendersOnlyWhenThePickedThemeChanges();
    void saysWhetherThePickedThemeIsDrawn();
    void keepsAQuietFolderForTheRender();
};

// The expected text is what `omarchy-theme-set-templates` itself rendered from
// the same template and colours, in a scratch home.
void BrowserThemeTest::fillsATemplateAsOmarchyDoes()
{
    QCOMPARE(omaweb::renderOmarchyTemplate(
                 contentsOf(fixture(QStringLiteral("tokens.txt.tpl"))), latte()),
        contentsOf(fixture(QStringLiteral("tokens.txt"))));
}

void BrowserThemeTest::ranksThePickedThemeBetweenTheReadersAndTheDesktops()
{
    QCOMPARE(omaweb::paletteSources({}, QStringLiteral("/c/theme.json"),
                 QStringLiteral("/s/browser-theme.json"), QStringLiteral("/o/omaweb.json"),
                 QStringLiteral(":/theme.json")),
        QStringList({QStringLiteral("/c/theme.json"), QStringLiteral("/s/browser-theme.json"),
            QStringLiteral("/o/omaweb.json"), QStringLiteral(":/theme.json")}));
}

void BrowserThemeTest::letsTheEnvironmentOverrideStandAlone()
{
    QCOMPARE(omaweb::paletteSources(QStringLiteral("/t/override.json"),
                 QStringLiteral("/c/theme.json"), QStringLiteral("/s/browser-theme.json"),
                 QStringLiteral("/o/omaweb.json"), QStringLiteral(":/theme.json")),
        QStringList({QStringLiteral("/t/override.json")}));
}

// The colours are what Omarchy's own render of the shipped template gives
// Catppuccin Latte: the window is its darker background, the text its
// foreground.
void BrowserThemeTest::rendersAPickedThemeThroughTheShippedTemplate()
{
    Home home;
    home.install(home.paths.systemThemes, QStringLiteral("catppuccin-latte"));
    BrowserTheme theme(home.paths, home.colours());

    theme.setChoice(QStringLiteral("catppuccin-latte"));

    const auto palette = home.rendered();
    QCOMPARE(palette.value(QStringLiteral("window")).toString(), QStringLiteral("#d7d8dc"));
    QCOMPARE(palette.value(QStringLiteral("text")).toString(), QStringLiteral("#4c4f69"));
    QCOMPARE(palette.value(QStringLiteral("accent")).toString(), QStringLiteral("#1e66f5"));
    QVERIFY(!theme.missing());
}

// A render left behind would outrank the desktop's for good.
void BrowserThemeTest::leavesNoRenderWhileFollowingTheDesktop()
{
    Home home;
    home.install(home.paths.systemThemes, QStringLiteral("catppuccin-latte"));
    BrowserTheme theme(home.paths, home.colours());
    theme.setChoice(QStringLiteral("catppuccin-latte"));
    QVERIFY(QFileInfo::exists(home.paths.rendered));

    theme.setChoice({});

    QVERIFY(!QFileInfo::exists(home.paths.rendered));
    QVERIFY(!theme.missing());
}

// A browser started after the reader chose to follow the desktop again draws
// the desktop's theme, not the render an earlier pick left (#740).
void BrowserThemeTest::startsWithoutTheRenderOfAnEarlierPick()
{
    Home home;
    home.install(home.paths.systemThemes, QStringLiteral("hackerman"));
    {
        BrowserTheme picked(home.paths, home.colours());
        picked.setChoice(QStringLiteral("hackerman"));
        QVERIFY(QFileInfo::exists(home.paths.rendered));
    }

    BrowserTheme restarted(home.paths, home.colours());
    restarted.setChoice({});

    QVERIFY(!QFileInfo::exists(home.paths.rendered));
}

void BrowserThemeTest::rendersThroughTheReadersTemplate()
{
    Home home;
    home.install(home.paths.systemThemes, QStringLiteral("catppuccin-latte"));
    Home::write(home.paths.userTemplate, R"({"window": "{{ background }}"})");
    BrowserTheme theme(home.paths, home.colours());

    theme.setChoice(QStringLiteral("catppuccin-latte"));

    QCOMPARE(home.rendered(), QJsonObject({{QStringLiteral("window"), QStringLiteral("#eff1f5")}}));
}

void BrowserThemeTest::prefersTheReadersCopyOfATheme()
{
    Home home;
    home.install(home.paths.systemThemes, QStringLiteral("nord"));
    const auto readers = home.install(home.paths.userThemes, QStringLiteral("nord"), "#123456\n");
    BrowserTheme theme(home.paths, home.colours());

    theme.setChoice(QStringLiteral("nord"));

    QCOMPARE(home.asked, QStringList({readers}));
    QCOMPARE(home.rendered().value(QStringLiteral("accent")).toString(), QStringLiteral("#123456"));
}

// Omarchy copies a theme's own `omaweb.json` rather than rendering the
// template, so a theme that ships one looks the same on the desktop and in a
// browser that picked it.
void BrowserThemeTest::takesAPaletteTheThemeShips()
{
    Home home;
    const auto colours = home.install(home.paths.systemThemes, QStringLiteral("white"));
    Home::write(QFileInfo(colours).dir().filePath(QStringLiteral("omaweb.json")),
        R"({"window": "#ffffff"})");
    BrowserTheme theme(home.paths, home.colours());

    theme.setChoice(QStringLiteral("white"));

    QCOMPARE(home.rendered(), QJsonObject({{QStringLiteral("window"), QStringLiteral("#ffffff")}}));
}

// What `omarchy theme list` prints for each folder.
void BrowserThemeTest::namesAThemeAsOmarchysMenuDoes()
{
    QCOMPARE(omaweb::omarchyThemeName(QStringLiteral("catppuccin-latte")),
        QStringLiteral("Catppuccin Latte"));
    QCOMPARE(omaweb::omarchyThemeName(QStringLiteral("retro-82")), QStringLiteral("Retro 82"));
    QCOMPARE(omaweb::omarchyThemeName(QStringLiteral("white")), QStringLiteral("White"));
}

// An editor saves by writing a new file over the old one, which is the edit
// made here.
void BrowserThemeTest::rendersAgainWhenThePickedThemesColoursChange()
{
    Home home;
    const auto colours = home.install(home.paths.userThemes, QStringLiteral("inkypinky"));
    BrowserTheme theme(home.paths, home.colours());
    theme.setChoice(QStringLiteral("inkypinky"));
    QCOMPARE(home.rendered().value(QStringLiteral("accent")).toString(), QStringLiteral("#1e66f5"));

    QSaveFile edit(colours);
    QVERIFY(edit.open(QIODevice::WriteOnly));
    edit.write("#abcdef\n");
    QVERIFY(edit.commit());

    QTRY_COMPARE(
        home.rendered().value(QStringLiteral("accent")).toString(), QStringLiteral("#abcdef"));
}

void BrowserThemeTest::followsTheDesktopWhileThePickedThemeIsMissing()
{
    Home home;
    home.install(home.paths.userThemes, QStringLiteral("inkypinky"));
    BrowserTheme theme(home.paths, home.colours());
    theme.setChoice(QStringLiteral("inkypinky"));
    QSignalSpy changed(&theme, &BrowserTheme::changed);

    QVERIFY(QDir(QDir(home.paths.userThemes).filePath(QStringLiteral("inkypinky")))
            .removeRecursively());

    QTRY_VERIFY(theme.missing());
    QVERIFY(!QFileInfo::exists(home.paths.rendered));
    QVERIFY(!changed.isEmpty());
    // The choice stands for when the theme comes back.
    QCOMPARE(theme.choice(), QStringLiteral("inkypinky"));
    QCOMPARE(theme.choiceName(), QStringLiteral("Inkypinky"));
}

void BrowserThemeTest::returnsToAThemeInstalledAgain()
{
    Home home;
    BrowserTheme theme(home.paths, home.colours());
    theme.setChoice(QStringLiteral("inkypinky"));
    QVERIFY(theme.missing());
    QVERIFY(!QFileInfo::exists(home.paths.rendered));

    home.install(home.paths.userThemes, QStringLiteral("inkypinky"), "#fedcba\n");

    QTRY_VERIFY(!theme.missing());
    QCOMPARE(home.rendered().value(QStringLiteral("accent")).toString(), QStringLiteral("#fedcba"));
}

void BrowserThemeTest::listsEveryInstalledThemeByName()
{
    Home home;
    home.install(home.paths.userThemes, QStringLiteral("nord"));
    home.install(home.paths.userThemes, QStringLiteral("inkypinky"));
    home.install(home.paths.systemThemes, QStringLiteral("nord"));
    home.install(home.paths.systemThemes, QStringLiteral("white"));
    home.install(home.paths.systemThemes, QStringLiteral("catppuccin-latte"));
    // Not a theme: Omarchy lists folders only.
    Home::write(QDir(home.paths.systemThemes).filePath(QStringLiteral("README")), "x");
    BrowserTheme theme(home.paths, home.colours());

    theme.refreshThemes();

    QStringList names;
    QStringList folders;
    for (const auto &entry : theme.themes()) {
        names.append(entry.toMap().value(QStringLiteral("name")).toString());
        folders.append(entry.toMap().value(QStringLiteral("folder")).toString());
    }
    QCOMPARE(names,
        QStringList({QStringLiteral("Catppuccin Latte"), QStringLiteral("Inkypinky"),
            QStringLiteral("Nord"), QStringLiteral("White")}));
    QCOMPARE(folders,
        QStringList({QStringLiteral("catppuccin-latte"), QStringLiteral("inkypinky"),
            QStringLiteral("nord"), QStringLiteral("white")}));
    QTRY_COMPARE(theme.themes().first().toMap().value(QStringLiteral("background")).toString(),
        QStringLiteral("#eff1f5"));
    const auto latte = theme.themes().first().toMap();
    QCOMPARE(latte.value(QStringLiteral("foreground")).toString(), QStringLiteral("#4c4f69"));
    QCOMPARE(latte.value(QStringLiteral("accent")).toString(), QStringLiteral("#1e66f5"));
}

void BrowserThemeTest::namesAndColoursTheDesktopTheme()
{
    Home home;
    Home::write(
        QDir(home.paths.desktopTheme).filePath(QStringLiteral("theme/colors.toml")), "#7aa2f7\n");
    BrowserTheme theme(home.paths, home.colours());

    theme.refreshThemes();

    QCOMPARE(theme.desktopName(), QStringLiteral("Tokyo Night"));
    QTRY_COMPARE(theme.desktopColours().value(QStringLiteral("accent")).toString(),
        QStringLiteral("#7aa2f7"));
}

void BrowserThemeTest::namesTheFileThatOverridesIt()
{
    Home home;
    const auto readerTheme = home.root.filePath(QStringLiteral(".config/omaweb/theme.json"));
    BrowserTheme following(home.paths, home.colours(), readerTheme);
    QCOMPARE(following.overrideSource(), QString());

    Home::write(readerTheme, "{}");
    QTRY_COMPARE(following.overrideSource(), readerTheme);
    QCOMPARE(BrowserTheme(home.paths, home.colours(), readerTheme, QStringLiteral("/t/x.json"))
                 .overrideSource(),
        QStringLiteral("OMAWEB_THEME_FILE"));
}

// The value comes from a file the reader edits and from Sync, so a path that
// leaves the two theme folders names no theme.
void BrowserThemeTest::readsOnlyAFolderInTheThemesPlaces()
{
    Home home;
    home.install(home.root.filePath(QStringLiteral("elsewhere")), QStringLiteral("theme"));
    BrowserTheme theme(home.paths, home.colours());

    theme.setChoice(QStringLiteral("../../../../elsewhere/theme"));

    QVERIFY(theme.missing());
    QVERIFY(home.asked.isEmpty());
    QVERIFY(!QFileInfo::exists(home.paths.rendered));
}

namespace {

QStringList filesUnder(const QString &directory)
{
    QStringList files;
    QDirIterator it(directory, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        files.append(it.next());
    }
    files.sort();
    return files;
}

} // namespace

// The desktop's theme and the app icon Omarchy renders with it stay as they
// are: nothing runs `omarchy`, and nothing is written into its folders.
void BrowserThemeTest::leavesTheDesktopsThemeAlone()
{
    Home home;
    home.install(home.paths.systemThemes, QStringLiteral("catppuccin-latte"));
    Home::write(home.paths.userTemplate, R"({"window": "{{ background }}"})");
    const auto marker = home.root.filePath(QStringLiteral("omarchy-ran"));
    const auto bin = home.root.filePath(QStringLiteral("bin"));
    Home::write(QDir(bin).filePath(QStringLiteral("omarchy")),
        "#!/bin/sh\ntouch '" + marker.toUtf8() + "'\n");
    QVERIFY(QFile::setPermissions(QDir(bin).filePath(QStringLiteral("omarchy")),
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    const auto path = qgetenv("PATH");
    qputenv("PATH", bin.toUtf8() + ':' + path);
    const auto omarchy = home.root.filePath(QStringLiteral(".config/omarchy"));
    const auto state = home.root.filePath(QStringLiteral(".local/state/omarchy"));
    const auto before = filesUnder(omarchy) + filesUnder(state);
    {
        BrowserTheme theme(home.paths, home.colours());
        theme.setChoice(QStringLiteral("catppuccin-latte"));
        theme.refreshThemes();
        QTRY_VERIFY(!theme.desktopColours().isEmpty());
        theme.setChoice({});
        QCoreApplication::processEvents();
    }
    qputenv("PATH", path);

    QVERIFY(!QFileInfo::exists(marker));
    QCOMPARE(filesUnder(omarchy) + filesUnder(state), before);
}

// Rendering runs Omarchy's colour process, on the window's thread, and the
// folders watched change for other themes too.
void BrowserThemeTest::rendersOnlyWhenThePickedThemeChanges()
{
    Home home;
    home.install(home.paths.systemThemes, QStringLiteral("catppuccin-latte"));
    BrowserTheme theme(home.paths, home.colours());
    theme.setChoice(QStringLiteral("catppuccin-latte"));
    QCOMPARE(home.asked.size(), 1);

    home.install(home.paths.userThemes, QStringLiteral("nord"));
    Home::write(QDir(home.paths.desktopTheme).filePath(QStringLiteral("theme.name")), "nord\n");
    QTRY_COMPARE(theme.desktopName(), QStringLiteral("Nord"));
    // Past the poll, which would render again if anything it reads moved.
    QTest::qWait(1500);

    QCOMPARE(home.asked.size(), 1);
}

void BrowserThemeTest::saysWhetherThePickedThemeIsDrawn()
{
    Home home;
    home.install(home.paths.systemThemes, QStringLiteral("catppuccin-latte"));
    const auto readerTheme = home.root.filePath(QStringLiteral(".config/omaweb/theme.json"));
    BrowserTheme theme(home.paths, home.colours(), readerTheme);
    QVERIFY(!theme.drawn());

    theme.setChoice(QStringLiteral("catppuccin-latte"));
    QVERIFY(theme.drawn());

    Home::write(readerTheme, "{}");
    QTRY_VERIFY(!theme.drawn());
    QVERIFY(QFile::remove(readerTheme));
    QTRY_VERIFY(theme.drawn());

    theme.setChoice(QStringLiteral("rose-pine"));
    QVERIFY(!theme.drawn());
}

// The theme controller watches the deepest folder of a palette source that
// exists. Without the render's own folder that is Omaweb's data root, which
// changes with every page, and each change would read the palette again.
void BrowserThemeTest::keepsAQuietFolderForTheRender()
{
    Home home;
    BrowserTheme theme(home.paths, home.colours());

    QVERIFY(QFileInfo(QFileInfo(home.paths.rendered).absolutePath()).isDir());
    QVERIFY(!QFileInfo::exists(home.paths.rendered));
}

QTEST_GUILESS_MAIN(BrowserThemeTest)
#include "tst_browsertheme.moc"
