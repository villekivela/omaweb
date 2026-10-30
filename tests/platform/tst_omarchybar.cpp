#include "OmarchyBar.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <QTest>

using omaweb::OmarchyBarPaths;
using omaweb::OmarchyBarPlacement;
using omaweb::OmarchyCommand;

namespace {

QByteArray contentsOf(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void write(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(contents), contents.size());
}

// A reader's layout, written the way the shell writes it, with keys in an order
// a sorted writer would not keep.
const QByteArray readersLayout(R"({
  "version": 1,
  "bar": {
    "position": "top",
    "layout": {
      "left": [
        { "id": "omarchy.menu" },
        { "id": "omarchy.workspaces" },
        { "id": "omarchy.media" }
      ],
      "center": [
        { "id": "omarchy.clock", "format": "HH:mm" }
      ],
      "right": [
        { "id": "omarchy.tray" }
      ]
    }
  },
  "appearance": {}
}
)");

QJsonObject layoutIn(const QString &path)
{
    return QJsonDocument::fromJson(contentsOf(path))
        .object()
        .value(QStringLiteral("bar"))
        .toObject()
        .value(QStringLiteral("layout"))
        .toObject();
}

// The ids in one section of the layout, each with its role where it has one.
QStringList section(const QJsonObject &layout, const QString &name)
{
    QStringList entries;
    for (const auto &entry : layout.value(name).toArray()) {
        const auto object = entry.toObject();
        const auto role = object.value(QStringLiteral("role")).toString();
        entries.append(object.value(QStringLiteral("id")).toString()
            + (role.isEmpty() ? QString() : u':' + role));
    }
    return entries;
}

// The shell, as far as Omaweb talks to it: `omarchy-shell` answers each call it
// is given, and `jq` is the real one, because the layout is edited by it.
class Desktop final {
public:
    bool shellRunning = true;
    // Whether jq answers on a later turn of the event loop, as a process does,
    // and each one sooner than the one before, as two processes may.
    bool later = false;
    int jqCalls = 0;
    QList<QStringList> shellCalls;

    OmarchyCommand command()
    {
        return [this](const QString &program, const QStringList &arguments, const QByteArray &input,
                   std::function<void(int, const QByteArray &)> done) {
            if (program == u"jq") {
                QProcess jq;
                jq.start(program, arguments);
                jq.write(input);
                jq.closeWriteChannel();
                jq.waitForFinished();
                const auto exitCode = jq.exitCode();
                const auto output = jq.readAllStandardOutput();
                if (later) {
                    QTimer::singleShot(std::max(0, 60 - 50 * jqCalls++),
                        [done, exitCode, output] { done(exitCode, output); });
                } else {
                    done(exitCode, output);
                }
                return;
            }
            if (program == u"omarchy-shell") {
                shellCalls.append(arguments);
                if (!shellRunning) {
                    done(1, {});
                    return;
                }
                done(0, arguments.contains(u"ping") ? QByteArray("pong\n") : QByteArray());
                return;
            }
            done(-1, {});
        };
    }
};

struct Home {
    QTemporaryDir root;
    OmarchyBarPaths paths {
        .plugins = root.filePath(QStringLiteral("config/omarchy/plugins")),
        .shellConfig = root.filePath(QStringLiteral("config/omarchy/shell.json")),
        .defaultShellConfig = root.filePath(QStringLiteral("omarchy/config/omarchy/shell.json")),
    };
    QString shipped = root.filePath(QStringLiteral("usr/share/omaweb/omarchy/bar"));

    Home() { QDir().mkpath(shipped); }
};

} // namespace

class OmarchyBarTest final : public QObject {
    Q_OBJECT

private slots:
    void placingLinksTheShippedWidgetIntoThePlugins();
    void placingPutsTheTabsAfterTheWorkspacesAndTheLocationFirstInTheCenter();
    void placingKeepsTheReadersLayoutAsTheyWroteIt();
    void placingTwicePlacesTheWidgetOnce();
    void aReaderWithoutALayoutGetsTheDefaultWithTheWidgetInIt();
    void removingTakesBothPlacementsAndTheLinkAway();
    void removingLeavesAPluginOmawebDidNotLinkAlone();
    void startingPointsTheLinkAtThisBuildAndLeavesTheLayoutAlone();
    void theSettingIsOfferedOnlyWhileTheShellAnswers();
    void choosingTheBarAndBackAtOnceLeavesTheSidebarsLayout();
    void spaceNamesAreTheTabsPlacementsOwnSetting();
};

void OmarchyBarTest::placingLinksTheShippedWidgetIntoThePlugins()
{
    Home home;
    write(home.paths.shellConfig, readersLayout);
    Desktop desktop;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());

    placement.place();

    QCOMPARE(QFileInfo(home.paths.link()).symLinkTarget(), home.shipped);
    QVERIFY(desktop.shellCalls.contains(QStringList {
        QStringLiteral("-q"), QStringLiteral("shell"), QStringLiteral("rescanPlugins")}));
}

void OmarchyBarTest::placingPutsTheTabsAfterTheWorkspacesAndTheLocationFirstInTheCenter()
{
    Home home;
    write(home.paths.shellConfig, readersLayout);
    Desktop desktop;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());

    placement.place();

    const auto layout = layoutIn(home.paths.shellConfig);
    QCOMPARE(section(layout, QStringLiteral("left")),
        (QStringList {QStringLiteral("omarchy.menu"), QStringLiteral("omarchy.workspaces"),
            QStringLiteral("omaweb.bar:tabs"), QStringLiteral("omarchy.media")}));
    QCOMPARE(section(layout, QStringLiteral("center")),
        (QStringList {QStringLiteral("omaweb.bar:location"), QStringLiteral("omarchy.clock")}));
    QCOMPARE(
        section(layout, QStringLiteral("right")), QStringList {QStringLiteral("omarchy.tray")});
    // Read again once it is written, so the shell shows what the file says.
    QCOMPARE(desktop.shellCalls.last(),
        (QStringList {
            QStringLiteral("-q"), QStringLiteral("shell"), QStringLiteral("reloadConfig")}));
}

void OmarchyBarTest::placingKeepsTheReadersLayoutAsTheyWroteIt()
{
    Home home;
    write(home.paths.shellConfig, readersLayout);
    Desktop desktop;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());

    placement.place();

    const auto written = QString::fromUtf8(contentsOf(home.paths.shellConfig));
    QVERIFY(written.indexOf(u"\"version\"") < written.indexOf(u"\"bar\""));
    QVERIFY(written.indexOf(u"\"bar\"") < written.indexOf(u"\"appearance\""));
    QVERIFY(written.indexOf(u"\"position\"") < written.indexOf(u"\"layout\""));
    QVERIFY(written.contains(u"\n  \"bar\": {"));
}

void OmarchyBarTest::placingTwicePlacesTheWidgetOnce()
{
    Home home;
    write(home.paths.shellConfig, readersLayout);
    Desktop desktop;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());

    placement.place();
    placement.place();

    const auto layout = layoutIn(home.paths.shellConfig);
    QCOMPARE(section(layout, QStringLiteral("left")).count(QStringLiteral("omaweb.bar:tabs")), 1);
    QCOMPARE(
        section(layout, QStringLiteral("center")).count(QStringLiteral("omaweb.bar:location")), 1);
}

// The shell reads its default layout while the reader has no file, and their
// file is the whole layout once they do, so the default comes along.
void OmarchyBarTest::aReaderWithoutALayoutGetsTheDefaultWithTheWidgetInIt()
{
    Home home;
    write(home.paths.defaultShellConfig, readersLayout);
    Desktop desktop;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());

    placement.place();

    const auto layout = layoutIn(home.paths.shellConfig);
    QVERIFY(section(layout, QStringLiteral("left")).contains(QStringLiteral("omarchy.menu")));
    QVERIFY(section(layout, QStringLiteral("left")).contains(QStringLiteral("omaweb.bar:tabs")));
    QCOMPARE(contentsOf(home.paths.defaultShellConfig), readersLayout);
}

void OmarchyBarTest::removingTakesBothPlacementsAndTheLinkAway()
{
    Home home;
    write(home.paths.shellConfig, readersLayout);
    Desktop desktop;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());
    placement.place();

    placement.remove();

    const auto layout = layoutIn(home.paths.shellConfig);
    QCOMPARE(section(layout, QStringLiteral("left")),
        (QStringList {QStringLiteral("omarchy.menu"), QStringLiteral("omarchy.workspaces"),
            QStringLiteral("omarchy.media")}));
    QCOMPARE(
        section(layout, QStringLiteral("center")), QStringList {QStringLiteral("omarchy.clock")});
    QVERIFY(!QFileInfo(home.paths.link()).isSymLink());
    QVERIFY(!QFileInfo::exists(home.paths.link()));
}

void OmarchyBarTest::removingLeavesAPluginOmawebDidNotLinkAlone()
{
    Home home;
    write(home.paths.shellConfig, readersLayout);
    write(home.paths.link() + QStringLiteral("/manifest.json"), "{}");
    Desktop desktop;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());

    placement.remove();

    QCOMPARE(contentsOf(home.paths.link() + QStringLiteral("/manifest.json")), QByteArray("{}"));
}

void OmarchyBarTest::startingPointsTheLinkAtThisBuildAndLeavesTheLayoutAlone()
{
    Home home;
    write(home.paths.shellConfig, readersLayout);
    QDir().mkpath(home.paths.plugins);
    QVERIFY(QFile::link(home.root.filePath(QStringLiteral("an-older-build")), home.paths.link()));
    Desktop desktop;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());

    placement.relink();

    QCOMPARE(QFileInfo(home.paths.link()).symLinkTarget(), home.shipped);
    QCOMPARE(contentsOf(home.paths.shellConfig), readersLayout);
}

void OmarchyBarTest::theSettingIsOfferedOnlyWhileTheShellAnswers()
{
    Home home;
    Desktop desktop;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());
    QSignalSpy changed(&placement, &OmarchyBarPlacement::availableChanged);
    QVERIFY(!placement.available());

    placement.checkAvailable();
    QVERIFY(placement.available());
    QCOMPARE(
        desktop.shellCalls.last(), (QStringList {QStringLiteral("shell"), QStringLiteral("ping")}));

    desktop.shellRunning = false;
    placement.checkAvailable();
    QVERIFY(!placement.available());
    QCOMPARE(changed.count(), 2);
}

// A reader who chooses the bar and the sidebar again before the first edit is
// written gets the second: each edit starts from what the one before it wrote.
void OmarchyBarTest::choosingTheBarAndBackAtOnceLeavesTheSidebarsLayout()
{
    Home home;
    write(home.paths.shellConfig, readersLayout);
    Desktop desktop;
    desktop.later = true;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());

    placement.place();
    placement.remove();

    QTRY_VERIFY(!QFileInfo(home.paths.link()).isSymLink());
    QTest::qWait(100);
    const auto layout = layoutIn(home.paths.shellConfig);
    QCOMPARE(section(layout, QStringLiteral("left")),
        (QStringList {QStringLiteral("omarchy.menu"), QStringLiteral("omarchy.workspaces"),
            QStringLiteral("omarchy.media")}));
    QCOMPARE(
        section(layout, QStringLiteral("center")), QStringList {QStringLiteral("omarchy.clock")});
}

// Omaweb's Settings and the bar's own widget settings name the same value: the
// `spaceNames` of the tabs' entry, which the Location's entry does not carry.
void OmarchyBarTest::spaceNamesAreTheTabsPlacementsOwnSetting()
{
    Home home;
    write(home.paths.shellConfig, readersLayout);
    Desktop desktop;
    OmarchyBarPlacement placement(home.paths, home.shipped, desktop.command());
    placement.place();
    QCOMPARE(placement.spaceNames(), QStringLiteral("full"));
    QSignalSpy changed(&placement, &OmarchyBarPlacement::spaceNamesChanged);

    placement.setSpaceNames(QStringLiteral("letter"));

    QCOMPARE(placement.spaceNames(), QStringLiteral("letter"));
    QCOMPARE(changed.count(), 1);
    const auto layout = layoutIn(home.paths.shellConfig);
    const auto tabs = layout.value(QStringLiteral("left")).toArray().at(2).toObject();
    QCOMPARE(tabs.value(QStringLiteral("role")).toString(), QStringLiteral("tabs"));
    QCOMPARE(tabs.value(QStringLiteral("spaceNames")).toString(), QStringLiteral("letter"));
    const auto location = layout.value(QStringLiteral("center")).toArray().at(0).toObject();
    QVERIFY(!location.contains(QStringLiteral("spaceNames")));
    QCOMPARE(desktop.shellCalls.last(),
        (QStringList {
            QStringLiteral("-q"), QStringLiteral("shell"), QStringLiteral("reloadConfig")}));

    // Set from the bar's side, it is read again when asked.
    write(home.paths.shellConfig,
        contentsOf(home.paths.shellConfig).replace("\"letter\"", "\"full\""));
    placement.readSpaceNames();
    QCOMPARE(placement.spaceNames(), QStringLiteral("full"));
}

QTEST_GUILESS_MAIN(OmarchyBarTest)

#include "tst_omarchybar.moc"
