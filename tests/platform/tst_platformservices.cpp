#include "ProcessResources.h"
#include "SystemNotifier.h"

#include <QGuiApplication>
#include <QTest>

#include <unistd.h>

using tanto::ProcessResources;
using tanto::SystemNotifier;

class PlatformServicesTest final : public QObject {
    Q_OBJECT

private slots:
    void reportsWhatAProcessHolds();
    void refusesToPresentWithoutANotificationService();
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

QTEST_MAIN(PlatformServicesTest)

#include "tst_platformservices.moc"
