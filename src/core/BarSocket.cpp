#include "BarSocket.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

#include <optional>

#include <sys/stat.h>

namespace omaweb {
namespace {

    // An action is a name and a tab. Anything longer is not one.
    constexpr qint64 maximumActionBytes = 4 * 1024;

    // A state is tens of kilobytes. A widget that lets this much wait unread
    // is not reading, and what it has not read is the browser's memory.
    constexpr qint64 maximumUnreadBytes = 4 * 1024 * 1024;

    // Long enough for a browser busy with a page to answer, short enough that
    // a socket nobody answers on is taken over at start without a wait.
    constexpr int probeTimeoutMs = 500;

    constexpr int defaultAbsenceGraceMs = 3000;

    bool isText(const QJsonValue &value) { return value.isString() && !value.toString().isEmpty(); }

    // The arguments `action` takes, read from `request`, or nothing where the
    // request is not that action with those arguments.
    std::optional<QVariantMap> argumentsOf(const QString &action, const QJsonObject &request)
    {
        static const QStringList bare {QStringLiteral("back"), QStringLiteral("forward"),
            QStringLiteral("reload"), QStringLiteral("omnibar")};
        static const QStringList onATab {QStringLiteral("activate"), QStringLiteral("close"),
            QStringLiteral("mute"), QStringLiteral("unmute")};
        const auto tab = request.value(QStringLiteral("tab"));
        const auto space = request.value(QStringLiteral("space"));
        if (bare.contains(action)) {
            return QVariantMap {};
        }
        if (onATab.contains(action)) {
            if (!isText(tab)) {
                return std::nullopt;
            }
            QVariantMap arguments {{QStringLiteral("tab"), tab.toString()}};
            if (action == u"activate" && isText(space)) {
                arguments.insert(QStringLiteral("space"), space.toString());
            }
            return arguments;
        }
        if (action == u"space" && isText(space)) {
            return QVariantMap {{QStringLiteral("space"), space.toString()}};
        }
        return std::nullopt;
    }

} // namespace

BarSocket::BarSocket(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
{
    connect(m_server, &QLocalServer::newConnection, this, &BarSocket::accept);
    m_absence.setSingleShot(true);
    m_absence.setInterval(defaultAbsenceGraceMs);
    connect(&m_absence, &QTimer::timeout, this, [this] {
        if (m_widgets.isEmpty()) {
            setWidgetsPresent(false);
        }
    });
}

BarSocket::~BarSocket()
{
    for (auto *widget : std::as_const(m_widgets)) {
        widget->disconnect(this);
    }
}

bool BarSocket::listen(const QString &path)
{
    if (m_server->isListening()) {
        return true;
    }
    const auto directory = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directory)) {
        qWarning("The bar socket's directory %s could not be made", qPrintable(directory));
        return false;
    }
    // Only this user may enter Omaweb's own directory. One `OMAWEB_BAR_SOCKET`
    // names may not be Omaweb's to change, and the socket's own mode holds
    // there, as the Agent socket's does.
    if (QFileInfo(directory).fileName() == u"omaweb"
        && ::chmod(QFile::encodeName(directory).constData(), S_IRWXU) != 0) {
        qWarning("The bar socket's directory %s could not be made private", qPrintable(directory));
        return false;
    }
    // Only a socket is ever taken away, and only one nobody answers on.
    struct stat status {};
    if (::lstat(QFile::encodeName(path).constData(), &status) == 0) {
        if (!S_ISSOCK(status.st_mode)) {
            qWarning(
                "%s is not a socket, so this Omaweb opens no bar socket over it", qPrintable(path));
            return false;
        }
        QLocalSocket probe;
        probe.connectToServer(path);
        if (probe.waitForConnected(probeTimeoutMs)) {
            qWarning("Another Omaweb is answering on %s, so this one opens no bar socket",
                qPrintable(path));
            return false;
        }
        QLocalServer::removeServer(path);
    }
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(path)) {
        qWarning("The bar socket %s could not be opened: %s", qPrintable(path),
            qPrintable(m_server->errorString()));
        return false;
    }
    if (::chmod(QFile::encodeName(path).constData(), S_IRUSR | S_IWUSR) != 0) {
        qWarning("The bar socket %s keeps the mode Qt gave it, 0700", qPrintable(path));
    }
    // The widgets connect once the shell sees the socket. Until the grace has
    // passed without one, the bar is taken to be on its way.
    setWidgetsPresent(true);
    m_absence.start();
    return true;
}

void BarSocket::close()
{
    const auto widgets = m_widgets;
    m_widgets.clear();
    for (auto *widget : widgets) {
        widget->disconnect(this);
        widget->abort();
        widget->deleteLater();
    }
    // Closing the server takes its socket file with it.
    m_server->close();
    m_absence.stop();
    setWidgetsPresent(false);
}

bool BarSocket::listening() const { return m_server->isListening(); }

void BarSocket::publish(const QJsonObject &state)
{
    m_line = QJsonDocument(state).toJson(QJsonDocument::Compact) + '\n';
    const auto widgets = m_widgets;
    for (auto *widget : widgets) {
        send(widget);
    }
}

bool BarSocket::widgetsPresent() const { return m_widgetsPresent; }

void BarSocket::setAbsenceGrace(int milliseconds) { m_absence.setInterval(milliseconds); }

void BarSocket::accept()
{
    while (auto *widget = m_server->nextPendingConnection()) {
        m_widgets.append(widget);
        connect(widget, &QLocalSocket::readyRead, this, [this, widget] { read(widget); });
        connect(widget, &QLocalSocket::disconnected, this, [this, widget] {
            m_widgets.removeAll(widget);
            widget->deleteLater();
            if (m_widgets.isEmpty()) {
                m_absence.start();
            }
        });
        m_absence.stop();
        setWidgetsPresent(true);
        if (!m_line.isEmpty()) {
            send(widget);
        }
    }
}

void BarSocket::read(QLocalSocket *widget)
{
    while (widget->canReadLine()) {
        const auto line = widget->readLine(maximumActionBytes + 1);
        if (!line.endsWith('\n')) {
            widget->disconnectFromServer();
            return;
        }
        const auto request = QJsonDocument::fromJson(line).object();
        const auto action = request.value(QStringLiteral("do")).toString();
        const auto arguments = argumentsOf(action, request);
        if (!arguments) {
            qWarning("The bar asked for an action Omaweb does not take: %s",
                line.trimmed().left(200).constData());
            continue;
        }
        emit actionRequested(action, *arguments);
    }
    if (widget->bytesAvailable() > maximumActionBytes) {
        widget->disconnectFromServer();
    }
}

void BarSocket::send(QLocalSocket *widget)
{
    widget->write(m_line);
    if (widget->bytesToWrite() > maximumUnreadBytes) {
        widget->abort();
    }
}

void BarSocket::setWidgetsPresent(bool present)
{
    if (m_widgetsPresent == present) {
        return;
    }
    m_widgetsPresent = present;
    emit widgetsPresentChanged();
}

QString barSocketPath()
{
    const auto override = qEnvironmentVariable("OMAWEB_BAR_SOCKET");
    if (!override.isEmpty()) {
        return override;
    }
#if defined(Q_OS_MACOS)
    const auto base = QDir::tempPath();
#else
    const auto base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#endif
    return QDir(base).filePath(QStringLiteral("omaweb/bar.sock"));
}

} // namespace omaweb
