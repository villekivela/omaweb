#include "DownloadPolicy.h"

#include <QTest>

using omaweb::DownloadPolicy::riskKind;

class DownloadPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void namesTheKindOfFileTheReaderIsBeingHanded_data();
    void namesTheKindOfFileTheReaderIsBeingHanded();
    void readsTheKindFromTheTypeWhereTheNameCannotSay_data();
    void readsTheKindFromTheTypeWhereTheNameCannotSay();
    void readsTheNameTheFilesystemWouldKeep_data();
    void readsTheNameTheFilesystemWouldKeep();
    void namesNoKindForAnEverydayDocument_data();
    void namesNoKindForAnEverydayDocument();
};

void DownloadPolicyTest::namesTheKindOfFileTheReaderIsBeingHanded_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("kind");

    QTest::newRow("Windows program") << QStringLiteral("setup.exe")
        << QStringLiteral("executable");
    QTest::newRow("Java program") << QStringLiteral("tool.jar")
        << QStringLiteral("executable");
    QTest::newRow("shell script") << QStringLiteral("install.sh")
        << QStringLiteral("script");
    QTest::newRow("PowerShell script") << QStringLiteral("bootstrap.ps1")
        << QStringLiteral("script");
    // A desktop entry is a program the desktop runs on a double-click, whatever
    // its icon says it is.
    QTest::newRow("desktop entry") << QStringLiteral("Invoice.desktop")
        << QStringLiteral("script");
    QTest::newRow("Debian package") << QStringLiteral("omaweb_1.0_amd64.deb")
        << QStringLiteral("installer");
    QTest::newRow("Arch package") << QStringLiteral("omaweb-1.0-x86_64.pkg.tar.zst")
        << QStringLiteral("archive");
    QTest::newRow("AppImage") << QStringLiteral("Editor-x86_64.AppImage")
        << QStringLiteral("installer");
    QTest::newRow("Apple disk image") << QStringLiteral("Omaweb.dmg")
        << QStringLiteral("disk image");
    QTest::newRow("optical image") << QStringLiteral("arch.iso")
        << QStringLiteral("disk image");
    QTest::newRow("zip archive") << QStringLiteral("photos.zip")
        << QStringLiteral("archive");
    QTest::newRow("compressed tarball") << QStringLiteral("sources.tar.gz")
        << QStringLiteral("archive");
}

void DownloadPolicyTest::namesTheKindOfFileTheReaderIsBeingHanded()
{
    QFETCH(QString, fileName);
    QFETCH(QString, kind);
    QCOMPARE(riskKind(fileName, {}), kind);
    // The name decides on its own, and an innocent type does not talk it round.
    QCOMPARE(riskKind(fileName, QStringLiteral("text/plain")), kind);
}

void DownloadPolicyTest::readsTheKindFromTheTypeWhereTheNameCannotSay_data()
{
    QTest::addColumn<QString>("mimeType");
    QTest::addColumn<QString>("kind");

    QTest::newRow("program") << QStringLiteral("application/x-executable")
        << QStringLiteral("executable");
    QTest::newRow("shell script") << QStringLiteral("application/x-shellscript")
        << QStringLiteral("script");
    QTest::newRow("Android package")
        << QStringLiteral("application/vnd.android.package-archive")
        << QStringLiteral("installer");
    QTest::newRow("Apple disk image") << QStringLiteral("application/x-apple-diskimage")
        << QStringLiteral("disk image");
    QTest::newRow("zip archive") << QStringLiteral("application/zip")
        << QStringLiteral("archive");
    // The type is read case-insensitively and without its parameters, because
    // that is how a server is entitled to send it.
    QTest::newRow("type with a parameter")
        << QStringLiteral("Application/Zip; charset=binary")
        << QStringLiteral("archive");
}

void DownloadPolicyTest::readsTheKindFromTheTypeWhereTheNameCannotSay()
{
    QFETCH(QString, mimeType);
    QFETCH(QString, kind);
    QCOMPARE(riskKind(QStringLiteral("attachment"), mimeType), kind);
}

void DownloadPolicyTest::readsTheNameTheFilesystemWouldKeep_data()
{
    QTest::addColumn<QString>("fileName");

    QTest::newRow("upper case") << QStringLiteral("SETUP.EXE");
    QTest::newRow("padded") << QStringLiteral("  setup.exe  ");
    // Trailing dots and spaces are dropped by the filesystems that accept the
    // name at all, so the extension that lands is the one before them.
    QTest::newRow("trailing dot") << QStringLiteral("setup.exe.");
    QTest::newRow("trailing dots and spaces") << QStringLiteral("setup.exe. . .");
    QTest::newRow("full path") << QStringLiteral("/home/reader/Downloads/setup.exe");
}

void DownloadPolicyTest::readsTheNameTheFilesystemWouldKeep()
{
    QFETCH(QString, fileName);
    QCOMPARE(riskKind(fileName, {}), QStringLiteral("executable"));
}

void DownloadPolicyTest::namesNoKindForAnEverydayDocument_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("mimeType");

    QTest::newRow("PDF") << QStringLiteral("invoice.pdf")
        << QStringLiteral("application/pdf");
    QTest::newRow("image") << QStringLiteral("diagram.png") << QStringLiteral("image/png");
    QTest::newRow("text") << QStringLiteral("notes.txt") << QStringLiteral("text/plain");
    QTest::newRow("spreadsheet") << QStringLiteral("budget.csv") << QStringLiteral("text/csv");
    QTest::newRow("no name at all") << QString() << QString();
    // A name whose last extension is harmless is harmless, even where an
    // earlier one is not: the filesystem hands the last one to the desktop.
    QTest::newRow("executable renamed") << QStringLiteral("setup.exe.txt")
        << QStringLiteral("text/plain");
}

void DownloadPolicyTest::namesNoKindForAnEverydayDocument()
{
    QFETCH(QString, fileName);
    QFETCH(QString, mimeType);
    QCOMPARE(riskKind(fileName, mimeType), QString());
}

QTEST_APPLESS_MAIN(DownloadPolicyTest)
#include "tst_downloadpolicy.moc"
