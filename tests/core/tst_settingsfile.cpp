#include "SettingsFile.h"

#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <atomic>
#include <memory>

using omaweb::SettingsFile;

class SettingsFileTest final : public QObject {
    Q_OBJECT

private slots:
    void readsTheReadersTypedValues();
    void rewritesTheFileAsTheReaderLaidItOut();
    void writesOnlyWhatDiffersFromTheDefault();
    void keepsSpaceColourOnUnlessTheReaderTurnsItOff();
    void keepsTheLastGoodValuesWhileTheFileCannotBeRead();
    void startsOnTheDefaultsFromAFileOfAnotherVersion();
    void readsABadValueAsItsDefaultAndNamesIt();
    void followsAnEditMadeWhileItRuns();
    void keepsBothOfTwoWritesAtOnce();
    void takesItsTurnBesideABusyWriter();
    void offersOnlyTheLimitsSettingsOffers();
    void offersTheTwoAppIcons();
    void leavesAKeyWithoutADefaultUndefined();
    void keepsASidebarOpacityBetweenHalfAndOne();
    void readsASidebarOpacityOutOfRangeAsTheThemes();
    void roundTripsAFractionThroughText();
    void writesOneMemberIntoWhatIsOnDisk();
};

namespace {

void writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(contents);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
}

} // namespace

// What the reader wrote is what Omaweb reads, typed, and a key they left out
// reads as its default.
void SettingsFileTest::readsTheReadersTypedValues()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    writeFile(root.filePath(QStringLiteral("settings.json")),
        R"({"version": 1, "sidebar-side": "right", "https-only": false,
            "put-away-unused-tabs-after": 3600})");
    SettingsFile settings(root.path());

    QVERIFY(settings.readable());
    QCOMPARE(settings.value(QStringLiteral("sidebar-side")), QJsonValue(QStringLiteral("right")));
    QCOMPARE(settings.value(QStringLiteral("https-only")), QJsonValue(false));
    QCOMPARE(settings.value(QStringLiteral("put-away-unused-tabs-after")), QJsonValue(3600));
    QVERIFY(settings.isSet(QStringLiteral("https-only")));
    QCOMPARE(settings.value(QStringLiteral("glance")), QJsonValue(true));
    QVERIFY(!settings.isSet(QStringLiteral("glance")));
    QVERIFY(settings.problem().isEmpty());
}

// A write changes the one key and nothing else the reader wrote: a key this
// build does not know stays, the order they chose stays, and a new key goes
// at the end.
void SettingsFileTest::rewritesTheFileAsTheReaderLaidItOut()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    writeFile(path,
        R"({"version": 1, "sidebar-side": "right", "from-a-newer-build": [1, 2],
            "page-fonts": {"standard-family": "Inter", "font-size": 17},
            "https-only": false})");
    SettingsFile settings(root.path());

    QVERIFY(settings.set(QStringLiteral("https-only"), false));
    QVERIFY(settings.set(QStringLiteral("glance"), false));
    QVERIFY(settings.set(QStringLiteral("page-fonts"),
        QJsonObject {{QStringLiteral("standard-family"), QStringLiteral("Inter")},
            {QStringLiteral("font-size"), 18}}));

    QCOMPARE(readFile(path), QByteArray(R"({
  "version": 1,
  "sidebar-side": "right",
  "from-a-newer-build": [
    1,
    2
  ],
  "page-fonts": {
    "standard-family": "Inter",
    "font-size": 18
  },
  "https-only": false,
  "glance": false
}
)"));
    QCOMPARE(settings.value(QStringLiteral("glance")), QJsonValue(false));
}

// Choosing the default takes the key out, so a later build's default reaches a
// reader who never chose otherwise.
void SettingsFileTest::writesOnlyWhatDiffersFromTheDefault()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("config/settings.json"));
    SettingsFile settings(root.filePath(QStringLiteral("config")));

    QVERIFY(settings.set(QStringLiteral("sidebar-side"), QStringLiteral("right")));
    QVERIFY(settings.set(QStringLiteral("tint-favicons"), true));
    QVERIFY(settings.set(QStringLiteral("sidebar-side"), QStringLiteral("left")));
    QVERIFY(settings.set(QStringLiteral("font-size"), QJsonValue()));
    QCOMPARE(readFile(path), QByteArray("{\n  \"version\": 1,\n  \"tint-favicons\": true\n}\n"));
    QVERIFY(!settings.isSet(QStringLiteral("sidebar-side")));

    QVERIFY(!settings.set(QStringLiteral("sidebar-side"), QStringLiteral("top")));
    QVERIFY(!settings.set(QStringLiteral("glance"), QStringLiteral("no")));
    QVERIFY(!settings.set(QStringLiteral("not-a-setting"), true));
    QCOMPARE(readFile(path), QByteArray("{\n  \"version\": 1,\n  \"tint-favicons\": true\n}\n"));
}

// Space colour is a switch in the file, on by default, as the Scene is.
void SettingsFileTest::keepsSpaceColourOnUnlessTheReaderTurnsItOff()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    SettingsFile settings(root.path());

    QVERIFY(SettingsFile::keys().contains(QStringLiteral("space-colours")));
    QCOMPARE(settings.value(QStringLiteral("space-colours")), QJsonValue(true));
    QVERIFY(settings.set(QStringLiteral("space-colours"), false));
    QCOMPARE(readFile(path), QByteArray("{\n  \"version\": 1,\n  \"space-colours\": false\n}\n"));
    QVERIFY(!settings.set(QStringLiteral("space-colours"), QStringLiteral("off")));
    QVERIFY(settings.set(QStringLiteral("space-colours"), true));
    QVERIFY(!settings.isSet(QStringLiteral("space-colours")));
}

// A file the reader is halfway through editing is not a file of defaults:
// what was read last stands, and Omaweb writes nothing over it, so a Settings
// change can never throw away what the reader typed.
void SettingsFileTest::keepsTheLastGoodValuesWhileTheFileCannotBeRead()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    writeFile(path, R"({"version": 1, "glance": false})");
    SettingsFile settings(root.path());
    QSignalSpy status(&settings, &SettingsFile::statusChanged);

    const QByteArray broken(R"({"version": 1, "glance": true,
  "sidebar-side": "right",,
})");
    writeFile(path, broken);
    settings.reload();

    QVERIFY(!settings.readable());
    QVERIFY(settings.needsAttention());
    QVERIFY(status.count() > 0);
    QVERIFY2(settings.problem().contains(QStringLiteral("line 2")), qPrintable(settings.problem()));
    QCOMPARE(settings.value(QStringLiteral("glance")), QJsonValue(false));
    QVERIFY(!settings.set(QStringLiteral("tint-favicons"), true));
    QCOMPARE(readFile(path), broken);

    writeFile(path, R"({"version": 1, "glance": true})");
    settings.reload();
    QVERIFY(settings.readable());
    QVERIFY(!settings.needsAttention());
    QVERIFY(settings.problem().isEmpty());
    QCOMPARE(settings.value(QStringLiteral("glance")), QJsonValue(true));
}

void SettingsFileTest::startsOnTheDefaultsFromAFileOfAnotherVersion()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    const QByteArray later(R"({"version": 2, "glance": false})");
    writeFile(path, later);
    SettingsFile settings(root.path());

    QVERIFY(!settings.readable());
    QVERIFY2(
        settings.problem().contains(QStringLiteral("version 2")), qPrintable(settings.problem()));
    QCOMPARE(settings.value(QStringLiteral("glance")), QJsonValue(true));
    QVERIFY(!settings.set(QStringLiteral("glance"), false));
    QCOMPARE(readFile(path), later);
}

// One value of the wrong kind costs that setting alone. A key this build does
// not know is the reader's, perhaps from a newer build, and is listed rather
// than read or thrown away.
void SettingsFileTest::readsABadValueAsItsDefaultAndNamesIt()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    writeFile(path,
        R"({"version": 1, "glance": "no", "sidebar-side": "top", "tint-favicons": true,
            "sidebar-colour": "red"})");
    SettingsFile settings(root.path());

    QVERIFY(settings.readable());
    QVERIFY(settings.needsAttention());
    QCOMPARE(settings.value(QStringLiteral("glance")), QJsonValue(true));
    QCOMPARE(settings.value(QStringLiteral("sidebar-side")), QJsonValue(QStringLiteral("left")));
    QCOMPARE(settings.value(QStringLiteral("tint-favicons")), QJsonValue(true));
    QCOMPARE(settings.invalidKeys(),
        (QStringList {QStringLiteral("glance"), QStringLiteral("sidebar-side")}));
    QCOMPARE(settings.ignoredKeys(), QStringList {QStringLiteral("sidebar-colour")});
    const auto problem = settings.problem();
    QVERIFY2(problem.contains(QStringLiteral("glance"))
            && problem.contains(QStringLiteral("sidebar-side"))
            && problem.contains(QStringLiteral("sidebar-colour")),
        qPrintable(problem));

    // Fixing the value through Settings is a write like any other.
    QVERIFY(settings.set(QStringLiteral("glance"), false));
    QVERIFY(!settings.invalidKeys().contains(QStringLiteral("glance")));
    QVERIFY(readFile(path).contains("\"sidebar-colour\": \"red\""));
}

// The file is watched as theme.json is, so an edit lands without a restart,
// whether the editor writes in place or replaces the file, and whether the
// file was there when Omaweb started or not.
void SettingsFileTest::followsAnEditMadeWhileItRuns()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto config = root.filePath(QStringLiteral("config"));
    const auto path = QDir(config).filePath(QStringLiteral("settings.json"));
    SettingsFile settings(config);
    QSignalSpy changed(&settings, &SettingsFile::changed);

    writeFile(path, R"({"version": 1, "glance": false})");
    QTRY_COMPARE(settings.value(QStringLiteral("glance")), QJsonValue(false));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.takeFirst().at(0).toStringList(), QStringList {QStringLiteral("glance")});

    // An editor that saves by renaming a new file over the old one.
    const auto replacement = QDir(config).filePath(QStringLiteral("settings.json.new"));
    writeFile(replacement, R"({"version": 1, "glance": false, "https-only": false})");
    QFile::remove(path);
    QVERIFY(QFile::rename(replacement, path));
    QTRY_COMPARE(settings.value(QStringLiteral("https-only")), QJsonValue(false));

    writeFile(path, R"({"version": 1, "glance": false, "https-only": true})");
    QTRY_COMPARE(settings.value(QStringLiteral("https-only")), QJsonValue(true));
}

// A reader clicking through Settings while Sync restores keeps the file's lock
// busy, giving it up only for a moment between writes. A write waiting beside
// them takes the lock in one of those moments, not only if it happens to look
// at the right one of a few.
void SettingsFileTest::takesItsTurnBesideABusyWriter()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto lockPath = root.filePath(QStringLiteral("settings.json.lock"));
    std::atomic_bool done = false;
    std::atomic_bool holding = false;
    std::unique_ptr<QThread> busy(QThread::create([lockPath, &done, &holding] {
        QLockFile lock(lockPath);
        // A slow disk's writes, a millisecond apart, for longer than the
        // waiting write is allowed.
        for (int round = 0; !done && round < 200; ++round) {
            lock.lock();
            holding = true;
            QThread::msleep(40);
            lock.unlock();
            QThread::msleep(1);
        }
    }));
    busy->start();
    const auto stop = qScopeGuard([&done, &busy] {
        done = true;
        busy->wait(30000);
    });
    QTRY_VERIFY(holding);

    SettingsFile settings(root.path());
    QVERIFY(settings.set(QStringLiteral("glance"), false));
}

// Two writers on one file, as Sync's restore and the Settings page are, each
// with an instance of its own. Neither may write from a copy that misses the
// other's key.
void SettingsFileTest::keepsBothOfTwoWritesAtOnce()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto config = root.path();
    constexpr int rounds = 40;
    // Each writer flips its switch and ends on the value that is not its
    // default, so a write the other dropped would read as the default.
    const auto writer = [config](const QString &key, bool last) {
        return QThread::create([config, key, last] {
            SettingsFile settings(config);
            for (int round = 0; round < rounds; ++round) {
                settings.set(key, (rounds - 1 - round) % 2 == 0 ? last : !last);
            }
        });
    };
    std::unique_ptr<QThread> first(writer(QStringLiteral("tint-favicons"), true));
    std::unique_ptr<QThread> second(writer(QStringLiteral("floating-controls"), false));
    first->start();
    second->start();
    QVERIFY(first->wait(30000));
    QVERIFY(second->wait(30000));

    SettingsFile settings(config);
    QCOMPARE(settings.value(QStringLiteral("tint-favicons")), QJsonValue(true));
    QVERIFY(settings.isSet(QStringLiteral("tint-favicons")));
    QCOMPARE(settings.value(QStringLiteral("floating-controls")), QJsonValue(false));
    QVERIFY(settings.isSet(QStringLiteral("floating-controls")));
}

// The limits Settings offers are the only ones the file takes: a value the
// page cannot show would be a choice the reader could not see or change back.
void SettingsFileTest::offersOnlyTheLimitsSettingsOffers()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    writeFile(root.filePath(QStringLiteral("settings.json")),
        R"({"version": 1, "put-away-unused-tabs-after": 5})");
    SettingsFile settings(root.path());
    QCOMPARE(settings.invalidKeys(), QStringList {QStringLiteral("put-away-unused-tabs-after")});
    QCOMPARE(settings.value(QStringLiteral("put-away-unused-tabs-after")), QJsonValue(43200));
    QVERIFY(!settings.set(QStringLiteral("put-away-unused-tabs-after"), 7));
    for (const int offered : {0, 3600, 43200, 86400, 604800}) {
        QVERIFY(SettingsFile::accepts(QStringLiteral("put-away-unused-tabs-after"), offered));
    }
}

// The app icon is one of two, and a fresh profile has the black and white one
// the package installs.
void SettingsFileTest::offersTheTwoAppIcons()
{
    const auto key = QStringLiteral("app-icon");
    QCOMPARE(SettingsFile::defaultValue(key), QJsonValue(QStringLiteral("black-and-white")));
    QVERIFY(SettingsFile::accepts(key, QStringLiteral("black-and-white")));
    QVERIFY(SettingsFile::accepts(key, QStringLiteral("theme")));
    QVERIFY(!SettingsFile::accepts(key, QStringLiteral("green")));
}

// A key whose default is not Omaweb's to state reads as undefined while the
// reader has set nothing, so a caller's own fallback stands.
void SettingsFileTest::leavesAKeyWithoutADefaultUndefined()
{
    const SettingsFile settings({});
    for (const auto &key : {QStringLiteral("font-size"), QStringLiteral("page-fonts"),
             QStringLiteral("known-extensions"), QStringLiteral("download-directory"),
             QStringLiteral("secure-dns-template")}) {
        QVERIFY2(SettingsFile::defaultValue(key).isUndefined(), qPrintable(key));
        QVERIFY2(settings.value(key).isUndefined(), qPrintable(key));
    }
}

// The sidebar's opacity is a fraction the reader sets from half to whole, and
// none until they do, so the theme's own stands.
void SettingsFileTest::keepsASidebarOpacityBetweenHalfAndOne()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    SettingsFile settings(root.path());
    const auto key = QStringLiteral("sidebar-opacity");

    QVERIFY(SettingsFile::defaultValue(key).isUndefined());
    QVERIFY(settings.value(key).isUndefined());
    for (const double offered : {0.5, 0.65, 1.0}) {
        QVERIFY2(SettingsFile::accepts(key, offered), qPrintable(QString::number(offered)));
    }
    const QList<QJsonValue> refusals {QJsonValue(0.45), QJsonValue(1.05), QJsonValue(0),
        QJsonValue(QStringLiteral("half")), QJsonValue(true)};
    for (const auto &refused : refusals) {
        QVERIFY(!SettingsFile::accepts(key, refused));
    }

    QVERIFY(settings.set(key, 0.75));
    QCOMPARE(settings.value(key), QJsonValue(0.75));
    QVERIFY(readFile(path).contains("\"sidebar-opacity\": 0.75"));
    QVERIFY(!settings.set(key, 0.2));
    QCOMPARE(settings.value(key), QJsonValue(0.75));

    QVERIFY(settings.set(key, QJsonValue()));
    QVERIFY(!readFile(path).contains("sidebar-opacity"));
}

void SettingsFileTest::readsASidebarOpacityOutOfRangeAsTheThemes()
{
    const auto key = QStringLiteral("sidebar-opacity");
    for (const auto *bad : {"0.1", "2", "\"0.8\""}) {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        writeFile(root.filePath(QStringLiteral("settings.json")),
            R"({"version": 1, "sidebar-opacity": )" + QByteArray(bad) + "}");
        SettingsFile settings(root.path());
        QVERIFY2(settings.value(key).isUndefined(), bad);
        QVERIFY2(!settings.isSet(key), bad);
        QCOMPARE(settings.invalidKeys(), QStringList {key});
    }
}

// The chrome that asks for a setting as text reads a fraction as the number
// it is, not as the whole number it rounds to.
void SettingsFileTest::roundTripsAFractionThroughText()
{
    const auto key = QStringLiteral("sidebar-opacity");
    QCOMPARE(SettingsFile::text(QJsonValue(0.75)), QStringLiteral("0.75"));
    QCOMPARE(SettingsFile::text(QJsonValue(1.0)), QStringLiteral("1"));
    QCOMPARE(SettingsFile::fromText(key, QStringLiteral("0.75")), QJsonValue(0.75));
    QVERIFY(SettingsFile::fromText(key, QStringLiteral("high")).isUndefined());
    QVERIFY(SettingsFile::fromText(key, QString()).isNull());
}

// A switch in an object, written from Settings, goes into the object as the
// file holds it now: a member the reader added a moment ago, before the watch
// told this instance, stays.
void SettingsFileTest::writesOneMemberIntoWhatIsOnDisk()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    SettingsFile settings(root.path());
    writeFile(path, R"({"version": 1, "known-extensions": {"ublock": true}})");

    QVERIFY(
        settings.setMember(QStringLiteral("known-extensions"), QStringLiteral("bitwarden"), true));
    QCOMPARE(settings.value(QStringLiteral("known-extensions")),
        QJsonValue(
            QJsonObject {{QStringLiteral("ublock"), true}, {QStringLiteral("bitwarden"), true}}));

    QVERIFY(settings.setMember(QStringLiteral("known-extensions"), QStringLiteral("ublock"), {}));
    QVERIFY(
        settings.setMember(QStringLiteral("known-extensions"), QStringLiteral("bitwarden"), {}));
    QVERIFY(!readFile(path).contains("known-extensions"));
    QVERIFY(!settings.setMember(QStringLiteral("known-extensions"), QStringLiteral("x"), 3));
}

QTEST_GUILESS_MAIN(SettingsFileTest)
#include "tst_settingsfile.moc"
