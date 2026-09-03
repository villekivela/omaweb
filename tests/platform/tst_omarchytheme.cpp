#include "OmarchyTheme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using omaweb::followOmarchyTheme;
using omaweb::OmarchyTemplateOutcome;
using omaweb::OmarchyThemePaths;

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

// A machine that has Omarchy: the state directory it generates into exists,
// and so does the theme the reader is currently looking at.
OmarchyThemePaths omarchyIn(const QTemporaryDir &home)
{
    OmarchyThemePaths paths;
    paths.configuration = home.filePath(QStringLiteral(".config/omarchy"));
    paths.state = home.filePath(QStringLiteral(".local/state/omarchy"));
    QDir().mkpath(QFileInfo(paths.renderedTheme()).absolutePath());
    return paths;
}

QString shippedTemplate()
{
    return QStringLiteral(OMAWEB_OMARCHY_TEMPLATE_PATH);
}

} // namespace

class OmarchyThemeTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void installsTheTemplateTheDesktopRendersFrom();
    void keepsATemplateSomeoneElseWrote();
    void writesNothingWhereOmarchyIsNotInstalled();
    void writesNothingWhenTheReaderManagesTheDirectory();
    void asksOmarchyToRenderTheActiveTheme();
    void leavesAnAlreadyRenderedThemeAlone();
    void rendersAgainForATemplateJustInstalled();
    void readsTheDirectoriesTheDesktopStandardNames();

private:
    QByteArray m_path;
};

void OmarchyThemeTest::init()
{
    m_path = qgetenv("PATH");
}

void OmarchyThemeTest::cleanup()
{
    // Every variable any slot sets is restored here rather than at the end of
    // the slot, which an assertion is free to return before reaching.
    qputenv("PATH", m_path);
    qunsetenv("OMAWEB_NO_OMARCHY_TEMPLATE");
    qunsetenv("OMARCHY_RENDERED");
    qunsetenv("OMARCHY_ASKED");
    qunsetenv("XDG_CONFIG_HOME");
    qunsetenv("XDG_STATE_HOME");
}

// Following the desktop's theme took three commands nobody was told to run.
// The template Omaweb ships is now installed where Omarchy renders templates
// from, and it is installed byte for byte: what the reader gets is what the
// repository reviewed.
void OmarchyThemeTest::installsTheTemplateTheDesktopRendersFrom()
{
    QTemporaryDir home;
    const auto paths = omarchyIn(home);

    QCOMPARE(followOmarchyTheme(paths, shippedTemplate()), OmarchyTemplateOutcome::Installed);
    QCOMPARE(contentsOf(paths.userTemplate()), contentsOf(shippedTemplate()));
    // The shipped template may come out of the binary's own read-only
    // resources. What lands on disk is the reader's file, and theirs to edit.
    QVERIFY(QFileInfo(paths.userTemplate()).isWritable());
}

// A template already there is a decision — a customisation of the reader's or
// of a theme's — and an upgrade does not overrule it.
void OmarchyThemeTest::keepsATemplateSomeoneElseWrote()
{
    QTemporaryDir home;
    const auto paths = omarchyIn(home);
    const auto mine = QByteArray(R"({"window": "{{ background }}"})");
    write(paths.userTemplate(), mine);

    // Left in place *and* said out loud: a reader chasing a colour the shipped
    // template names has no other way to learn theirs is the one being used.
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(
        QStringLiteral("^Omaweb kept the Omarchy theme template already at .*, which differs")));
    QCOMPARE(followOmarchyTheme(paths, shippedTemplate()), OmarchyTemplateOutcome::Kept);
    QCOMPARE(contentsOf(paths.userTemplate()), mine);
}

// Omaweb runs on desktops that are not Omarchy, and on macOS. Nothing is
// written into a configuration directory no theme manager reads, and the
// absence is not an error worth telling anyone about.
void OmarchyThemeTest::writesNothingWhereOmarchyIsNotInstalled()
{
    QTemporaryDir home;
    OmarchyThemePaths paths;
    paths.configuration = home.filePath(QStringLiteral(".config/omarchy"));
    paths.state = home.filePath(QStringLiteral(".local/state/omarchy"));

    QCOMPARE(followOmarchyTheme(paths, shippedTemplate()), OmarchyTemplateOutcome::Absent);
    QVERIFY(!QFileInfo::exists(paths.configuration));
}

// The reader whose `~/.config/omarchy` is a tracked, generated, or read-only
// tree says so once and Omaweb never writes there.
void OmarchyThemeTest::writesNothingWhenTheReaderManagesTheDirectory()
{
    QTemporaryDir home;
    const auto paths = omarchyIn(home);
    qputenv("OMAWEB_NO_OMARCHY_TEMPLATE", "1");

    QCOMPARE(followOmarchyTheme(paths, shippedTemplate()), OmarchyTemplateOutcome::Declined);
    QVERIFY(!QFileInfo::exists(paths.userTemplate()));
}

// Installing the template renders nothing on its own: Omarchy renders on
// `omarchy theme set`. So Omaweb asks for the theme that is already active,
// and the palette appears without the reader running a command.
void OmarchyThemeTest::asksOmarchyToRenderTheActiveTheme()
{
    QTemporaryDir home;
    const auto paths = omarchyIn(home);
    const auto binary = home.filePath(QStringLiteral("bin/omarchy"));
    write(binary, QByteArray("#!/bin/sh\n"
                             "[ \"$2\" = current ] && { echo matte-black; exit 0; }\n"
                             "printf '{\"window\": \"#101010\"}' > \"$OMARCHY_RENDERED\"\n"));
    QVERIFY(QFile::setPermissions(binary,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    qputenv("PATH", (QFileInfo(binary).absolutePath() + QStringLiteral(":")
        + QString::fromLocal8Bit(m_path)).toLocal8Bit());
    qputenv("OMARCHY_RENDERED", paths.renderedTheme().toLocal8Bit());

    QCOMPARE(followOmarchyTheme(paths, shippedTemplate()), OmarchyTemplateOutcome::Installed);
    QTRY_VERIFY(QFileInfo::exists(paths.renderedTheme()));
}

// A theme that is already rendered is the ordinary case, on every start after
// the first. Nothing is asked of Omarchy then, because a theme switch is the
// reader's to make and not a thing startup redoes.
void OmarchyThemeTest::leavesAnAlreadyRenderedThemeAlone()
{
    QTemporaryDir home;
    const auto paths = omarchyIn(home);
    const auto rendered = QByteArray(R"({"window": "#101010"})");
    write(paths.renderedTheme(), rendered);
    write(paths.userTemplate(), contentsOf(shippedTemplate()));
    const auto asked = home.filePath(QStringLiteral("asked"));
    const auto binary = home.filePath(QStringLiteral("bin/omarchy"));
    write(binary, QByteArray("#!/bin/sh\ntouch \"$OMARCHY_ASKED\"\n"));
    QVERIFY(QFile::setPermissions(binary,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    qputenv("PATH", (QFileInfo(binary).absolutePath() + QStringLiteral(":")
        + QString::fromLocal8Bit(m_path)).toLocal8Bit());
    qputenv("OMARCHY_ASKED", asked.toLocal8Bit());

    QCOMPARE(followOmarchyTheme(paths, shippedTemplate()), OmarchyTemplateOutcome::Kept);
    QTest::qWait(200);
    QVERIFY(!QFileInfo::exists(asked));
    QCOMPARE(contentsOf(paths.renderedTheme()), rendered);
}

// Taking the shipped template back -- deleting a customisation -- leaves a
// palette on disk that was rendered from a template that is no longer there.
// Installing one always asks for a render, so the colours match the template
// in use rather than the one it replaced.
void OmarchyThemeTest::rendersAgainForATemplateJustInstalled()
{
    QTemporaryDir home;
    const auto paths = omarchyIn(home);
    write(paths.renderedTheme(), QByteArray(R"({"window": "#101010"})"));
    const auto binary = home.filePath(QStringLiteral("bin/omarchy"));
    write(binary, QByteArray("#!/bin/sh\n"
                             "[ \"$2\" = current ] && { echo matte-black; exit 0; }\n"
                             "printf '{\"window\": \"#202020\"}' > \"$OMARCHY_RENDERED\"\n"));
    QVERIFY(QFile::setPermissions(binary,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    qputenv("PATH", (QFileInfo(binary).absolutePath() + QStringLiteral(":")
        + QString::fromLocal8Bit(m_path)).toLocal8Bit());
    qputenv("OMARCHY_RENDERED", paths.renderedTheme().toLocal8Bit());

    QCOMPARE(followOmarchyTheme(paths, shippedTemplate()), OmarchyTemplateOutcome::Installed);
    QTRY_COMPARE(contentsOf(paths.renderedTheme()), QByteArray(R"({"window": "#202020"})"));
}

// The directories are the desktop's, so they are read the way every other tool
// on that desktop reads them.
void OmarchyThemeTest::readsTheDirectoriesTheDesktopStandardNames()
{
    QTemporaryDir home;
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toLocal8Bit());
    qputenv("XDG_STATE_HOME", home.filePath(QStringLiteral("state")).toLocal8Bit());
    const auto configured = OmarchyThemePaths::fromEnvironment();
    QCOMPARE(configured.configuration, home.filePath(QStringLiteral("config/omarchy")));
    QCOMPARE(configured.state, home.filePath(QStringLiteral("state/omarchy")));

    qunsetenv("XDG_CONFIG_HOME");
    qunsetenv("XDG_STATE_HOME");

    const auto bare = OmarchyThemePaths::fromEnvironment();
    QCOMPARE(bare.configuration, QDir::home().filePath(QStringLiteral(".config/omarchy")));
    QCOMPARE(bare.state, QDir::home().filePath(QStringLiteral(".local/state/omarchy")));
    QVERIFY(bare.renderedTheme().endsWith(QStringLiteral("/current/theme/omaweb.json")));
    QVERIFY(bare.userTemplate().endsWith(QStringLiteral("/themed/omaweb.json.tpl")));
}

// Following the desktop's theme is files and one detached process, so this
// suite needs no window server -- and must not ask for one, or it aborts on a
// headless Linux runner where no platform plugin can load.
QTEST_GUILESS_MAIN(OmarchyThemeTest)

#include "tst_omarchytheme.moc"
