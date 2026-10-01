#include "SystemMotion.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QLocalSocket>

#include <functional>
#include <memory>

namespace omaweb {

namespace {

    const auto kPortalService = QStringLiteral("org.freedesktop.portal.Desktop");
    const auto kPortalPath = QStringLiteral("/org/freedesktop/portal/desktop");
    const auto kSettingsInterface = QStringLiteral("org.freedesktop.portal.Settings");
    const auto kAppearance = QStringLiteral("org.freedesktop.appearance");
    const auto kReducedMotion = QStringLiteral("reduced-motion");
    const auto kGnomeInterface = QStringLiteral("org.gnome.desktop.interface");
    const auto kEnableAnimations = QStringLiteral("enable-animations");

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
                  setAsks(Source::Portal, portalAsksToReduceMotion(value));
              } else if (nameSpace == kGnomeInterface && key == kEnableAnimations) {
                  setAsks(Source::Gnome, gnomeAsksToReduceMotion(value));
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
        connect(socket, &QLocalSocket::connected, socket,
            [socket] { socket->write("j/getoption animations:enabled"); });
        // Hyprland answers and closes, so the whole answer is in hand once
        // the socket is.
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            setAsks(Source::Hyprland, hyprlandAsksToReduceMotion(socket->readAll()));
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
    eventSocket->connectToServer(events);
}

SystemMotion::~SystemMotion() = default;

} // namespace omaweb

#include "LinuxSystemMotion.moc"
