#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <chrono>

namespace omaweb {

// A Problem recording (ADR 0068): what happened in the window's tabs while the
// reader reproduced a problem, one line to an event, with the time since it
// started. The reader starts it, and it ends when they end it, when a report
// takes it, when the browser quits, or at its limit. It is kept in memory and
// nowhere else.
//
// While it runs it also keeps Omaweb's own warnings, from a message handler
// installed for that time alone and handing every message on to the one before
// it.
class ProblemRecording final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    // When a recording running now ends on its own.
    Q_PROPERTY(QDateTime endsAt READ endsAt NOTIFY recordingChanged)

public:
    explicit ProblemRecording(
        QObject *parent = nullptr, std::chrono::milliseconds limit = std::chrono::minutes(10));
    ~ProblemRecording() override;

    bool recording() const;
    QDateTime endsAt() const;
    std::chrono::milliseconds limit() const;

    // Starts a new recording, dropping what an earlier one kept.
    Q_INVOKABLE void start();
    // Ends it, keeping what it recorded for the next report.
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    // One event in the tab `tabId`: what happened, at which address, and what
    // more there is to say. Nothing while no recording runs.
    Q_INVOKABLE void record(
        const QString &kind, const QString &tabId, const QUrl &address, const QString &detail);
    // What was recorded, for a report, which ends the recording and leaves it
    // empty.
    Q_INVOKABLE QStringList take();

signals:
    void recordingChanged();

private:
    // `source` is the tab, by its number in this recording, or the browser.
    void keep(const QString &source, const QString &event);
    static void handleMessage(
        QtMsgType type, const QMessageLogContext &context, const QString &message);

    std::chrono::milliseconds m_limit;
    QTimer m_end;
    QElapsedTimer m_clock;
    QDateTime m_endsAt;
    QHash<QString, int> m_tabNumbers;
    QStringList m_lines;
    QtMessageHandler m_previousHandler = nullptr;
    bool m_recording = false;
};

} // namespace omaweb
