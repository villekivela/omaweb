#include "GlobalPrivacyControl.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::GlobalPrivacyControl;

class GlobalPrivacyControlTest final : public QObject {
    Q_OBJECT

private slots:
    void isOnWithoutBeingSetUp();
    void keepsTheReadersChoiceAcrossARestart();
    void readsAFileItCannotUseAsTheDefault_data();
    void readsAFileItCannotUseAsTheDefault();
    void writesNothingForAChoiceAlreadyMade();
    void namesTheSignalTheEnginesSend();
};

// A reader who chose a blocking browser is not asked to turn the signal on:
// the configuration directory is empty on a first run and the answer is yes.
void GlobalPrivacyControlTest::isOnWithoutBeingSetUp()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const GlobalPrivacyControl control(root.filePath(QStringLiteral("config")));
    QVERIFY(control.enabled());
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/privacy.json"))));
}

// Off is the reader's decision, so it is written where the reader's other
// decisions live (ADR 0016) and a second instance reads it back.
void GlobalPrivacyControlTest::keepsTheReadersChoiceAcrossARestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto configRoot = root.filePath(QStringLiteral("config"));
    {
        GlobalPrivacyControl control(configRoot);
        QSignalSpy changed(&control, &GlobalPrivacyControl::enabledChanged);
        control.setEnabled(false);
        QVERIFY(!control.enabled());
        QCOMPARE(changed.count(), 1);
    }
    {
        GlobalPrivacyControl restarted(configRoot);
        QVERIFY(!restarted.enabled());
        restarted.setEnabled(true);
    }
    const GlobalPrivacyControl again(configRoot);
    QVERIFY(again.enabled());
}

void GlobalPrivacyControlTest::readsAFileItCannotUseAsTheDefault_data()
{
    QTest::addColumn<QByteArray>("contents");
    QTest::newRow("not json") << QByteArray("global-privacy-control = false");
    QTest::newRow("not an object") << QByteArray("[false]");
    QTest::newRow("another key") << QByteArray(R"({"do-not-track": false})");
    QTest::newRow("not a bool") << QByteArray(R"({"global-privacy-control": "off"})");
}

// A file the reader edited by hand and got wrong turns the signal off for
// nobody: the default is the answer until the file says otherwise in a form
// that can be read.
void GlobalPrivacyControlTest::readsAFileItCannotUseAsTheDefault()
{
    QFETCH(QByteArray, contents);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath(QStringLiteral("config")));
    QFile file(root.filePath(QStringLiteral("config/privacy.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
    file.close();
    const GlobalPrivacyControl control(root.filePath(QStringLiteral("config")));
    QVERIFY(control.enabled());
}

// Setting the value it already has is not a change: nothing is written and
// nobody listening is told to re-attach anything.
void GlobalPrivacyControlTest::writesNothingForAChoiceAlreadyMade()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    GlobalPrivacyControl control(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&control, &GlobalPrivacyControl::enabledChanged);
    control.setEnabled(true);
    QCOMPARE(changed.count(), 0);
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/privacy.json"))));
}

// The header and the script property are the two halves the specification
// requires, and every engine adapter sends the same ones.
void GlobalPrivacyControlTest::namesTheSignalTheEnginesSend()
{
    QCOMPARE(GlobalPrivacyControl::headerName(), QByteArray("Sec-GPC"));
    QCOMPARE(GlobalPrivacyControl::headerValue(), QByteArray("1"));
    QVERIFY(GlobalPrivacyControl::scriptSource().contains(QStringLiteral("globalPrivacyControl")));
}

QTEST_GUILESS_MAIN(GlobalPrivacyControlTest)

#include "tst_globalprivacycontrol.moc"
