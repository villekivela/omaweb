#include "PagePrinter.h"

#include "PortalWindow.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QUuid>
#include <QVariantMap>

namespace omaweb {
namespace {

    const auto kService = QStringLiteral("org.freedesktop.portal.Desktop");
    const auto kPath = QStringLiteral("/org/freedesktop/portal/desktop");
    const auto kInterface = QStringLiteral("org.freedesktop.portal.Print");

    // Handing the document over is quick; what takes time is the reader
    // answering, and that happens after this call has returned.
    constexpr int kCallTimeoutMs = 5000;

    bool portalHasPrinting()
    {
        auto bus = QDBusConnection::sessionBus();
        auto *interface = bus.interface();
        if (!interface) {
            return false;
        }
        if (!interface->isServiceRegistered(kService)
            && !interface->activatableServiceNames().value().contains(kService)) {
            return false;
        }
        // The desktop portal is there, which does not mean this desktop has a
        // print portal behind it. Reading the interface's own version is what
        // says whether anything would answer.
        auto query = QDBusMessage::createMethodCall(kService, kPath,
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
        query << kInterface << QStringLiteral("version");
        const auto reply = bus.call(query, QDBus::Block, kCallTimeoutMs);
        return reply.type() == QDBusMessage::ReplyMessage;
    }

} // namespace

// Printing is a desktop service reached through the portal, which is also the
// only way to it from inside a sandbox. Where no portal answers, there is no
// dialog to present and the command says so rather than blocking on one nobody
// can see.
bool PagePrinter::available() const
{
    static const bool reachable = portalHasPrinting();
    return reachable;
}

// The portal's own print dialog is the desktop's, and its "Print to File"
// destination is the PDF one the reader expects. The page has already been
// rendered to PDF by the engine adapter, so what is printed is what was on
// screen.
bool PagePrinter::present(const QString &path, const QString &jobName, QWindow *window)
{
    if (!available() || path.isEmpty()) {
        discard(path);
        return false;
    }

    QFile document(path);
    if (!document.open(QIODevice::ReadOnly)) {
        discard(path);
        return false;
    }
    // The descriptor is duplicated into the message, and the portal reads the
    // document through it rather than by name. That is what lets the spooled
    // copy be taken away below while the reader is still answering: the file
    // has no name any more, and the open descriptor keeps it alive until the
    // portal is done.
    const QDBusUnixFileDescriptor descriptor(document.handle());
    document.close();

    auto message
        = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("Print"));
    QVariantMap options {
        {QStringLiteral("handle_token"),
            QStringLiteral("omaweb_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8))},
    };
    // The window the reader asked from, named the way the portal names one, so
    // its dialog stands over that window. A desktop that gives no name gets an
    // empty one, which is a dialog of its own placing rather than no dialog.
    //
    // No print token, which is what makes the portal show a dialog at all: a
    // token from `PreparePrint` would mean the settings were already answered
    // and this should print straight away.
    message << portalWindowHandle(window)
            << (jobName.isEmpty() ? QStringLiteral("Omaweb") : jobName)
            << QVariant::fromValue(descriptor) << options;

    const auto reply = QDBusConnection::sessionBus().call(message, QDBus::Block, kCallTimeoutMs);
    const auto accepted = reply.type() == QDBusMessage::ReplyMessage;

    // The rendered copy has served its purpose either way; a document the
    // reader wanted to keep is a download, not a leftover in a spool.
    discard(path);

    // What the portal answers here is whether it took the document, and a
    // dialog follows when it did. Whether the reader goes on to print or
    // cancel arrives later, on the request this call names, and neither is a
    // failure: a reader who cancels has printed nothing on purpose.
    return accepted;
}

} // namespace omaweb
