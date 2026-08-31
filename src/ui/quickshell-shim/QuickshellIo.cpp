#include "QuickshellIo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMetaObject>
#include <QTextStream>

namespace tanto::quickshell {

StdioCollector::StdioCollector(QObject *parent)
    : QObject(parent)
{
}

QString StdioCollector::text() const
{
    return m_text;
}

bool StdioCollector::waitForEnd() const
{
    return m_waitForEnd;
}

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

void StdioCollector::finish()
{
    emit streamFinished();
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
    connect(&m_process, &QProcess::finished, this, &Process::finish);
    // A command the host does not have is a normal outcome here. Report the
    // stream as finished anyway so the consumer's fallback path runs.
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finish();
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

QStringList Process::command() const
{
    return m_command;
}

void Process::setCommand(const QStringList &command)
{
    if (m_command == command) {
        return;
    }
    m_command = command;
    emit commandChanged();
}

bool Process::isRunning() const
{
    return m_running;
}

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

StdioCollector *Process::standardOutput() const
{
    return m_standardOutput;
}

void Process::setStandardOutput(StdioCollector *collector)
{
    if (m_standardOutput == collector) {
        return;
    }
    m_standardOutput = collector;
    emit standardOutputChanged();
}

void Process::start()
{
    if (m_command.isEmpty()) {
        return;
    }
    if (m_standardOutput) {
        m_standardOutput->reset();
    }
    m_running = true;
    emit runningChanged();
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

QString FileView::path() const
{
    return m_path;
}

void FileView::setPath(const QString &path)
{
    if (m_path == path) {
        return;
    }
    m_path = path;
    emit pathChanged();
    refreshWatch();
    // Load after the component finishes binding, so an `onLoaded` handler
    // declared alongside `path` is connected before the signal fires.
    QMetaObject::invokeMethod(this, &FileView::reload, Qt::QueuedConnection);
}

bool FileView::watchChanges() const
{
    return m_watchChanges;
}

void FileView::setWatchChanges(bool watchChanges)
{
    if (m_watchChanges == watchChanges) {
        return;
    }
    m_watchChanges = watchChanges;
    emit watchChangesChanged();
    refreshWatch();
}

bool FileView::printErrors() const
{
    return m_printErrors;
}

void FileView::setPrintErrors(bool printErrors)
{
    if (m_printErrors == printErrors) {
        return;
    }
    m_printErrors = printErrors;
    emit printErrorsChanged();
}

QString FileView::text() const
{
    return m_text;
}

void FileView::reload()
{
    m_text.clear();
    QFile file(m_path);
    if (m_path.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (m_printErrors && !m_path.isEmpty()) {
            qWarning("FileView could not read %s", qPrintable(m_path));
        }
        emit loadFailed();
        return;
    }
    QTextStream stream(&file);
    m_text = stream.readAll();
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

} // namespace tanto::quickshell
