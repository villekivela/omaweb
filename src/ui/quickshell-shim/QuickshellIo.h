#pragma once

// The `Quickshell.Io` half of the shim: see Quickshell.h for why this exists.

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringDecoder>
#include <QStringList>
#include <QVariantMap>

// `stdout` is a macro from <cstdio>, and the QML property the vendored kit
// assigns to is named exactly that (`Process { stdout: StdioCollector {} }`).
// A Qt property name is its C++ name, so the macro has to go.
#ifdef stdout
#undef stdout
#endif

class QFileSystemWatcher;

namespace omaweb::quickshell {

// What a `Process`'s `stdout` property takes: a collector or a line splitter.
class StdioSink : public QObject {
    Q_OBJECT

public:
    explicit StdioSink(QObject *parent = nullptr);

    virtual void reset() = 0;
    virtual void append(const QByteArray &data) = 0;
    virtual void finish() = 0;
};

// Collects a process's standard output and reports it once the stream ends.
class StdioCollector : public StdioSink {
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(bool waitForEnd READ waitForEnd WRITE setWaitForEnd NOTIFY waitForEndChanged)

public:
    explicit StdioCollector(QObject *parent = nullptr);

    QString text() const;
    bool waitForEnd() const;
    void setWaitForEnd(bool waitForEnd);

    void reset() override;
    void append(const QByteArray &data) override;
    void finish() override;

signals:
    void textChanged();
    void waitForEndChanged();
    void streamFinished();

private:
    QString m_text;
    bool m_waitForEnd = false;
};

// Hands a process's standard output over one line at a time, as it arrives. A last line with no
// newline after it is handed over when the stream ends.
class SplitParser : public StdioSink {
    Q_OBJECT
    Q_PROPERTY(QString splitMarker READ splitMarker WRITE setSplitMarker NOTIFY splitMarkerChanged)

public:
    explicit SplitParser(QObject *parent = nullptr);

    QString splitMarker() const;
    void setSplitMarker(const QString &splitMarker);

    void reset() override;
    void append(const QByteArray &data) override;
    void finish() override;

signals:
    void splitMarkerChanged();
    void read(const QString &line);

private:
    QString m_splitMarker = QStringLiteral("\n");
    QString m_pending;
    QStringDecoder m_decoder {QStringDecoder::Utf8};
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
    Q_PROPERTY(
        QVariantMap environment READ environment WRITE setEnvironment NOTIFY environmentChanged)
    Q_PROPERTY(bool stdinEnabled READ stdinEnabled WRITE setStdinEnabled NOTIFY stdinEnabledChanged)
    Q_PROPERTY(
        StdioSink *stdout READ standardOutput WRITE setStandardOutput NOTIFY standardOutputChanged)

public:
    explicit Process(QObject *parent = nullptr);
    ~Process() override;

    QStringList command() const;
    void setCommand(const QStringList &command);
    bool isRunning() const;
    void setRunning(bool running);
    QVariantMap environment() const;
    void setEnvironment(const QVariantMap &environment);
    bool stdinEnabled() const;
    void setStdinEnabled(bool stdinEnabled);
    StdioSink *standardOutput() const;
    void setStandardOutput(StdioSink *sink);

    Q_INVOKABLE void write(const QString &data);

signals:
    void commandChanged();
    void runningChanged();
    void environmentChanged();
    void stdinEnabledChanged();
    void standardOutputChanged();
    void exited(int exitCode, int exitStatus);

private:
    void start();
    void finish();

    QStringList m_command;
    QVariantMap m_environment;
    bool m_stdinEnabled = false;
    StdioSink *m_standardOutput = nullptr;
    QProcess m_process;
    bool m_running = false;
};

// `FileView { path; watchChanges; text(); reload() }`. Absent files are
// expected — every consumer in the kit handles `onLoadFailed`.
class FileView : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(bool blockLoading READ blockLoading WRITE setBlockLoading NOTIFY blockLoadingChanged)
    Q_PROPERTY(bool watchChanges READ watchChanges WRITE setWatchChanges NOTIFY watchChangesChanged)
    Q_PROPERTY(bool printErrors READ printErrors WRITE setPrintErrors NOTIFY printErrorsChanged)

public:
    explicit FileView(QObject *parent = nullptr);

    QString path() const;
    void setPath(const QString &path);
    bool blockLoading() const;
    void setBlockLoading(bool blockLoading);
    bool watchChanges() const;
    void setWatchChanges(bool watchChanges);
    bool printErrors() const;
    void setPrintErrors(bool printErrors);

    Q_INVOKABLE QString text() const;
    Q_INVOKABLE void reload();

signals:
    void pathChanged();
    void blockLoadingChanged();
    void watchChangesChanged();
    void printErrorsChanged();
    void loaded();
    void loadFailed();
    void fileChanged();

private:
    void refreshWatch();

    QString m_path;
    QString m_text;
    bool m_blockLoading = false;
    bool m_watchChanges = false;
    bool m_printErrors = true;
    QFileSystemWatcher *m_watcher = nullptr;
};

// `IpcHandler { enabled; target }`. Omaweb answers no `qs ipc` call, so the handler only holds the
// two properties the kit sets. `qs.Commons` declares an `IpcRegistry` singleton that holds one, and
// the engine resolves every type in a module's directory when any of them loads, so a missing
// `IpcHandler` fails every `qs.Commons` token, not only a panel.
class IpcHandler : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY targetChanged)

public:
    explicit IpcHandler(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);
    QString target() const;
    void setTarget(const QString &target);

signals:
    void enabledChanged();
    void targetChanged();

private:
    bool m_enabled = true;
    QString m_target;
};

} // namespace omaweb::quickshell
