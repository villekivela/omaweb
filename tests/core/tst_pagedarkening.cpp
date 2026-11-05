#include "PageDarkening.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::PageDarkening;

class PageDarkeningTest final : public QObject {
    Q_OBJECT

private slots:
    void isOffWithoutBeingSetUp();
    void keepsTheReadersChoiceAcrossARestart();
    void readsAFileItCannotUseAsOff_data();
    void readsAFileItCannotUseAsOff();
    void writesNothingForAChoiceAlreadyMade();
    void followsAnEditMadeToTheFile();
};

// Pages are shown as they were written until the reader asks otherwise, so a
// first run, with no configuration directory yet, leaves them alone.
void PageDarkeningTest::isOffWithoutBeingSetUp()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const PageDarkening darkening(root.filePath(QStringLiteral("config")));
    QVERIFY(!darkening.enabled());
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/settings.json"))));
}

// On is the reader's decision, so it is written where the reader's other
// decisions live (ADR 0016), under a key of its own, and a second instance
// reads it back.
void PageDarkeningTest::keepsTheReadersChoiceAcrossARestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto configRoot = root.filePath(QStringLiteral("config"));
    {
        PageDarkening darkening(configRoot);
        QSignalSpy changed(&darkening, &PageDarkening::enabledChanged);
        darkening.setEnabled(true);
        QVERIFY(darkening.enabled());
        QCOMPARE(changed.count(), 1);
    }
    QFile file(root.filePath(QStringLiteral("config/settings.json")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.readAll().contains("\"darken-pages\": true"));
    file.close();
    PageDarkening restarted(configRoot);
    QVERIFY(restarted.enabled());
    restarted.setEnabled(false);
    const PageDarkening again(configRoot);
    QVERIFY(!again.enabled());
}

void PageDarkeningTest::readsAFileItCannotUseAsOff_data()
{
    QTest::addColumn<QByteArray>("contents");
    QTest::newRow("not json") << QByteArray("darken-pages = true");
    QTest::newRow("not an object") << QByteArray("[true]");
    QTest::newRow("another key") << QByteArray(R"({"https-only": false})");
    QTest::newRow("not a bool") << QByteArray(R"({"darken-pages": "on"})");
}

// A file the reader edited by hand and got wrong darkens nothing: off is the
// answer until the file says otherwise in a form that can be read.
void PageDarkeningTest::readsAFileItCannotUseAsOff()
{
    QFETCH(QByteArray, contents);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath(QStringLiteral("config")));
    QFile file(root.filePath(QStringLiteral("config/settings.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
    file.close();
    const PageDarkening darkening(root.filePath(QStringLiteral("config")));
    QVERIFY(!darkening.enabled());
}

void PageDarkeningTest::writesNothingForAChoiceAlreadyMade()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    PageDarkening darkening(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&darkening, &PageDarkening::enabledChanged);
    darkening.setEnabled(false);
    QCOMPARE(changed.count(), 0);
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/settings.json"))));
}

// Sync replaces the file under a running browser, and the pages follow it.
void PageDarkeningTest::followsAnEditMadeToTheFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto configRoot = root.filePath(QStringLiteral("config"));
    PageDarkening darkening(configRoot);
    PageDarkening other(configRoot);
    QSignalSpy changed(&other, &PageDarkening::enabledChanged);
    darkening.setEnabled(true);
    QTRY_VERIFY(other.enabled());
    QCOMPARE(changed.count(), 1);
}

QTEST_GUILESS_MAIN(PageDarkeningTest)

#include "tst_pagedarkening.moc"
