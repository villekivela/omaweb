#include "FileChooser.h"

#include "PortalWindow.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUuid>

namespace omaweb {
namespace {

    const auto kService = QStringLiteral("org.freedesktop.portal.Desktop");
    const auto kPath = QStringLiteral("/org/freedesktop/portal/desktop");
    const auto kInterface = QStringLiteral("org.freedesktop.portal.FileChooser");
    const auto kRequestInterface = QStringLiteral("org.freedesktop.portal.Request");

    // Asking returns as soon as the portal has the request; the reader's
    // answer arrives afterwards, as `Response` on it.
    constexpr int kCallTimeoutMs = 5000;

    // A filter as the portal takes one, `(sa(us))`: a name and its patterns,
    // each a glob (0) or a MIME type (1).
    struct PortalPattern {
        uint kind = 0;
        QString value;
    };
    struct PortalFilter {
        QString name;
        QList<PortalPattern> patterns;
    };

    QDBusArgument &operator<<(QDBusArgument &argument, const PortalPattern &pattern)
    {
        argument.beginStructure();
        argument << pattern.kind << pattern.value;
        argument.endStructure();
        return argument;
    }
    const QDBusArgument &operator>>(const QDBusArgument &argument, PortalPattern &pattern)
    {
        argument.beginStructure();
        argument >> pattern.kind >> pattern.value;
        argument.endStructure();
        return argument;
    }
    QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilter &filter)
    {
        argument.beginStructure();
        argument << filter.name << filter.patterns;
        argument.endStructure();
        return argument;
    }
    const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilter &filter)
    {
        argument.beginStructure();
        argument >> filter.name >> filter.patterns;
        argument.endStructure();
        return argument;
    }

} // namespace
} // namespace omaweb

Q_DECLARE_METATYPE(omaweb::PortalPattern)
Q_DECLARE_METATYPE(omaweb::PortalFilter)

namespace omaweb {
namespace {

    void registerPortalTypes()
    {
        static const bool registered = [] {
            qDBusRegisterMetaType<PortalPattern>();
            qDBusRegisterMetaType<QList<PortalPattern>>();
            qDBusRegisterMetaType<PortalFilter>();
            qDBusRegisterMetaType<QList<PortalFilter>>();
            return true;
        }();
        Q_UNUSED(registered);
    }

    // "Text files (*.txt *.text)" is a filter named "Text files" of two globs.
    // A string with no parenthesised patterns is a single glob list of its own.
    PortalFilter fromNameFilter(const QString &nameFilter)
    {
        static const QRegularExpression shape(QStringLiteral(R"(^(.*)\(([^()]*)\)\s*$)"));
        const auto match = shape.match(nameFilter);
        const auto name = match.hasMatch() ? match.captured(1).trimmed() : nameFilter.trimmed();
        const auto globs = match.hasMatch() ? match.captured(2) : nameFilter;
        PortalFilter filter {name.isEmpty() ? nameFilter : name, {}};
        for (const auto &glob : globs.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            filter.patterns.append({0, glob});
        }
        return filter;
    }

    // `.pdf` as `*.[pP][dD][fF]`. A portal's globs are matched as written, and
    // Chrome's accept filters match a suffix in either case.
    QString caseBlindGlob(const QString &suffix)
    {
        QString glob = QStringLiteral("*");
        for (const auto character : suffix) {
            const auto lower = character.toLower();
            const auto upper = character.toUpper();
            if (lower == upper) {
                glob.append(character);
            } else {
                glob.append(QLatin1Char('[')).append(lower).append(upper).append(QLatin1Char(']'));
            }
        }
        return glob;
    }

    // A page's `accept` list holds MIME types, wildcards such as `image/*`
    // among them, and suffixes such as `.pdf`.
    PortalFilter fromAcceptedTypes(const QString &name, const QStringList &types)
    {
        PortalFilter filter {name, {}};
        for (const auto &type : types) {
            const auto trimmed = type.trimmed();
            if (trimmed.startsWith(QLatin1Char('.'))) {
                filter.patterns.append({0, caseBlindGlob(trimmed)});
            } else if (trimmed.contains(QLatin1Char('/'))) {
                filter.patterns.append({1, trimmed});
            }
        }
        return filter;
    }

    // The path the portal will give the request, known before asking so the
    // answer cannot arrive before anyone is listening for it.
    QString requestPath(const QDBusConnection &bus, const QString &token)
    {
        auto sender = bus.baseService().mid(1);
        sender.replace(QLatin1Char('.'), QLatin1Char('_'));
        return QStringLiteral("%1/request/%2/%3").arg(kPath, sender, token);
    }

    // The portal takes a folder as a NUL-terminated byte string.
    QByteArray portalFolder(const QString &path)
    {
        auto bytes = QFile::encodeName(path);
        bytes.append('\0');
        return bytes;
    }

} // namespace

bool FileChooser::ask(const QVariantMap &question, QWindow *window)
{
    close();
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }
    registerPortalTypes();

    const auto mode = question.value(QStringLiteral("mode")).toString();
    const auto saving = mode == QStringLiteral("save");
    const auto token
        = QStringLiteral("omaweb_%1").arg(QUuid::createUuid().toString(QUuid::Id128).left(12));
    QVariantMap options {{QStringLiteral("handle_token"), token}, {QStringLiteral("modal"), true}};
    if (!saving) {
        options.insert(QStringLiteral("multiple"), mode == QStringLiteral("open-multiple"));
        options.insert(QStringLiteral("directory"), mode == QStringLiteral("folder"));
    }

    QList<PortalFilter> filters;
    const auto accepted
        = fromAcceptedTypes(question.value(QStringLiteral("acceptedTypesName")).toString(),
            question.value(QStringLiteral("acceptedTypes")).toStringList());
    if (!accepted.patterns.isEmpty()) {
        filters.append(accepted);
    }
    for (const auto &nameFilter : question.value(QStringLiteral("nameFilters")).toStringList()) {
        const auto filter = fromNameFilter(nameFilter);
        if (!filter.patterns.isEmpty()) {
            filters.append(filter);
        }
    }
    if (!filters.isEmpty()) {
        options.insert(QStringLiteral("filters"), QVariant::fromValue(filters));
        options.insert(QStringLiteral("current_filter"), QVariant::fromValue(filters.constFirst()));
    }

    const auto currentFile = question.value(QStringLiteral("currentFile")).toUrl();
    if (saving && currentFile.isLocalFile()) {
        const QFileInfo file(currentFile.toLocalFile());
        options.insert(QStringLiteral("current_name"), file.fileName());
        options.insert(QStringLiteral("current_folder"), portalFolder(file.absolutePath()));
    }

    // Listening first: a portal may answer before the call that asked returns.
    auto handle = requestPath(bus, token);
    hear(handle, true);

    auto message = QDBusMessage::createMethodCall(kService, kPath, kInterface,
        saving ? QStringLiteral("SaveFile") : QStringLiteral("OpenFile"));
    message << portalWindowHandle(window) << question.value(QStringLiteral("title")).toString()
            << options;
    const auto reply = bus.call(message, QDBus::Block, kCallTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage
        && reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.NoReply")) {
        // A portal still starting may show its dialog after the wait, so this
        // one counts as open, and the answer comes on the path listened to.
        m_handle = handle;
        emit openChanged();
        return true;
    }
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        // No portal, or none with a file chooser behind it: nothing was shown.
        hear(handle, false);
        return false;
    }
    // A portal older than the predictable path names its own request.
    const auto given = qvariant_cast<QDBusObjectPath>(reply.arguments().constFirst()).path();
    if (!given.isEmpty() && given != handle) {
        hear(handle, false);
        handle = given;
        hear(handle, true);
    }
    m_handle = handle;
    emit openChanged();
    return true;
}

void FileChooser::close()
{
    if (m_handle.isEmpty()) {
        return;
    }
    hear(m_handle, false);
    // Closing the request takes the dialog off the screen, and the portal
    // sends no answer for it.
    QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall(kService, m_handle,
                                           kRequestInterface, QStringLiteral("Close")),
        QDBus::NoBlock);
    forget();
}

void FileChooser::hear(const QString &handle, bool listening)
{
    auto bus = QDBusConnection::sessionBus();
    if (listening) {
        bus.connect(kService, handle, kRequestInterface, QStringLiteral("Response"), this,
            SLOT(answered(uint, QVariantMap)));
    } else {
        bus.disconnect(kService, handle, kRequestInterface, QStringLiteral("Response"), this,
            SLOT(answered(uint, QVariantMap)));
    }
}

void FileChooser::answered(uint response, const QVariantMap &results)
{
    if (m_handle.isEmpty()) {
        return;
    }
    hear(m_handle, false);
    forget();
    // 0 is a choice, 1 the reader cancelling, and 2 the dialog ending some
    // other way, which leaves the reader with nothing chosen either.
    QList<QUrl> urls;
    if (response == 0) {
        for (const auto &uri : results.value(QStringLiteral("uris")).toStringList()) {
            urls.append(QUrl(uri));
        }
    }
    if (urls.isEmpty()) {
        emit cancelled();
        return;
    }
    emit chosen(urls);
}

} // namespace omaweb
