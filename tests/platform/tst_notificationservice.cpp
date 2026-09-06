#include "SystemNotifier.h"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

using omaweb::SystemNotifier;

namespace {

const auto kService = QStringLiteral("org.freedesktop.Notifications");
const auto kPath = QStringLiteral("/org/freedesktop/Notifications");

// What the desktop's notification daemon does, reduced to what the seam depends
// on: it hands back an identifier, remembers what it was asked, and can be told
// to report an answer. The real daemon draws something and waits for a person,
// neither of which a test can do.
class StubNotificationService final : public QObject {
    Q_OBJECT

public:
    struct Request {
        QString applicationName;
        uint replacesId = 0;
        QString summary;
        QString body;
        QStringList actions;
        int timeout = -1;
    };

    QList<Request> requests;
    QList<uint> closed;

    void reportActionInvoked(uint id, const QString &action) { emit ActionInvoked(id, action); }

    void reportClosed(uint id, uint reason) { emit NotificationClosed(id, reason); }

public slots:
    uint Notify(const QString &applicationName, uint replacesId, const QString &icon,
        const QString &summary, const QString &body, const QStringList &actions,
        const QVariantMap &hints, int timeout)
    {
        Q_UNUSED(icon)
        Q_UNUSED(hints)
        requests.append(Request {applicationName, replacesId, summary, body, actions, timeout});
        // A daemon replacing a notification may keep the identifier or issue a
        // new one. Keeping it is the harder case for the seam's bookkeeping,
        // because the same number then stands for two requests.
        return replacesId != 0 ? replacesId : ++m_nextId;
    }

    void CloseNotification(uint id) { closed.append(id); }

signals:
    void ActionInvoked(uint id, const QString &action);
    void NotificationClosed(uint id, uint reason);

private:
    uint m_nextId = 0;
};

// Puts the stub on the bus under the name the seam calls, so the adaptor is what
// exports the slots and signals above rather than the object itself.
class NotificationAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    explicit NotificationAdaptor(StubNotificationService *service)
        : QDBusAbstractAdaptor(service)
    {
        setAutoRelaySignals(true);
    }

public slots:
    uint Notify(const QString &applicationName, uint replacesId, const QString &icon,
        const QString &summary, const QString &body, const QStringList &actions,
        const QVariantMap &hints, int timeout)
    {
        return service()->Notify(
            applicationName, replacesId, icon, summary, body, actions, hints, timeout);
    }

    void CloseNotification(uint id) { service()->CloseNotification(id); }

signals:
    void ActionInvoked(uint id, const QString &action);
    void NotificationClosed(uint id, uint reason);

private:
    StubNotificationService *service() const
    {
        return static_cast<StubNotificationService *>(parent());
    }
};

} // namespace

class NotificationServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void namesOmawebAndAsksForAnAnswerableNotification();
    void reportsAnInvokedActionOnceAndNotTheClosingThatFollows();
    void reportsAClosedNotificationAsDismissed();
    void saysNothingAboutANotificationOmawebWithdrew();
    void replacesANotificationAlreadyOnScreenForTheSameKey();

private:
    // One per test function would be one bus registration per test, and the
    // seam's own state is process-wide, so both are made once and each test
    // works on a key of its own.
    StubNotificationService *m_service = nullptr;
    SystemNotifier *m_notifier = nullptr;

    uint lastId() const { return static_cast<uint>(m_service->requests.size()); }
};

void NotificationServiceTest::initTestCase()
{
    auto bus = QDBusConnection::sessionBus();
    QVERIFY2(bus.isConnected(), "the test needs a session bus, run it under dbus-run-session");

    m_service = new StubNotificationService;
    new NotificationAdaptor(m_service);
    QVERIFY(bus.registerObject(kPath, m_service));
    // Registered before the notifier is built, because the seam matches the
    // daemon's signals by who owns this name.
    QVERIFY(bus.registerService(kService));

    m_notifier = new SystemNotifier(this);
    QVERIFY(m_notifier->available());
}

// The daemon is told who is asking and given an action to report back through.
// Without an action there is nothing for a reader to answer, and the page would
// wait forever.
void NotificationServiceTest::namesOmawebAndAsksForAnAnswerableNotification()
{
    QVERIFY(m_notifier->present(QStringLiteral("first"), QStringLiteral("example.com · Personal"),
        QStringLiteral("Build finished")));
    QCOMPARE(m_service->requests.size(), 1);

    const auto request = m_service->requests.constLast();
    QCOMPARE(request.applicationName, QStringLiteral("Omaweb"));
    QCOMPARE(request.summary, QStringLiteral("example.com · Personal"));
    QCOMPARE(request.body, QStringLiteral("Build finished"));
    QVERIFY(request.actions.contains(QStringLiteral("default")));
    // Nothing new to replace on a key the desktop has not been told about.
    QCOMPARE(request.replacesId, 0u);
    // No expiry is asked for. A daemon may overrule it, which the seam reports
    // as a dismissal rather than assuming cannot happen.
    QCOMPARE(request.timeout, 0);
}

// The reader answering is one event to the page and two on the bus: the daemon
// reports the action and then reports the notification closed. The page hears
// about it once.
void NotificationServiceTest::reportsAnInvokedActionOnceAndNotTheClosingThatFollows()
{
    const auto key = QStringLiteral("invoked");
    QVERIFY(m_notifier->present(key, QStringLiteral("origin · Space"), QStringLiteral("Body")));
    const auto id = lastId();

    QSignalSpy activated(m_notifier, &SystemNotifier::activated);
    QSignalSpy dismissed(m_notifier, &SystemNotifier::dismissed);

    m_service->reportActionInvoked(id, QStringLiteral("default"));
    QTRY_COMPARE(activated.size(), 1);
    QCOMPARE(activated.constFirst().constFirst().toString(), key);

    // The daemon closes what the reader acted on. The answer is already given,
    // so this is not a second one.
    m_service->reportClosed(id, 2);
    QTest::qWait(50);
    QCOMPARE(activated.size(), 1);
    QCOMPARE(dismissed.size(), 0);
}

// Expired, or swept away by the reader without acting on it. Either way they did
// not answer, and the page has to stop waiting.
void NotificationServiceTest::reportsAClosedNotificationAsDismissed()
{
    const auto key = QStringLiteral("closed");
    QVERIFY(m_notifier->present(key, QStringLiteral("origin · Space"), QStringLiteral("Body")));
    const auto id = lastId();

    QSignalSpy activated(m_notifier, &SystemNotifier::activated);
    QSignalSpy dismissed(m_notifier, &SystemNotifier::dismissed);

    m_service->reportClosed(id, 2);
    QTRY_COMPARE(dismissed.size(), 1);
    QCOMPARE(dismissed.constFirst().constFirst().toString(), key);
    QCOMPARE(activated.size(), 0);

    // A daemon repeating itself, or reporting a notification twice, is not a
    // second answer either.
    m_service->reportClosed(id, 2);
    QTest::qWait(50);
    QCOMPARE(dismissed.size(), 1);
}

// The page, tab or Space went away, so Omaweb took the notification down. There
// is nobody left to tell, and the closing the daemon reports back is Omaweb's
// own doing.
void NotificationServiceTest::saysNothingAboutANotificationOmawebWithdrew()
{
    const auto key = QStringLiteral("withdrawn");
    QVERIFY(m_notifier->present(key, QStringLiteral("origin · Space"), QStringLiteral("Body")));
    const auto id = lastId();

    QSignalSpy activated(m_notifier, &SystemNotifier::activated);
    QSignalSpy dismissed(m_notifier, &SystemNotifier::dismissed);

    m_notifier->withdraw(key);
    QTRY_VERIFY(m_service->closed.contains(id));

    m_service->reportClosed(id, 3);
    QTest::qWait(50);
    QCOMPARE(activated.size(), 0);
    QCOMPARE(dismissed.size(), 0);

    // Withdrawing again asks the desktop for nothing: the key is already gone.
    const auto asked = m_service->closed.size();
    m_notifier->withdraw(key);
    QCOMPARE(m_service->closed.size(), asked);
}

// The page is asking about the same thing it asked about before, so the reader
// gets one notification rather than a stack of them.
void NotificationServiceTest::replacesANotificationAlreadyOnScreenForTheSameKey()
{
    const auto key = QStringLiteral("replaced");
    QVERIFY(m_notifier->present(key, QStringLiteral("origin · Space"), QStringLiteral("First")));
    const auto id = lastId();

    QVERIFY(m_notifier->present(key, QStringLiteral("origin · Space"), QStringLiteral("Second")));
    QCOMPARE(m_service->requests.constLast().replacesId, id);

    // Still one notification, and still answerable under the same key.
    QSignalSpy dismissed(m_notifier, &SystemNotifier::dismissed);
    m_service->reportClosed(id, 2);
    QTRY_COMPARE(dismissed.size(), 1);
    QCOMPARE(dismissed.constFirst().constFirst().toString(), key);
}

QTEST_MAIN(NotificationServiceTest)

#include "tst_notificationservice.moc"
