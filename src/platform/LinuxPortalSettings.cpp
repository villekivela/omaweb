#include "LinuxPortalSettings.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

namespace omaweb {

namespace {

    const auto kPortalService = QStringLiteral("org.freedesktop.portal.Desktop");
    const auto kPortalPath = QStringLiteral("/org/freedesktop/portal/desktop");
    const auto kSettingsInterface = QStringLiteral("org.freedesktop.portal.Settings");

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
        PortalSettingListener(PortalSettingAnswer changed, QObject *parent)
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
        PortalSettingAnswer m_changed;
    };

} // namespace

void followPortalSettings(
    QObject *owner, const QList<PortalSetting> &settings, PortalSettingAnswer answer)
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    auto *listener = new PortalSettingListener(answer, owner);
    bus.connect(kPortalService, kPortalPath, kSettingsInterface, QStringLiteral("SettingChanged"),
        listener, SLOT(settingChanged(QString, QString, QDBusVariant)));
    // A desktop with no portal answers with an error, which says nothing.
    for (const auto &[nameSpace, key] : settings) {
        auto call = QDBusMessage::createMethodCall(
            kPortalService, kPortalPath, kSettingsInterface, QStringLiteral("Read"));
        call << nameSpace << key;
        auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), owner);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, owner,
            [answer, nameSpace, key](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<QDBusVariant> reply = *finished;
                if (reply.isValid()) {
                    answer(nameSpace, key, unwrapped(reply.value().variant()));
                }
                finished->deleteLater();
            });
    }
}

} // namespace omaweb

#include "LinuxPortalSettings.moc"
