#include "ProbeClock.h"

#include "PerformanceProbe.h"

#include <QEventLoop>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <rhi/qrhi.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>

namespace omaweb::test {

namespace {

    template <typename Value> double mean(const std::vector<Value> &values)
    {
        return values.empty() ? 0.0
                              : std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    }

    // By nearest rank: the smallest value at least `fraction` of them do not
    // exceed. `sorted` is in ascending order.
    template <typename Value> double percentile(const std::vector<Value> &sorted, double fraction)
    {
        return sorted.empty()
            ? 0.0
            : sorted[static_cast<std::size_t>(std::ceil(fraction * sorted.size())) - 1];
    }

} // namespace

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
    m_frameEnds.clear();
    m_gpuMilliseconds.clear();
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
            if (m_frameStart <= 0) {
                return;
            }
            const auto end = m_clock.nsecsElapsed();
            m_frameNanoseconds.push_back(end - m_frameStart);
            m_frameEnds.push_back(end);
            // What the GPU spent on a frame, which the CPU bracket above never
            // sees: it records commands and moves on. Read off the swapchain's
            // command buffer, which is the one the frames are submitted on,
            // and zero unless `QSG_RHI_PROFILE=1` turned the timestamps on.
            // It is the last frame the GPU finished rather than the one just
            // ended, a frame or so behind the CPU number beside it, which a
            // mean over a second does not notice. The software rasteriser has
            // no swapchain.
            auto *renderer = m_watched->rendererInterface();
            auto *swapChain = static_cast<QRhiSwapChain *>(
                renderer->getResource(m_watched, QSGRendererInterface::RhiSwapchainResource));
            if (swapChain != nullptr) {
                m_gpuMilliseconds.push_back(
                    swapChain->currentFrameCommandBuffer()->lastCompletedGpuTime() * 1e3);
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
    const auto max = frames.empty() ? 0.0 : *std::max_element(frames.begin(), frames.end()) / 1e6;
    std::vector<qint64> intervals;
    std::adjacent_difference(m_frameEnds.begin(), m_frameEnds.end(), std::back_inserter(intervals));
    if (!intervals.empty()) {
        intervals.erase(intervals.begin());
    }
    std::ranges::sort(intervals);
    return {
        {QStringLiteral("frames"), static_cast<int>(frames.size())},
        {QStringLiteral("meanFrameMilliseconds"), mean(frames) / 1e6},
        {QStringLiteral("maxFrameMilliseconds"), max},
        {QStringLiteral("meanGpuMilliseconds"), mean(m_gpuMilliseconds)},
        {QStringLiteral("intervals"), static_cast<int>(intervals.size())},
        {QStringLiteral("maxIntervalMilliseconds"),
            intervals.empty() ? 0.0 : intervals.back() / 1e6},
        {QStringLiteral("p95IntervalMilliseconds"), percentile(intervals, 0.95) / 1e6},
    };
}

} // namespace omaweb::test
