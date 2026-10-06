#include "SystemMotion.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QFileInfo>
#include <QLocalSocket>
#include <QTimer>

#include <functional>
#include <memory>
#include <utility>

namespace omaweb {

namespace {

    const auto kPortalService = QStringLiteral("org.freedesktop.portal.Desktop");
    const auto kPortalPath = QStringLiteral("/org/freedesktop/portal/desktop");
    const auto kSettingsInterface = QStringLiteral("org.freedesktop.portal.Settings");
    const auto kAppearance = QStringLiteral("org.freedesktop.appearance");
    const auto kReducedMotion = QStringLiteral("reduced-motion");
    const auto kGnomeInterface = QStringLiteral("org.gnome.desktop.interface");
    const auto kEnableAnimations = QStringLiteral("enable-animations");
    // Long enough not to spin against a compositor that is not there, short
    // enough that a restarted one is heard from before the reader notices.
    constexpr int kReconnectMs = 5000;

    // The portal hands a setting back wrapped in a variant, and the older
    // `Read` wraps it twice.
    QVariant unwrapped(QVariant value)
    {
        while (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
            value = qvariant_cast<QDBusVariant>(value).variant();
        }
        return value;
    }

    // The portal's change signal needs a slot to arrive at; a setting that
    // changes while the browser runs is heard here.
    class PortalSettingListener final : public QObject {
        Q_OBJECT

    public:
        using Changed = std::function<void(const QString &, const QString &, const QVariant &)>;

        PortalSettingListener(Changed changed, QObject *parent)
            : QObject(parent)
            , m_changed(std::move(changed))
        {
        }

    public slots:
        void settingChanged(const QString &nameSpace, const QString &key, const QDBusVariant &value)
        {
            m_changed(nameSpace, key, unwrapped(value.variant()));
        }

    private:
        Changed m_changed;
    };

} // namespace

SystemMotion::SystemMotion(QObject *parent)
    : QObject(parent)
{
    auto bus = QDBusConnection::sessionBus();
    const auto answer
        = [this](const QString &nameSpace, const QString &key, const QVariant &value) {
              if (nameSpace == kAppearance && key == kReducedMotion) {
                  setReducedBy(Source::Portal, portalAsksToReduceMotion(value));
              } else if (nameSpace == kGnomeInterface && key == kEnableAnimations) {
                  setReducedBy(Source::Gnome, gnomeAsksToReduceMotion(value));
              }
          };

    if (bus.isConnected()) {
        auto *listener = new PortalSettingListener(answer, this);
        bus.connect(kPortalService, kPortalPath, kSettingsInterface,
            QStringLiteral("SettingChanged"), listener,
            SLOT(settingChanged(QString, QString, QDBusVariant)));
        // Asked once each at start, without waiting: a desktop with no portal
        // answers with an error, which says nothing about motion.
        for (const auto &[nameSpace, key] : {std::pair {kAppearance, kReducedMotion},
                 std::pair {kGnomeInterface, kEnableAnimations}}) {
            auto call = QDBusMessage::createMethodCall(
                kPortalService, kPortalPath, kSettingsInterface, QStringLiteral("Read"));
            call << nameSpace << key;
            auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), this);
            connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [answer, nameSpace, key](QDBusPendingCallWatcher *finished) {
                    const QDBusPendingReply<QDBusVariant> reply = *finished;
                    if (reply.isValid()) {
                        answer(nameSpace, key, unwrapped(reply.value().variant()));
                    }
                    finished->deleteLater();
                });
        }
    }

    // Hyprland is asked over its own socket rather than through `hyprctl`,
    // so no process is started for it. A session that is not Hyprland's has
    // no socket, and nothing here runs.
    const auto environment = QProcessEnvironment::systemEnvironment();
    const auto requests = hyprlandSocketPath(environment, HyprlandSocket::Requests);
    const auto events = hyprlandSocketPath(environment, HyprlandSocket::Events);
    if (requests.isEmpty()) {
        return;
    }
    const auto askHyprland = [this, requests] {
        auto *socket = new QLocalSocket(this);
        // Once it accepts, Hyprland stops the whole compositor for up to five
        // seconds waiting for the request, and a window being created waits
        // on the compositor. A write left for the event loop to send would
        // hold both until Hyprland gives up, so it is sent at once.
        connect(socket, &QLocalSocket::connected, socket, [socket] {
            socket->write("j/getoption animations:enabled");
            socket->flush();
        });
        // Hyprland answers and closes, so the whole answer is in hand once
        // the socket is.
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            setReducedBy(Source::Hyprland, hyprlandAsksToReduceMotion(socket->readAll()));
            socket->deleteLater();
        });
        connect(socket, &QLocalSocket::errorOccurred, socket, [socket](auto) {
            if (socket->state() == QLocalSocket::UnconnectedState) {
                socket->deleteLater();
            }
        });
        socket->connectToServer(requests);
    };
    askHyprland();
    // Events arrive in reads that need not end at a line, so whatever follows
    // the last whole line waits for the next read.
    auto *eventSocket = new QLocalSocket(this);
    auto pending = std::make_shared<QByteArray>();
    connect(eventSocket, &QLocalSocket::readyRead, this, [eventSocket, askHyprland, pending] {
        pending->append(eventSocket->readAll());
        const auto end = pending->lastIndexOf('\n') + 1;
        const auto whole = pending->first(end);
        pending->remove(0, end);
        if (hyprlandConfigurationReloaded(whole)) {
            askHyprland();
        }
    });
    // A socket that was not up yet, or that the compositor dropped, is tried
    // again a few seconds on, and asked again once it answers, since a reload
    // may have come and gone in between. A socket that is gone for good stops
    // being tried: a restarted Hyprland is another instance, under a
    // signature this process was never given.
    auto *retry = new QTimer(this);
    retry->setSingleShot(true);
    retry->setInterval(kReconnectMs);
    connect(retry, &QTimer::timeout, eventSocket, [eventSocket, events] {
        if (QFileInfo::exists(events)) {
            eventSocket->connectToServer(events);
        }
    });
    auto connectedBefore = std::make_shared<bool>(false);
    connect(eventSocket, &QLocalSocket::connected, this, [pending, askHyprland, connectedBefore] {
        pending->clear();
        if (std::exchange(*connectedBefore, true)) {
            askHyprland();
        }
    });
    connect(eventSocket, &QLocalSocket::disconnected, retry, qOverload<>(&QTimer::start));
    connect(eventSocket, &QLocalSocket::errorOccurred, retry, [eventSocket, retry](auto) {
        if (eventSocket->state() == QLocalSocket::UnconnectedState) {
            retry->start();
        }
    });
    eventSocket->connectToServer(events);
}

SystemMotion::~SystemMotion() = default;

} // namespace omaweb

#include "LinuxSystemMotion.moc"
