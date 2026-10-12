#include "QuickshellIo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QProcessEnvironment>
#include <QTextStream>

#include <utility>

namespace omaweb::quickshell {

// Every file the vendored kit reads, and whether it got it. The kit asks for
// four -- a theme's colours and shell tokens, a machine-level override, a
// Hyprland toggle -- with `printErrors` off on the ones that matter most, and
// installs empty values when a read fails. A palette that came out wrong is
// then indistinguishable from one that never arrived, so the shim says.
Q_LOGGING_CATEGORY(fileViewLog, "omaweb.quickshell.fileview")

StdioSink::StdioSink(QObject *parent)
    : QObject(parent)
{
}

StdioCollector::StdioCollector(QObject *parent)
    : StdioSink(parent)
{
}

QString StdioCollector::text() const { return m_text; }

bool StdioCollector::waitForEnd() const { return m_waitForEnd; }

void StdioCollector::setWaitForEnd(bool waitForEnd)
{
    if (m_waitForEnd == waitForEnd) {
        return;
    }
    m_waitForEnd = waitForEnd;
    emit waitForEndChanged();
}

void StdioCollector::reset()
{
    if (m_text.isEmpty()) {
        return;
    }
    m_text.clear();
    emit textChanged();
}

void StdioCollector::append(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }
    m_text.append(QString::fromUtf8(data));
    emit textChanged();
}

void StdioCollector::finish() { emit streamFinished(); }

SplitParser::SplitParser(QObject *parent)
    : StdioSink(parent)
{
}

QString SplitParser::splitMarker() const { return m_splitMarker; }

void SplitParser::setSplitMarker(const QString &splitMarker)
{
    if (m_splitMarker == splitMarker) {
        return;
    }
    m_splitMarker = splitMarker;
    emit splitMarkerChanged();
}

void SplitParser::reset()
{
    m_pending.clear();
    m_decoder = QStringDecoder(QStringDecoder::Utf8);
}

void SplitParser::append(const QByteArray &data)
{
    if (data.isEmpty() || m_splitMarker.isEmpty()) {
        return;
    }
    // A multi-byte character can arrive in two reads, so the decoder keeps its tail.
    m_pending.append(m_decoder(data));
    for (auto end = m_pending.indexOf(m_splitMarker); end >= 0;
        end = m_pending.indexOf(m_splitMarker)) {
        const auto line = m_pending.left(end);
        m_pending.remove(0, end + m_splitMarker.size());
        emit read(line);
    }
}

void SplitParser::finish()
{
    if (m_pending.isEmpty()) {
        return;
    }
    const auto line = std::exchange(m_pending, QString());
    emit read(line);
}

Process::Process(QObject *parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        if (m_standardOutput) {
            m_standardOutput->append(m_process.readAllStandardOutput());
        }
    });
    connect(
        &m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
            finish();
            emit exited(exitCode, status);
        });
    // A command the host does not have is a normal outcome here. Report the
    // stream as finished anyway so the consumer's fallback path runs.
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finish();
            emit exited(-1, QProcess::CrashExit);
        }
    });
}

// The kit asks `hyprctl` and `fc-match` for values it can live without, so a
// still-running child at teardown is expected rather than a leak to report.
Process::~Process()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(100);
    }
}

QStringList Process::command() const { return m_command; }

void Process::setCommand(const QStringList &command)
{
    if (m_command == command) {
        return;
    }
    m_command = command;
    emit commandChanged();
}

bool Process::isRunning() const { return m_running; }

void Process::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }
    if (running) {
        start();
        return;
    }
    m_process.kill();
    m_running = false;
    emit runningChanged();
}

QVariantMap Process::environment() const { return m_environment; }

void Process::setEnvironment(const QVariantMap &environment)
{
    if (m_environment == environment) {
        return;
    }
    m_environment = environment;
    emit environmentChanged();
}

bool Process::stdinEnabled() const { return m_stdinEnabled; }

void Process::setStdinEnabled(bool stdinEnabled)
{
    if (m_stdinEnabled == stdinEnabled) {
        return;
    }
    m_stdinEnabled = stdinEnabled;
    emit stdinEnabledChanged();
}

StdioSink *Process::standardOutput() const { return m_standardOutput; }

void Process::setStandardOutput(StdioSink *sink)
{
    if (m_standardOutput == sink) {
        return;
    }
    m_standardOutput = sink;
    emit standardOutputChanged();
}

void Process::write(const QString &data)
{
    if (m_stdinEnabled && m_process.state() == QProcess::Running) {
        m_process.write(data.toUtf8());
    }
}

void Process::start()
{
    if (m_command.isEmpty() || m_process.state() != QProcess::NotRunning) {
        return;
    }
    if (m_standardOutput) {
        m_standardOutput->reset();
    }
    m_running = true;
    emit runningChanged();
    // The kit's variables go on top of the browser's own, as Quickshell's do.
    auto environment = QProcessEnvironment::systemEnvironment();
    for (auto it = m_environment.cbegin(); it != m_environment.cend(); ++it) {
        if (it.value().isNull()) {
            environment.remove(it.key());
        } else {
            environment.insert(it.key(), it.value().toString());
        }
    }
    m_process.setProcessEnvironment(environment);
    m_process.start(m_command.first(), m_command.mid(1));
}

void Process::finish()
{
    const auto wasRunning = m_running;
    m_running = false;
    if (m_standardOutput) {
        m_standardOutput->append(m_process.readAllStandardOutput());
        m_standardOutput->finish();
    }
    if (wasRunning) {
        emit runningChanged();
    }
}

FileView::FileView(QObject *parent)
    : QObject(parent)
{
}

QString FileView::path() const { return m_path; }

void FileView::setPath(const QString &path)
{
    if (m_path == path) {
        return;
    }
    m_path = path;
    emit pathChanged();
    refreshWatch();
    // A file that asked to block has its text the moment the path is set. Any other loads after
    // the component finishes binding, so an `onLoaded` handler declared alongside `path` is
    // connected before the signal fires.
    if (m_blockLoading) {
        reload();
        return;
    }
    QMetaObject::invokeMethod(this, &FileView::reload, Qt::QueuedConnection);
}

bool FileView::blockLoading() const { return m_blockLoading; }

void FileView::setBlockLoading(bool blockLoading)
{
    if (m_blockLoading == blockLoading) {
        return;
    }
    m_blockLoading = blockLoading;
    emit blockLoadingChanged();
}

bool FileView::watchChanges() const { return m_watchChanges; }

void FileView::setWatchChanges(bool watchChanges)
{
    if (m_watchChanges == watchChanges) {
        return;
    }
    m_watchChanges = watchChanges;
    emit watchChangesChanged();
    refreshWatch();
}

bool FileView::printErrors() const { return m_printErrors; }

void FileView::setPrintErrors(bool printErrors)
{
    if (m_printErrors == printErrors) {
        return;
    }
    m_printErrors = printErrors;
    emit printErrorsChanged();
}

QString FileView::text() const { return m_text; }

void FileView::reload()
{
    m_text.clear();
    QFile file(m_path);
    if (m_path.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (m_printErrors && !m_path.isEmpty()) {
            qWarning("FileView could not read %s", qPrintable(m_path));
        }
        qCDebug(fileViewLog, "could not read %s", qPrintable(m_path));
        emit loadFailed();
        return;
    }
    QTextStream stream(&file);
    m_text = stream.readAll();
    qCDebug(fileViewLog, "read %lld bytes from %s", qint64(m_text.size()), qPrintable(m_path));
    emit loaded();
}

void FileView::refreshWatch()
{
    if (!m_watchChanges || m_path.isEmpty()) {
        delete m_watcher;
        m_watcher = nullptr;
        return;
    }
    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this] {
            refreshWatch();
            emit fileChanged();
        });
        // Watching the directory too is what makes a file that does not exist
        // yet — a user's optional override — arrive live rather than at the
        // next restart.
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
            const auto watched = m_watcher->files();
            if (QFileInfo::exists(m_path) && !watched.contains(m_path)) {
                refreshWatch();
                emit fileChanged();
            }
        });
    }
    if (!m_watcher->files().isEmpty()) {
        m_watcher->removePaths(m_watcher->files());
    }
    if (!m_watcher->directories().isEmpty()) {
        m_watcher->removePaths(m_watcher->directories());
    }
    if (QFileInfo::exists(m_path)) {
        m_watcher->addPath(m_path);
    }
    const auto directory = QFileInfo(m_path).absolutePath();
    if (QDir(directory).exists()) {
        m_watcher->addPath(directory);
    }
}

IpcHandler::IpcHandler(QObject *parent)
    : QObject(parent)
{
}

bool IpcHandler::enabled() const { return m_enabled; }

void IpcHandler::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    emit enabledChanged();
}

QString IpcHandler::target() const { return m_target; }

void IpcHandler::setTarget(const QString &target)
{
    if (m_target == target) {
        return;
    }
    m_target = target;
    emit targetChanged();
}

} // namespace omaweb::quickshell
