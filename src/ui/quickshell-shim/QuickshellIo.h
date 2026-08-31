#pragma once

// The `Quickshell.Io` half of the shim: see Quickshell.h for why this exists.

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

// `stdout` is a macro from <cstdio>, and the QML property the vendored kit
// assigns to is named exactly that (`Process { stdout: StdioCollector {} }`).
// A Qt property name is its C++ name, so the macro has to go.
#ifdef stdout
#undef stdout
#endif

class QFileSystemWatcher;

namespace tanto::quickshell {

// Collects a process's standard output and reports it once the stream ends.
class StdioCollector : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(bool waitForEnd READ waitForEnd WRITE setWaitForEnd NOTIFY waitForEndChanged)

public:
    explicit StdioCollector(QObject *parent = nullptr);

    QString text() const;
    bool waitForEnd() const;
    void setWaitForEnd(bool waitForEnd);

    void reset();
    void append(const QByteArray &data);
    void finish();

signals:
    void textChanged();
    void waitForEndChanged();
    void streamFinished();

private:
    QString m_text;
    bool m_waitForEnd = false;
};

// `Process { command: [...]; stdout: StdioCollector { ... } }`.
//
// A missing executable is an ordinary outcome, not an error: the kit asks
// `hyprctl` and `fc-match` for values that only exist on an Omarchy desktop,
// and every caller already falls back when the answer never arrives.
class Process : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList command READ command WRITE setCommand NOTIFY commandChanged)
    Q_PROPERTY(bool running READ isRunning WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(StdioCollector *stdout READ standardOutput WRITE setStandardOutput
        NOTIFY standardOutputChanged)

public:
    explicit Process(QObject *parent = nullptr);
    ~Process() override;

    QStringList command() const;
    void setCommand(const QStringList &command);
    bool isRunning() const;
    void setRunning(bool running);
    StdioCollector *standardOutput() const;
    void setStandardOutput(StdioCollector *collector);

signals:
    void commandChanged();
    void runningChanged();
    void standardOutputChanged();

private:
    void start();
    void finish();

    QStringList m_command;
    StdioCollector *m_standardOutput = nullptr;
    QProcess m_process;
    bool m_running = false;
};

// `FileView { path; watchChanges; text(); reload() }`. Absent files are
// expected — every consumer in the kit handles `onLoadFailed`.
class FileView : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(bool watchChanges READ watchChanges WRITE setWatchChanges NOTIFY watchChangesChanged)
    Q_PROPERTY(bool printErrors READ printErrors WRITE setPrintErrors NOTIFY printErrorsChanged)

public:
    explicit FileView(QObject *parent = nullptr);

    QString path() const;
    void setPath(const QString &path);
    bool watchChanges() const;
    void setWatchChanges(bool watchChanges);
    bool printErrors() const;
    void setPrintErrors(bool printErrors);

    Q_INVOKABLE QString text() const;
    Q_INVOKABLE void reload();

signals:
    void pathChanged();
    void watchChangesChanged();
    void printErrorsChanged();
    void loaded();
    void loadFailed();
    void fileChanged();

private:
    void refreshWatch();

    QString m_path;
    QString m_text;
    bool m_watchChanges = false;
    bool m_printErrors = true;
    QFileSystemWatcher *m_watcher = nullptr;
};

} // namespace tanto::quickshell
