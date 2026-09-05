#include "ProcessResources.h"
#include "SavedDownload.h"
#include "SystemNotifier.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QTest>

#include <sys/xattr.h>
#include <unistd.h>

using omaweb::ProcessResources;
using omaweb::SavedDownload;
using omaweb::SystemNotifier;

class PlatformServicesTest final : public QObject {
    Q_OBJECT

private slots:
    void reportsWhatAProcessHolds();
    void refusesToPresentWithoutANotificationService();
    void marksAFinishedDownloadWithWhereItCameFrom();
    void takesTheExecuteBitsOffEveryDownload();
    void refusesToTouchWhatIsNotAFinishedDownload();
};

// A retained tab keeps a renderer running for a Space the reader is not looking
// at, and the browser has to be able to say what that holds. The process it
// asks about here is its own, which is the one process a test can be sure of.
void PlatformServicesTest::reportsWhatAProcessHolds()
{
    ProcessResources resources;
    QVERIFY(resources.available());
    QVERIFY(resources.residentBytes(getpid()) > 0);
    // Nothing running costs nothing, and a number is never invented for it: a
    // page with no renderer, and a process id that names no process.
    QCOMPARE(resources.residentBytes(0), 0);
    QCOMPARE(resources.residentBytes(-1), 0);
}

// The tests run without a window server, which is a desktop with no
// notification service. A page's request must then go unanswered rather than
// being swallowed as though the reader had seen it — the shell reads this to
// tell the page its notification closed.
void PlatformServicesTest::refusesToPresentWithoutANotificationService()
{
    SystemNotifier notifier;
    QCOMPARE(QGuiApplication::platformName(), QStringLiteral("offscreen"));
    QVERIFY(!notifier.available());
    QVERIFY(!notifier.present(QStringLiteral("space:1"), QStringLiteral("origin · Space"),
        QStringLiteral("Something happened")));
    // Withdrawing one that was never shown is not an error.
    notifier.withdraw(QStringLiteral("space:1"));
}

void PlatformServicesTest::marksAFinishedDownloadWithWhereItCameFrom()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = QDir(directory.path()).filePath(QStringLiteral("archive.zip"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("PK\x03\x04", 4), 4);
    file.close();

    SavedDownload saved;
    QVERIFY(saved.quarantineAvailable());
    QVERIFY(saved.quarantine(path, QUrl(QStringLiteral("https://files.example/archive.zip")),
        QUrl(QStringLiteral("https://files.example/downloads"))));

    const auto attribute = [&path](const char *name) {
        const auto native = QFile::encodeName(path);
        char value[512] = {};
#if defined(Q_OS_MACOS)
        const auto size = ::getxattr(native.constData(), name, value, sizeof(value), 0, 0);
#else
        const auto size = ::getxattr(native.constData(), name, value, sizeof(value));
#endif
        return size > 0 ? QByteArray(value, size) : QByteArray();
    };
#if defined(Q_OS_MACOS)
    QVERIFY(attribute("com.apple.quarantine").startsWith("0083;"));
    QVERIFY(attribute("com.apple.quarantine").contains(";Omaweb;"));
#endif
    QCOMPARE(attribute("user.xdg.origin.url"), "https://files.example/archive.zip");
    QCOMPARE(attribute("user.xdg.referrer.url"), "https://files.example/downloads");
}

void PlatformServicesTest::takesTheExecuteBitsOffEveryDownload()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = QDir(directory.path()).filePath(QStringLiteral("install.sh"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("#!/bin/sh\n"), 10);
    file.close();
    QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
        | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther));

    SavedDownload saved;
    QVERIFY(saved.quarantine(path, QUrl(QStringLiteral("https://files.example/install.sh")), {}));

    const auto permissions = QFileInfo(path).permissions();
    QVERIFY(!permissions.testFlag(QFileDevice::ExeOwner));
    QVERIFY(!permissions.testFlag(QFileDevice::ExeGroup));
    QVERIFY(!permissions.testFlag(QFileDevice::ExeOther));
    QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
}

void PlatformServicesTest::refusesToTouchWhatIsNotAFinishedDownload()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SavedDownload saved;
    const auto missing = QDir(directory.path()).filePath(QStringLiteral("nothing.zip"));
    QVERIFY(!saved.quarantine(missing, QUrl(QStringLiteral("https://files.example/x")), {}));
    QVERIFY(!saved.quarantine(QString(), {}, {}));
    QVERIFY(!saved.quarantine(directory.path(), {}, {}));
    QVERIFY(!saved.reveal(missing));
    QVERIFY(!saved.reveal(QString()));
}

QTEST_MAIN(PlatformServicesTest)

#include "tst_platformservices.moc"
