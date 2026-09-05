#include "SystemNotifier.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QHash>
#include <QPointer>
#include <QVariantMap>

namespace omaweb {
namespace {

    const auto kService = QStringLiteral("org.freedesktop.Notifications");
    const auto kPath = QStringLiteral("/org/freedesktop/Notifications");

    // `NotificationClosed` carries why it closed. Only the reasons that mean the
    // reader is done with it are answers; the one Omaweb caused itself is not.
    constexpr uint kClosedByRequest = 3;

    // A local daemon answers in well under a millisecond, so a bounded blocking
    // call keeps `present` able to say whether the desktop took the notification.
    // The bound is what matters: a daemon that stops answering stalls one call
    // rather than the interface.
    constexpr int kCallTimeoutMs = 1000;

    // The desktop identifies a notification by a number it assigns, and Omaweb by
    // a key of its own. Holding a key here is what "still waiting for an answer"
    // means, so the first terminal event takes it and every later one about the
    // same notification finds nothing and reports nothing. That is what keeps an
    // `ActionInvoked` and the `NotificationClosed` that follows it from being
    // reported as two different answers.
    QHash<uint, QString> keyById;
    QHash<QString, uint> idByKey;

    // The subscription is the process's, one per session bus, and it outlives any
    // single notifier. It reports to whichever notifier is current, and to none
    // once that notifier is gone.
    QPointer<SystemNotifier> currentNotifier;

    QString takeKey(uint id)
    {
        const auto key = keyById.take(id);
        if (!key.isEmpty()) {
            idByKey.remove(key);
        }
        return key;
    }

    bool serviceReachable()
    {
        auto *bus = QDBusConnection::sessionBus().interface();
        if (!bus) {
            return false;
        }
        if (bus->isServiceRegistered(kService)) {
            return true;
        }
        // A daemon that starts on demand is not registered until something asks.
        // It can still answer, so it counts as present.
        return bus->activatableServiceNames().value().contains(kService);
    }

    // The D-Bus signals arrive by name rather than through a typed proxy, so a
    // receiver with real slots is what the connection needs. It is the file's
    // own, which keeps every D-Bus detail out of the shared header.
    class NotificationListener final : public QObject {
        Q_OBJECT

    public:
        using QObject::QObject;

    public slots:
        void onActionInvoked(uint id, const QString &action)
        {
            Q_UNUSED(action)
            const auto key = takeKey(id);
            if (key.isEmpty()) {
                return;
            }
            if (auto *notifier = currentNotifier.data()) {
                emit notifier->activated(key);
            }
        }

        void onNotificationClosed(uint id, uint reason)
        {
            // Omaweb asked for this one to go, so there is nobody to tell.
            // `withdraw` has already taken the key; this guards a daemon that
            // reports the reason for a notification Omaweb never withdrew.
            if (reason == kClosedByRequest) {
                takeKey(id);
                return;
            }
            const auto key = takeKey(id);
            if (key.isEmpty()) {
                return;
            }
            // Expired, dismissed, or closed for a reason the daemon does not
            // name: the reader did not answer it, and the page has to hear that
            // rather than keep waiting.
            if (auto *notifier = currentNotifier.data()) {
                emit notifier->dismissed(key);
            }
        }
    };

    NotificationListener *listener()
    {
        static NotificationListener *shared = [] {
            auto *created = new NotificationListener;
            auto bus = QDBusConnection::sessionBus();
            bus.connect(kService, kPath, kService, QStringLiteral("ActionInvoked"), created,
                SLOT(onActionInvoked(uint, QString)));
            bus.connect(kService, kPath, kService, QStringLiteral("NotificationClosed"), created,
                SLOT(onNotificationClosed(uint, uint)));
            return created;
        }();
        return shared;
    }

} // namespace

SystemNotifier::SystemNotifier(QObject *parent)
    : QObject(parent)
{
    listener();
    currentNotifier = this;
}

SystemNotifier::~SystemNotifier()
{
    if (currentNotifier == this) {
        currentNotifier.clear();
    }
}

bool SystemNotifier::available() const
{
    // The property is constant, so the desktop is asked once and the answer
    // stands for the session. A desktop that gains a notification daemon while
    // the browser is running is not a case worth re-reading a property for.
    static const bool reachable = serviceReachable();
    return reachable;
}

bool SystemNotifier::present(const QString &key, const QString &title, const QString &body)
{
    if (!available() || key.isEmpty()) {
        return false;
    }

    auto message
        = QDBusMessage::createMethodCall(kService, kPath, kService, QStringLiteral("Notify"));
    QVariantMap hints {
        // Names the installed desktop entry so the daemon can draw the
        // browser's own icon and group what Omaweb sends.
        {QStringLiteral("desktop-entry"), QStringLiteral("omaweb")},
    };
    message << QStringLiteral("Omaweb")
            // A key already on screen is replaced rather than stacked: the page
            // is asking about the same thing it asked about before.
            << idByKey.value(key, 0) << QStringLiteral("omaweb") << title
            << body
            // A notification with no action is one the daemon never reports back
            // on, so the default action is what makes an answer possible at all.
            << QStringList {QStringLiteral("default"), QStringLiteral("Open")}
            << hints
            // Ask for no expiry, because a page is waiting on the reader rather
            // than on a clock. A daemon is free to overrule it and several do,
            // which is why expiry is an answer this reports rather than a state
            // it assumes cannot happen: the page hears `dismissed` either way.
            << 0;

    const QDBusReply<uint> reply
        = QDBusConnection::sessionBus().call(message, QDBus::Block, kCallTimeoutMs);
    if (!reply.isValid() || reply.value() == 0) {
        return false;
    }

    // A replaced notification keeps its key and takes the identifier the daemon
    // answered with, which is the same one for a replacement but need not be.
    if (const auto previous = idByKey.value(key, 0); previous != 0) {
        keyById.remove(previous);
    }
    keyById.insert(reply.value(), key);
    idByKey.insert(key, reply.value());
    return true;
}

void SystemNotifier::withdraw(const QString &key)
{
    const auto id = idByKey.value(key, 0);
    if (id == 0) {
        return;
    }
    // Taken before the call, so the `NotificationClosed` it causes finds no key
    // and reports nothing. Nobody is waiting for this answer any more.
    takeKey(id);

    auto message = QDBusMessage::createMethodCall(
        kService, kPath, kService, QStringLiteral("CloseNotification"));
    message << id;
    QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
}

} // namespace omaweb

#include "LinuxSystemNotifier.moc"
