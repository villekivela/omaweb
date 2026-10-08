#include "AppIcon.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using omaweb::AppIcon;
using omaweb::AppIconLauncher;
using omaweb::AppIconPaths;

namespace {

QByteArray contentsOf(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void write(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(contents), contents.size());
}

void makeExecutable(const QString &path, const QByteArray &script)
{
    write(path, script);
    QVERIFY(QFile::setPermissions(
        path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
}

// The `Icon=` the user entry names, read the way a launcher reads it.
QString entryIcon(const QString &entry)
{
    const auto match
        = QRegularExpression(QStringLiteral("^Icon=(.*)$"), QRegularExpression::MultilineOption)
              .match(QString::fromUtf8(contentsOf(entry)));
    return match.hasMatch() ? match.captured(1) : QString();
}

QString shippedTemplate() { return QStringLiteral(OMAWEB_OMARCHY_ICON_TEMPLATE_PATH); }

// An installed entry with a second group, so the test sees that only the
// application's own group is changed.
const QByteArray installedEntry("[Desktop Entry]\n"
                                "Type=Application\n"
                                "Name=Omaweb\n"
                                "Exec=omaweb %u\n"
                                "Icon=omaweb\n"
                                "StartupWMClass=omaweb\n"
                                "\n"
                                "[Desktop Action new-window]\n"
                                "Name=New Window\n"
                                "Exec=omaweb --new-window\n"
                                "Icon=omaweb\n");

// What Omarchy renders the template into under two themes.
const QByteArray tokyoNight("<svg><rect fill=\"#1a1b26\"/><path fill=\"#7aa2f7\"/></svg>\n");
const QByteArray gruvbox("<svg><rect fill=\"#282828\"/><path fill=\"#7daea3\"/></svg>\n");

} // namespace

class AppIconTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void themePointsTheLauncherAtTheRenderedIcon();
    void themeInstallsTheTemplateAndAsksOmarchyToRenderIt();
    void themeAsksAgainForATemplateNeverRendered();
    void asksForOneRenderWithThePalettesTemplate();
    void themeKeepsATemplateTheReaderChanged();
    void themeFollowsAThemeSwitch();
    void themeKeepsTheIconWhileAThemeSwitchIsHalfDone();
    void blackAndWhiteTakesTheLauncherBack();
    void blackAndWhiteKeepsATemplateTheReaderChanged();
    void leavesAnEntryTheReaderWrote();
    void rebuildsTheEntryFromTheInstalledOneOnStart();
    void declinedWritesNoTemplate();
    void declinedUsesAnIconTheReaderRendered();
    void writesNothingWhereOmarchyIsNotInstalled();
    void writesNoEntryWithoutAnInstalledOne();
    void namesTheIconByItsAbsolutePath();
    void readsTheDirectoriesTheDesktopStandardNames();

private:
    // A machine that has Omarchy and the installed package, in a temporary
    // home.
    AppIconPaths paths() const;

    std::unique_ptr<QTemporaryDir> m_home;
    QByteArray m_path;
};

void AppIconTest::init()
{
    m_home = std::make_unique<QTemporaryDir>();
    m_path = qgetenv("PATH");
    // The real `omarchy` would set the desktop's theme. Every run sees a
    // stand-in that only records being asked.
    makeExecutable(m_home->filePath(QStringLiteral("bin/omarchy")),
        QByteArray("#!/bin/sh\necho \"$@\" >> \"$(dirname \"$0\")/asked\"\n"));
    qputenv("PATH",
        (m_home->filePath(QStringLiteral("bin")) + QStringLiteral(":")
            + QString::fromLocal8Bit(m_path))
            .toLocal8Bit());
    const auto layout = paths();
    QDir().mkpath(QFileInfo(layout.renderedIcon()).absolutePath());
    write(layout.installedEntry, installedEntry);
}

void AppIconTest::cleanup()
{
    qputenv("PATH", m_path);
    qunsetenv("OMAWEB_NO_OMARCHY_TEMPLATE");
    qunsetenv("XDG_CONFIG_HOME");
    qunsetenv("XDG_STATE_HOME");
    qunsetenv("XDG_DATA_HOME");
    qunsetenv("XDG_DATA_DIRS");
    m_home.reset();
}

AppIconPaths AppIconTest::paths() const
{
    AppIconPaths paths;
    paths.omarchy.configuration = m_home->filePath(QStringLiteral(".config/omarchy"));
    paths.omarchy.state = m_home->filePath(QStringLiteral(".local/state/omarchy"));
    paths.installedEntry
        = m_home->filePath(QStringLiteral("usr/share/applications/omaweb.desktop"));
    paths.userEntry = m_home->filePath(QStringLiteral(".local/share/applications/omaweb.desktop"));
    paths.iconDirectory = m_home->filePath(QStringLiteral(".local/share/omaweb/app-icon"));
    return paths;
}

// Choosing Theme puts the theme's icon in the launcher: the user entry is the
// installed one with `Icon=` naming a copy of what Omarchy rendered, and
// `TryExec=` so a launcher that reads it drops the entry once the browser is
// gone. Nothing else in it differs.
void AppIconTest::themePointsTheLauncherAtTheRenderedIcon()
{
    const auto layout = paths();
    write(layout.renderedIcon(), tokyoNight);
    AppIconLauncher launcher(layout, shippedTemplate());

    launcher.setChoice(AppIcon::Theme);

    const auto icon = entryIcon(layout.userEntry);
    QVERIFY(QFileInfo(icon).isAbsolute());
    QCOMPARE(contentsOf(icon), tokyoNight);
    QCOMPARE(contentsOf(layout.userEntry),
        QByteArray("[Desktop Entry]\n"
                   "TryExec=omaweb\n"
                   "Type=Application\n"
                   "Name=Omaweb\n"
                   "Exec=omaweb %u\n"
                   "Icon=")
            + icon.toUtf8()
            + QByteArray("\n"
                         "StartupWMClass=omaweb\n"
                         "\n"
                         "[Desktop Action new-window]\n"
                         "Name=New Window\n"
                         "Exec=omaweb --new-window\n"
                         "Icon=omaweb\n"));
}

// Omarchy draws the Theme icon from a template, so Omaweb installs the one it
// ships, byte for byte and the reader's to edit, then asks Omarchy to render
// the active theme through it: the icon appears now, not at the next switch.
void AppIconTest::themeInstallsTheTemplateAndAsksOmarchyToRenderIt()
{
    const auto layout = paths();
    AppIconLauncher launcher(layout, shippedTemplate());

    launcher.setChoice(AppIcon::Theme);

    QCOMPARE(contentsOf(layout.iconTemplate()), contentsOf(shippedTemplate()));
    QVERIFY(QFileInfo(layout.iconTemplate()).isWritable());
    QTRY_VERIFY(contentsOf(m_home->filePath(QStringLiteral("bin/asked"))).startsWith("theme"));
}

// A template installed on a start that had no `omarchy` to ask, or whose
// render failed, is no icon yet. Every choice of Theme asks again while there
// is none, as the palette's template does.
void AppIconTest::themeAsksAgainForATemplateNeverRendered()
{
    const auto layout = paths();
    write(layout.iconTemplate(), contentsOf(shippedTemplate()));
    AppIconLauncher launcher(layout, shippedTemplate());

    launcher.setChoice(AppIcon::Theme);

    QTRY_VERIFY(contentsOf(m_home->filePath(QStringLiteral("bin/asked"))).startsWith("theme"));
}

// The first start with Theme chosen installs both of Omaweb's templates, and
// each wants a render. One `omarchy theme set` renders every template, and
// each is a full theme switch, so the desktop is asked once.
void AppIconTest::asksForOneRenderWithThePalettesTemplate()
{
    const auto layout = paths();
    AppIconLauncher launcher(layout, shippedTemplate());

    QCOMPARE(
        omaweb::followOmarchyTheme(layout.omarchy, QStringLiteral(OMAWEB_OMARCHY_TEMPLATE_PATH)),
        omaweb::OmarchyTemplateOutcome::Installed);
    launcher.setChoice(AppIcon::Theme);

    const auto asked = m_home->filePath(QStringLiteral("bin/asked"));
    QTRY_VERIFY(contentsOf(asked).contains("theme set"));
    // Long enough for a second request to have run as well.
    QTest::qWait(500);
    QCOMPARE(contentsOf(asked).count("theme set"), 1);
}

// A template already there is a customisation, the reader's or a theme's, and
// choosing Theme does not overrule it. Saying so is how a reader learns why the
// icon is not the one this version ships.
void AppIconTest::themeKeepsATemplateTheReaderChanged()
{
    const auto layout = paths();
    const auto mine = QByteArray(R"(<svg><path fill="{{ red }}"/></svg>)");
    write(layout.iconTemplate(), mine);
    AppIconLauncher launcher(layout, shippedTemplate());

    QTest::ignoreMessage(QtInfoMsg,
        QRegularExpression(
            QStringLiteral("^Omaweb kept the Omarchy icon template already at .*, which differs")));
    launcher.setChoice(AppIcon::Theme);

    QCOMPARE(contentsOf(layout.iconTemplate()), mine);
}

// Omarchy renders the icon to the same path on every theme switch, and its
// menu caches an icon by path, so it would keep the colours it drew first.
// While Omaweb runs, each render gets a copy of its own, the launcher is
// pointed at it, and the copy it replaced goes.
void AppIconTest::themeFollowsAThemeSwitch()
{
    const auto layout = paths();
    write(layout.renderedIcon(), tokyoNight);
    AppIconLauncher launcher(layout, shippedTemplate());
    launcher.setChoice(AppIcon::Theme);
    const auto before = entryIcon(layout.userEntry);

    // Omarchy builds the next theme beside the current one and swaps them.
    const auto next = m_home->filePath(QStringLiteral(".local/state/omarchy/current/next-theme"));
    write(QDir(next).filePath(QStringLiteral("omaweb-icon.svg")), gruvbox);
    QVERIFY(QDir(QFileInfo(layout.renderedIcon()).absolutePath()).removeRecursively());
    QVERIFY(QDir().rename(next, QFileInfo(layout.renderedIcon()).absolutePath()));

    QTRY_COMPARE(contentsOf(entryIcon(layout.userEntry)), gruvbox);
    QVERIFY(entryIcon(layout.userEntry) != before);
    QVERIFY(!QFileInfo::exists(before));
}

// Omarchy deletes the current theme before it renames the next one into its
// place, and the watch can fire in between. A theme half switched is not a
// theme without an icon: the launcher keeps the one it shows until the next
// render lands.
void AppIconTest::themeKeepsTheIconWhileAThemeSwitchIsHalfDone()
{
    const auto layout = paths();
    write(layout.renderedIcon(), tokyoNight);
    AppIconLauncher launcher(layout, shippedTemplate());
    launcher.setChoice(AppIcon::Theme);
    const auto before = entryIcon(layout.userEntry);
    const auto theme = QFileInfo(layout.renderedIcon()).absolutePath();
    const auto next = m_home->filePath(QStringLiteral(".local/state/omarchy/current/next-theme"));
    write(QDir(next).filePath(QStringLiteral("omaweb-icon.svg")), gruvbox);

    QVERIFY(QDir(theme).removeRecursively());
    // Long enough for the watch to report the removal, which the switch's
    // own rename otherwise follows at once.
    QTest::qWait(500);

    QCOMPARE(entryIcon(layout.userEntry), before);
    QCOMPARE(contentsOf(before), tokyoNight);

    QVERIFY(QDir().rename(next, theme));
    QTRY_COMPARE(contentsOf(entryIcon(layout.userEntry)), gruvbox);
}

// Black and white is the installed entry's own icon, so everything Theme put
// down goes: the user entry, the icon copies, and the template, which is still
// exactly what Omaweb shipped.
void AppIconTest::blackAndWhiteTakesTheLauncherBack()
{
    const auto layout = paths();
    write(layout.renderedIcon(), tokyoNight);
    AppIconLauncher launcher(layout, shippedTemplate());
    launcher.setChoice(AppIcon::Theme);
    QVERIFY(QFileInfo::exists(layout.userEntry));

    launcher.setChoice(AppIcon::BlackAndWhite);

    QVERIFY(!QFileInfo::exists(layout.userEntry));
    QVERIFY(QDir(layout.iconDirectory).isEmpty());
    QVERIFY(!QFileInfo::exists(layout.iconTemplate()));
}

// A template the reader changed is theirs. Taking the launcher back leaves it,
// and says so.
void AppIconTest::blackAndWhiteKeepsATemplateTheReaderChanged()
{
    const auto layout = paths();
    const auto mine = QByteArray(R"(<svg><path fill="{{ red }}"/></svg>)");
    write(layout.iconTemplate(), mine);
    AppIconLauncher launcher(layout, shippedTemplate());

    QTest::ignoreMessage(QtInfoMsg,
        QRegularExpression(
            QStringLiteral("^Omaweb left the Omarchy icon template at .*, which differs")));
    launcher.setChoice(AppIcon::BlackAndWhite);

    QCOMPARE(contentsOf(layout.iconTemplate()), mine);
}

// A reader may keep their own `omaweb.desktop`, with flags of theirs on
// `Exec=`. Omaweb writes and removes only the entry it wrote, which is the one
// whose icon is a copy of its own, and says why Theme left the other.
void AppIconTest::leavesAnEntryTheReaderWrote()
{
    const auto layout = paths();
    write(layout.renderedIcon(), tokyoNight);
    const auto mine = QByteArray("[Desktop Entry]\nExec=omaweb --mine\nIcon=omaweb\n");
    write(layout.userEntry, mine);
    AppIconLauncher launcher(layout, shippedTemplate());

    QTest::ignoreMessage(QtInfoMsg,
        QRegularExpression(QStringLiteral("^Omaweb left the desktop entry at .*, which it did not "
                                          "write")));
    launcher.setChoice(AppIcon::Theme);
    QCOMPARE(contentsOf(layout.userEntry), mine);

    launcher.setChoice(AppIcon::BlackAndWhite);
    QCOMPARE(contentsOf(layout.userEntry), mine);
}

// The user entry outranks the installed one, so a package that changes its
// entry would otherwise never reach a reader who chose Theme. Each start
// builds the user entry again from the installed one.
void AppIconTest::rebuildsTheEntryFromTheInstalledOneOnStart()
{
    const auto layout = paths();
    write(layout.renderedIcon(), tokyoNight);
    {
        AppIconLauncher launcher(layout, shippedTemplate());
        launcher.setChoice(AppIcon::Theme);
    }
    write(layout.installedEntry,
        QByteArray("[Desktop Entry]\nName=Omaweb\nExec=omaweb --upgraded %u\nIcon=omaweb\n"));

    AppIconLauncher launcher(layout, shippedTemplate());
    launcher.setChoice(AppIcon::Theme);

    const auto icon = entryIcon(layout.userEntry);
    QCOMPARE(contentsOf(icon), tokyoNight);
    QCOMPARE(contentsOf(layout.userEntry),
        QByteArray("[Desktop Entry]\nTryExec=omaweb\nName=Omaweb\nExec=omaweb --upgraded %u\nIcon=")
            + icon.toUtf8() + '\n');
}

// `OMAWEB_NO_OMARCHY_TEMPLATE` keeps Omaweb out of `~/.config/omarchy`
// altogether. Theme then has no icon to show unless the reader's own setup
// renders one, and the launcher keeps the installed icon.
void AppIconTest::declinedWritesNoTemplate()
{
    const auto layout = paths();
    qputenv("OMAWEB_NO_OMARCHY_TEMPLATE", "1");
    AppIconLauncher launcher(layout, shippedTemplate());

    launcher.setChoice(AppIcon::Theme);

    QVERIFY(!QFileInfo::exists(layout.iconTemplate()));
    QVERIFY(!QFileInfo::exists(layout.userEntry));
    QVERIFY(!QFileInfo::exists(m_home->filePath(QStringLiteral("bin/asked"))));
}

// The reader who manages their templates may render the icon themselves, and
// Theme then shows what they rendered.
void AppIconTest::declinedUsesAnIconTheReaderRendered()
{
    const auto layout = paths();
    qputenv("OMAWEB_NO_OMARCHY_TEMPLATE", "1");
    write(layout.renderedIcon(), gruvbox);
    AppIconLauncher launcher(layout, shippedTemplate());

    launcher.setChoice(AppIcon::Theme);

    QCOMPARE(contentsOf(entryIcon(layout.userEntry)), gruvbox);
    QVERIFY(!QFileInfo::exists(layout.iconTemplate()));
}

// Without Omarchy nothing renders the Theme icon, and nothing is written into
// a configuration directory no theme manager reads.
void AppIconTest::writesNothingWhereOmarchyIsNotInstalled()
{
    auto layout = paths();
    QVERIFY(QDir(layout.omarchy.state).removeRecursively());
    AppIconLauncher launcher(layout, shippedTemplate());

    launcher.setChoice(AppIcon::Theme);

    QVERIFY(!QFileInfo::exists(layout.omarchy.configuration));
    QVERIFY(!QFileInfo::exists(layout.userEntry));
}

// A build run from its tree has no installed entry to copy, and an entry made
// up without one would launch something other than what the package installs.
void AppIconTest::writesNoEntryWithoutAnInstalledOne()
{
    auto layout = paths();
    write(layout.renderedIcon(), tokyoNight);
    layout.installedEntry.clear();
    AppIconLauncher launcher(layout, shippedTemplate());

    launcher.setChoice(AppIcon::Theme);

    QVERIFY(!QFileInfo::exists(layout.userEntry));
    QVERIFY(QDir(layout.iconDirectory).isEmpty());
}

// `OMAWEB_DATA_ROOT` may be relative to where Omaweb was started. A launcher
// reads `Icon=` from anywhere, so it names the copy by its absolute path, and
// the entry is still recognised as Omaweb's on the next start.
void AppIconTest::namesTheIconByItsAbsolutePath()
{
    auto layout = paths();
    write(layout.renderedIcon(), tokyoNight);
    const auto started = QDir::currentPath();
    QVERIFY(QDir::setCurrent(m_home->path()));
    const auto restore = qScopeGuard([started] { QDir::setCurrent(started); });
    layout.iconDirectory = QStringLiteral("relative/app-icon");
    AppIconLauncher launcher(layout, shippedTemplate());

    launcher.setChoice(AppIcon::Theme);
    QVERIFY(QFileInfo(entryIcon(layout.userEntry)).isAbsolute());

    launcher.setChoice(AppIcon::BlackAndWhite);
    QVERIFY(!QFileInfo::exists(layout.userEntry));
}

// The entries are the desktop's, so they are found the way every launcher
// finds them: the reader's own under XDG_DATA_HOME, the installed one in the
// first of XDG_DATA_DIRS that has it, which is never the reader's own even
// where a desktop lists that directory there too.
void AppIconTest::readsTheDirectoriesTheDesktopStandardNames()
{
    const auto root = m_home->filePath(QStringLiteral("xdg"));
    write(
        QDir(root).filePath(QStringLiteral("system/applications/omaweb.desktop")), installedEntry);
    qputenv("XDG_DATA_HOME", QDir(root).filePath(QStringLiteral("data")).toLocal8Bit());
    write(QDir(root).filePath(QStringLiteral("data/applications/omaweb.desktop")), installedEntry);
    qputenv("XDG_DATA_DIRS",
        (QDir(root).filePath(QStringLiteral("data")) + QStringLiteral(":")
            + QDir(root).filePath(QStringLiteral("local")) + QStringLiteral(":")
            + QDir(root).filePath(QStringLiteral("system")))
            .toLocal8Bit());
    qputenv("XDG_CONFIG_HOME", QDir(root).filePath(QStringLiteral("config")).toLocal8Bit());
    qputenv("XDG_STATE_HOME", QDir(root).filePath(QStringLiteral("state")).toLocal8Bit());

    const auto layout
        = AppIconPaths::fromEnvironment(QDir(root).filePath(QStringLiteral("omaweb")));

    QCOMPARE(
        layout.userEntry, QDir(root).filePath(QStringLiteral("data/applications/omaweb.desktop")));
    QCOMPARE(layout.installedEntry,
        QDir(root).filePath(QStringLiteral("system/applications/omaweb.desktop")));
    QCOMPARE(layout.iconDirectory, QDir(root).filePath(QStringLiteral("omaweb/app-icon")));
    QCOMPARE(layout.iconTemplate(),
        QDir(root).filePath(QStringLiteral("config/omarchy/themed/omaweb-icon.svg.tpl")));
    QCOMPARE(layout.renderedIcon(),
        QDir(root).filePath(QStringLiteral("state/omarchy/current/theme/omaweb-icon.svg")));
}

// The app icon is files and one detached process, so the suite needs no
// window server.
QTEST_GUILESS_MAIN(AppIconTest)

#include "tst_appicon.moc"
