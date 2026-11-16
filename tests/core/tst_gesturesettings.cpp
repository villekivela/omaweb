#include "GestureSettings.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::GestureSettings;

class GestureSettingsTest final : public QObject {
    Q_OBJECT

private slots:
    void startsOnTheDefaults();
    void writesOnlyWhatTheReaderChanged();
    void keepsTheChoicesAcrossARestart();
    void takesTheNearestStepWithinTheRange();
    void refusesAnActionItDoesNotKnow();
    void followsAnEditOfTheFile();
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

// Two fingers over the page go back and forward and over the sidebar switch
// Spaces, as they did before the page had a say; three fingers switch tabs.
void GestureSettingsTest::startsOnTheDefaults()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const GestureSettings gestures(root.path());
    QCOMPARE(gestures.pageAction(), QStringLiteral("back-and-forward"));
    QCOMPARE(gestures.sidebarAction(), QStringLiteral("switch-space"));
    QCOMPARE(gestures.threeFingerAction(), QStringLiteral("switch-tab"));
    QCOMPARE(gestures.distance(), 120);
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("settings.json"))));
}

// A choice goes into "gestures" alone, and choosing the default again takes it
// out, so a default a later build changes reaches a reader who never chose.
void GestureSettingsTest::writesOnlyWhatTheReaderChanged()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    GestureSettings gestures(root.path());
    QSignalSpy changed(&gestures, &GestureSettings::changed);

    gestures.setAction(QStringLiteral("page"), QStringLiteral("switch-tab"));
    QCOMPARE(gestures.pageAction(), QStringLiteral("switch-tab"));
    QCOMPARE(changed.count(), 1);
    QVERIFY(readFile(path).contains("\"page\": \"switch-tab\""));
    QVERIFY(!readFile(path).contains("sidebar"));

    gestures.setAction(QStringLiteral("page"), QStringLiteral("back-and-forward"));
    QCOMPARE(gestures.pageAction(), QStringLiteral("back-and-forward"));
    QVERIFY(!readFile(path).contains("gestures"));
}

void GestureSettingsTest::keepsTheChoicesAcrossARestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    {
        GestureSettings gestures(root.path());
        gestures.setAction(QStringLiteral("sidebar"), QStringLiteral("nothing"));
        gestures.setAction(QStringLiteral("three-fingers"), QStringLiteral("switch-space"));
        gestures.setAction(QStringLiteral("page"), QStringLiteral("switch-tab"));
        gestures.setDistance(180);
    }
    const GestureSettings restarted(root.path());
    QCOMPARE(restarted.pageAction(), QStringLiteral("switch-tab"));
    QCOMPARE(restarted.sidebarAction(), QStringLiteral("nothing"));
    QCOMPARE(restarted.threeFingerAction(), QStringLiteral("switch-space"));
    QCOMPARE(restarted.distance(), 180);
}

// The slider moves by 10 pixels from 60 to 240.
void GestureSettingsTest::takesTheNearestStepWithinTheRange()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    GestureSettings gestures(root.path());

    gestures.setDistance(154);
    QCOMPARE(gestures.distance(), 150);
    QVERIFY(readFile(path).contains("\"distance\": 150"));
    gestures.setDistance(20);
    QCOMPARE(gestures.distance(), 60);
    gestures.setDistance(1000);
    QCOMPARE(gestures.distance(), 240);
    gestures.setDistance(117);
    QCOMPARE(gestures.distance(), 120);
    QVERIFY(!readFile(path).contains("gestures"));
}

void GestureSettingsTest::refusesAnActionItDoesNotKnow()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    GestureSettings gestures(root.path());
    QSignalSpy changed(&gestures, &GestureSettings::changed);

    gestures.setAction(QStringLiteral("page"), QStringLiteral("zoom"));
    gestures.setAction(QStringLiteral("four-fingers"), QStringLiteral("switch-tab"));
    QCOMPARE(gestures.pageAction(), QStringLiteral("back-and-forward"));
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("settings.json"))));
}

// An edit of settings.json, by hand or by Sync, applies while Omaweb runs.
void GestureSettingsTest::followsAnEditOfTheFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    GestureSettings gestures(root.path());
    QSignalSpy changed(&gestures, &GestureSettings::changed);

    writeFile(path, R"({"version": 1, "gestures": {"three-fingers": "nothing", "distance": 90}})");
    QTRY_COMPARE(gestures.threeFingerAction(), QStringLiteral("nothing"));
    QCOMPARE(gestures.distance(), 90);
    QCOMPARE(changed.count(), 1);

    writeFile(path, R"({"version": 1})");
    QTRY_COMPARE(gestures.threeFingerAction(), QStringLiteral("switch-tab"));
    QCOMPARE(gestures.distance(), 120);
}

QTEST_MAIN(GestureSettingsTest)
#include "tst_gesturesettings.moc"
