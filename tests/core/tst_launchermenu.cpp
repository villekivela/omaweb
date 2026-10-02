#include "BrowserController.h"
#include "LauncherMenu.h"
#include "SpaceStorage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::BrowserController;
using omaweb::LauncherMenu;
using omaweb::LauncherMenuPaths;
using omaweb::SpaceStorage;

namespace {

// A package's menu, an Elephant configuration directory that may or may not
// be there, and a profile for the setting to live in.
class Machine final {
public:
    Machine()
    {
        // The package lays the menu out under `omaweb/elephant/menus`, and a link into it is
        // how Omaweb knows a link is its own.
        QDir(root.path()).mkpath(QStringLiteral("usr/share/omaweb/elephant/menus"));
        write(source(), "-- the menu");
    }

    QString source() const
    {
        return root.filePath(QStringLiteral("usr/share/omaweb/elephant/menus/omawebtabs.lua"));
    }
    QString menus() const { return root.filePath(QStringLiteral("home/.config/elephant/menus")); }
    QString link() const { return QDir(menus()).filePath(QStringLiteral("omawebtabs.lua")); }

    LauncherMenuPaths paths(bool elephantInstalled = true) const
    {
        return {.source = source(), .directory = menus(), .elephantInstalled = elephantInstalled};
    }

    void makeMenusDirectory() const
    {
        QDir(root.path()).mkpath(QStringLiteral("home/.config/elephant/menus"));
    }

    static void write(const QString &path, const QByteArray &text)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text);
    }

    // The link's target without following it, so a dangling one is read too.
    QString target() const { return QFile::symLinkTarget(link()); }
    bool somethingIsThere() const
    {
        return QFileInfo(link()).isSymLink() || QFileInfo::exists(link());
    }

    QTemporaryDir root;
    QTemporaryDir profile;
};

} // namespace

class LauncherMenuTest final : public QObject {
    Q_OBJECT

private slots:
    void leavesElephantsDirectoryAloneWhereElephantIsNotInstalled();
    void linksTheShippedMenuIntoElephantsDirectory();
    void keepsALinkItAlreadyMade();
    void pointsItsOwnLinkAtTheMenuThatIsInstalledNow();
    void replacesItsOwnLinkWhoseMenuIsGone();
    void neverReplacesAFileTheReaderPutThere();
    void leavesAForeignLinkAlone();
    void leavesADanglingForeignLinkAlone();
    void linksNothingWhenThePackageShippedNoMenu();
    void removesItsOwnLinkWhenTurnedOffAndNothingElse();
    void isOnUntilTheReaderTurnsItOffAndRemembersIt();
    void isAvailableOnlyWhereElephantIsInstalled();
};

void LauncherMenuTest::leavesElephantsDirectoryAloneWhereElephantIsNotInstalled()
{
    Machine machine;
    omaweb::updateLauncherLink(machine.paths(false), true);
    QVERIFY(!QFileInfo::exists(machine.menus()));
    QVERIFY(!QFileInfo::exists(QDir(machine.root.path()).filePath(QStringLiteral("home"))));
}

void LauncherMenuTest::linksTheShippedMenuIntoElephantsDirectory()
{
    Machine machine;
    omaweb::updateLauncherLink(machine.paths(), true);
    QVERIFY(QFileInfo(machine.link()).isSymLink());
    QCOMPARE(machine.target(), machine.source());
    QCOMPARE(QFileInfo(machine.link()).size(), QFileInfo(machine.source()).size());
}

void LauncherMenuTest::keepsALinkItAlreadyMade()
{
    Machine machine;
    omaweb::updateLauncherLink(machine.paths(), true);
    const auto before = QFileInfo(machine.link()).birthTime();
    omaweb::updateLauncherLink(machine.paths(), true);
    QCOMPARE(machine.target(), machine.source());
    QCOMPARE(QFileInfo(machine.link()).birthTime(), before);
}

// A package that moved, or an upgrade to another prefix, leaves a link naming the old place.
void LauncherMenuTest::pointsItsOwnLinkAtTheMenuThatIsInstalledNow()
{
    Machine machine;
    QDir(machine.root.path()).mkpath(QStringLiteral("old/share/omaweb/elephant/menus"));
    const auto old
        = machine.root.filePath(QStringLiteral("old/share/omaweb/elephant/menus/omawebtabs.lua"));
    Machine::write(old, "-- the old menu");
    machine.makeMenusDirectory();
    QVERIFY(QFile::link(old, machine.link()));

    omaweb::updateLauncherLink(machine.paths(), true);
    QCOMPARE(machine.target(), machine.source());
}

void LauncherMenuTest::replacesItsOwnLinkWhoseMenuIsGone()
{
    Machine machine;
    machine.makeMenusDirectory();
    QVERIFY(QFile::link(
        QStringLiteral("/gone/share/omaweb/elephant/menus/omawebtabs.lua"), machine.link()));

    omaweb::updateLauncherLink(machine.paths(), true);
    QCOMPARE(machine.target(), machine.source());
}

void LauncherMenuTest::neverReplacesAFileTheReaderPutThere()
{
    Machine machine;
    machine.makeMenusDirectory();
    Machine::write(machine.link(), "-- mine");

    omaweb::updateLauncherLink(machine.paths(), true);
    QVERIFY(!QFileInfo(machine.link()).isSymLink());
    QFile file(machine.link());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("-- mine"));

    // Turning it off removes nothing of theirs either.
    omaweb::updateLauncherLink(machine.paths(), false);
    QVERIFY(QFileInfo::exists(machine.link()));
}

void LauncherMenuTest::leavesAForeignLinkAlone()
{
    Machine machine;
    machine.makeMenusDirectory();
    const auto theirs = machine.root.filePath(QStringLiteral("dotfiles/tabs.lua"));
    QDir(machine.root.path()).mkpath(QStringLiteral("dotfiles"));
    Machine::write(theirs, "-- theirs");
    QVERIFY(QFile::link(theirs, machine.link()));

    omaweb::updateLauncherLink(machine.paths(), true);
    QCOMPARE(machine.target(), theirs);
    omaweb::updateLauncherLink(machine.paths(), false);
    QCOMPARE(machine.target(), theirs);
}

void LauncherMenuTest::leavesADanglingForeignLinkAlone()
{
    Machine machine;
    machine.makeMenusDirectory();
    QVERIFY(
        QFile::link(machine.root.filePath(QStringLiteral("dotfiles/missing.lua")), machine.link()));

    omaweb::updateLauncherLink(machine.paths(), true);
    QCOMPARE(machine.target(), machine.root.filePath(QStringLiteral("dotfiles/missing.lua")));
    omaweb::updateLauncherLink(machine.paths(), false);
    QVERIFY(QFileInfo(machine.link()).isSymLink());
}

// A build tree, which has no installed package beside it.
void LauncherMenuTest::linksNothingWhenThePackageShippedNoMenu()
{
    Machine machine;
    QVERIFY(QFile::remove(machine.source()));
    omaweb::updateLauncherLink(machine.paths(), true);
    QVERIFY(!machine.somethingIsThere());
}

void LauncherMenuTest::removesItsOwnLinkWhenTurnedOffAndNothingElse()
{
    Machine machine;
    omaweb::updateLauncherLink(machine.paths(), true);
    Machine::write(QDir(machine.menus()).filePath(QStringLiteral("theirs.lua")), "-- theirs");

    omaweb::updateLauncherLink(machine.paths(), false);
    QVERIFY(!machine.somethingIsThere());
    QVERIFY(QFileInfo::exists(QDir(machine.menus()).filePath(QStringLiteral("theirs.lua"))));
    QVERIFY(QFileInfo::exists(machine.source()));

    // Nothing left to remove is not an error.
    omaweb::updateLauncherLink(machine.paths(), false);
}

void LauncherMenuTest::isOnUntilTheReaderTurnsItOffAndRemembersIt()
{
    Machine machine;
    {
        BrowserController browser(SpaceStorage(machine.profile.path(), QStringLiteral("test")));
        LauncherMenu menu(&browser, machine.paths());
        QVERIFY(menu.enabled());
        QVERIFY(machine.somethingIsThere());

        QSignalSpy changed(&menu, &LauncherMenu::enabledChanged);
        menu.setEnabled(false);
        QCOMPARE(changed.count(), 1);
        QVERIFY(!machine.somethingIsThere());
        menu.setEnabled(false);
        QCOMPARE(changed.count(), 1);
    }
    BrowserController browser(SpaceStorage(machine.profile.path(), QStringLiteral("test")));
    LauncherMenu again(&browser, machine.paths());
    QVERIFY(!again.enabled());
    QVERIFY(!machine.somethingIsThere());
    again.setEnabled(true);
    QCOMPARE(machine.target(), machine.source());
}

void LauncherMenuTest::isAvailableOnlyWhereElephantIsInstalled()
{
    Machine machine;
    BrowserController browser(SpaceStorage(machine.profile.path(), QStringLiteral("test")));
    QVERIFY(LauncherMenu(&browser, machine.paths(true)).available());
    QVERIFY(!LauncherMenu(&browser, machine.paths(false)).available());
}

QTEST_GUILESS_MAIN(LauncherMenuTest)
#include "tst_launchermenu.moc"
