#include "SettingsFile.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

using omaweb::SettingsFile;

class SettingsFileTest final : public QObject {
    Q_OBJECT

private slots:
    void readsTheReadersTypedValues();
    void rewritesTheFileAsTheReaderLaidItOut();
    void writesOnlyWhatDiffersFromTheDefault();
    void keepsTheLastGoodValuesWhileTheFileCannotBeRead();
    void startsOnTheDefaultsFromAFileOfAnotherVersion();
    void readsABadValueAsItsDefaultAndNamesIt();
    void followsAnEditMadeWhileItRuns();
    void keepsBothOfTwoWritesAtOnce();
    void offersOnlyTheLimitsSettingsOffers();
    void leavesAKeyWithoutADefaultUndefined();
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
