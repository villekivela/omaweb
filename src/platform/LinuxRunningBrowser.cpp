#include "RunningBrowser.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QVariantMap>

namespace omaweb {
namespace {

    // The application's own name, which is the macOS bundle identifier as well,
    // so Omaweb is one identity on both platforms.
    //
    // The desktop entry is deliberately not named after it. Qt takes the desktop
    // file name as the Wayland app id and the X11 window class, and a desktop's
    // window rules are written against that, so the entry stays `omaweb` and the
    // freedesktop convention of naming it after the bus name is given up. That
    // costs `DBusActivatable`, which needs the two to match; the launcher runs
    // the command instead, and the process handles the handover itself.
    const auto kBusName = QStringLiteral("dev.omaweb.browser");
    const auto kObjectPath = QStringLiteral("/dev/omaweb/browser");
    const auto kInterface = QStringLiteral("org.freedesktop.Application");

    // A local call to a process that is already running. Long enough to survive
    // a browser busy with a page, short enough that a wedged one does not leave
    // the reader looking at nothing.
    constexpr int kCallTimeoutMs = 5000;

    // The freedesktop application interface rather than one of Omaweb's own.
    // Nothing here needs a private protocol, and answering the standard one
    // means the desktop can reach the browser the way it reaches any other
    // application.
    class ApplicationAdaptor final : public QDBusAbstractAdaptor {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Application")

    public:
        explicit ApplicationAdaptor(RunningBrowser *browser)
            : QDBusAbstractAdaptor(browser)
            , m_browser(browser)
        {
        }

    public slots:
        void Activate(const QVariantMap &platformData)
        {
            Q_UNUSED(platformData)
            emit m_browser->activationRequested();
        }

        void Open(const QStringList &uris, const QVariantMap &platformData)
        {
            Q_UNUSED(platformData)
            for (const auto &uri : uris) {
                // Whatever arrives here came from outside the browser, so it is
                // read exactly as strictly as a command line is.
                const QUrl url(uri, QUrl::StrictMode);
                if (url.isValid() && !url.isRelative()) {
                    emit m_browser->openRequested(url);
                }
            }
        }

        void ActivateAction(
            const QString &name, const QVariantList &parameter, const QVariantMap &platformData)
        {
            Q_UNUSED(name)
            Q_UNUSED(parameter)
            Q_UNUSED(platformData)
            // Omaweb's desktop entry declares no actions, so anything named here
            // is a caller guessing. Coming forward is the harmless reading.
            emit m_browser->activationRequested();
        }

    private:
        RunningBrowser *m_browser;
    };

} // namespace

RunningBrowser::RunningBrowser(QObject *parent)
    : QObject(parent)
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        // No session bus is a desktop that cannot tell one launch from another.
        // Being the browser that answers is the only useful thing to be.
        return;
    }

    // The claim is the name, and taking it is what decides this. Registering
    // the object first means a later launch never finds the name owned by a
    // process that cannot yet answer on it.
    new ApplicationAdaptor(this);
    if (!bus.registerObject(kObjectPath, this)) {
        return;
    }
    m_primary = bus.registerService(kBusName);
}

RunningBrowser::~RunningBrowser()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    if (m_primary) {
        bus.unregisterService(kBusName);
    }
    bus.unregisterObject(kObjectPath);
}

bool RunningBrowser::isPrimary() const { return m_primary; }

bool RunningBrowser::handOver(const QUrl &url)
{
    if (m_primary) {
        return false;
    }
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }

    // An address to open, or nothing to open and a browser to come forward.
    const auto method = url.isValid() ? QStringLiteral("Open") : QStringLiteral("Activate");
    auto message = QDBusMessage::createMethodCall(kBusName, kObjectPath, kInterface, method);
    if (url.isValid()) {
        message << QStringList {url.toString()};
    }
    message << QVariantMap {};

    const auto reply = bus.call(message, QDBus::Block, kCallTimeoutMs);
    return reply.type() == QDBusMessage::ReplyMessage;
}

} // namespace omaweb

#include "LinuxRunningBrowser.moc"
