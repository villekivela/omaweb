#include "ProblemRecording.h"

#include "ProblemReport.h"

#include <QMetaObject>
#include <QMutex>
#include <QPointer>
#include <QThread>
#include <QTime>

#include <utility>

namespace omaweb {

namespace {

    // A recording ten minutes long is a few hundred events. A page that logs
    // an error every frame is not, and loses its oldest.
    constexpr qsizetype maximumLines = 2000;
    constexpr qsizetype maximumDetailLength = 300;

    // The recording the message handler keeps warnings for. One at a time, as
    // there is one handler.
    QMutex handlerLock;
    QPointer<ProblemRecording> handlerRecording;

} // namespace

ProblemRecording::ProblemRecording(QObject *parent, std::chrono::milliseconds limit)
    : QObject(parent)
    , m_limit(limit)
{
    m_end.setSingleShot(true);
    connect(&m_end, &QTimer::timeout, this, &ProblemRecording::stop);
}

ProblemRecording::~ProblemRecording() { stop(); }

bool ProblemRecording::recording() const { return m_recording; }

bool ProblemRecording::pending() const { return m_recording || !m_lines.isEmpty(); }

QDateTime ProblemRecording::endsAt() const { return m_endsAt; }

std::chrono::milliseconds ProblemRecording::limit() const { return m_limit; }

void ProblemRecording::start()
{
    stop();
    m_lines.clear();
    m_tabNumbers.clear();
    m_clock.start();
    m_endsAt = QDateTime::currentDateTimeUtc().addDuration(m_limit);
    m_end.start(m_limit);
    {
        const QMutexLocker locker(&handlerLock);
        handlerRecording = this;
        m_previousHandler = qInstallMessageHandler(&ProblemRecording::handleMessage);
    }
    m_recording = true;
    emit recordingChanged();
}

void ProblemRecording::stop()
{
    if (!m_recording) {
        return;
    }
    m_end.stop();
    {
        const QMutexLocker locker(&handlerLock);
        qInstallMessageHandler(m_previousHandler);
        m_previousHandler = nullptr;
        handlerRecording = nullptr;
    }
    m_recording = false;
    m_endsAt = {};
    emit recordingChanged();
}

void ProblemRecording::toggle()
{
    if (m_recording) {
        stop();
    } else {
        start();
    }
}

void ProblemRecording::record(
    const QString &kind, const QString &tabId, const QUrl &address, const QString &detail)
{
    if (!m_recording) {
        return;
    }
    auto event = kind;
    if (!address.isEmpty()) {
        event += u' ' + redactedAddress(address);
    }
    if (!detail.isEmpty()) {
        event += QStringLiteral(": ") + redactedText(detail.simplified()).left(maximumDetailLength);
    }
    auto number = m_tabNumbers.value(tabId);
    if (number == 0) {
        number = static_cast<int>(m_tabNumbers.size()) + 1;
        m_tabNumbers.insert(tabId, number);
    }
    keep(QStringLiteral("tab %1").arg(number), event);
}

QStringList ProblemRecording::take()
{
    stop();
    return std::exchange(m_lines, {});
}

void ProblemRecording::keep(const QString &source, const QString &event)
{
    const auto elapsed = QTime::fromMSecsSinceStartOfDay(static_cast<int>(m_clock.elapsed()));
    m_lines.append(QStringLiteral("+%1 %2 %3")
            .arg(elapsed.toString(QStringLiteral("mm:ss.zzz")), source, event));
    if (m_lines.size() > maximumLines) {
        m_lines.removeFirst();
    }
}

// Called on whichever thread logged, so a warning from another one reaches
// the recording through its event loop.
void ProblemRecording::handleMessage(
    QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QtMessageHandler previous = nullptr;
    QPointer<ProblemRecording> recording;
    {
        const QMutexLocker locker(&handlerLock);
        recording = handlerRecording;
        previous = recording ? recording->m_previousHandler : nullptr;
    }
    if (recording && type != QtDebugMsg && type != QtInfoMsg) {
        const auto event
            = QStringLiteral("Omaweb warning: ") + redactedText(message).left(maximumDetailLength);
        if (QThread::currentThread() == recording->thread()) {
            recording->keep(QStringLiteral("browser"), event);
        } else {
            QMetaObject::invokeMethod(recording, [recording, event] {
                if (recording && recording->m_recording) {
                    recording->keep(QStringLiteral("browser"), event);
                }
            });
        }
    }
    if (previous) {
        previous(type, context, message);
    }
}

} // namespace omaweb
