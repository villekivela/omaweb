#include "ProbeClock.h"

#include "PerformanceProbe.h"

#include <QEventLoop>
#include <QQuickWindow>
#include <QTimer>

#include <algorithm>
#include <numeric>

namespace omaweb::test {

ProbeClock::ProbeClock(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
}

double ProbeClock::milliseconds() const { return m_clock.nsecsElapsed() / 1e6; }

QString ProbeClock::report(
    const QString &name, double measured, const QString &unit, double threshold) const
{
    return omaweb::probe::report(name, measured, unit, threshold);
}

bool ProbeClock::waitForFrame(QQuickWindow *window, int timeout)
{
    if (window == nullptr) {
        return false;
    }
    QEventLoop loop;
    bool swapped = false;
    const auto connection = connect(window, &QQuickWindow::frameSwapped, &loop, [&] {
        swapped = true;
        loop.quit();
    });
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    loop.exec();
    disconnect(connection);
    return swapped;
}

void ProbeClock::watchFrames(QQuickWindow *window)
{
    if (m_watched != nullptr) {
        m_watched->disconnect(this);
    }
    m_watched = window;
    m_frameNanoseconds.clear();
    m_frameStart = 0;
    if (window == nullptr) {
        return;
    }
    // Direct, so the timestamps are taken on the thread drawing the frame
    // rather than after a queued hop back to the GUI thread.
    connect(
        window, &QQuickWindow::beforeFrameBegin, this,
        [this] { m_frameStart = m_clock.nsecsElapsed(); }, Qt::DirectConnection);
    connect(
        window, &QQuickWindow::afterFrameEnd, this,
        [this] {
            if (m_frameStart > 0) {
                m_frameNanoseconds.push_back(m_clock.nsecsElapsed() - m_frameStart);
            }
        },
        Qt::DirectConnection);
}

QVariantMap ProbeClock::frameReport()
{
    if (m_watched != nullptr) {
        m_watched->disconnect(this);
        m_watched = nullptr;
    }
    const auto &frames = m_frameNanoseconds;
    const auto mean = frames.empty()
        ? 0.0
        : std::accumulate(frames.begin(), frames.end(), 0.0) / frames.size() / 1e6;
    const auto max = frames.empty() ? 0.0 : *std::max_element(frames.begin(), frames.end()) / 1e6;
    return {
        {QStringLiteral("frames"), static_cast<int>(frames.size())},
        {QStringLiteral("meanFrameMilliseconds"), mean},
        {QStringLiteral("maxFrameMilliseconds"), max},
    };
}

} // namespace omaweb::test
