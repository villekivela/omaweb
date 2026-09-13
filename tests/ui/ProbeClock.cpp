#include "ProbeClock.h"

#include "PerformanceProbe.h"

#include <QEventLoop>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <rhi/qrhi.h>

#include <algorithm>
#include <numeric>

namespace omaweb::test {

namespace {

    template <typename Value> double mean(const std::vector<Value> &values)
    {
        return values.empty() ? 0.0
                              : std::accumulate(values.begin(), values.end(), 0.0) / values.size();
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
            m_frameNanoseconds.push_back(m_clock.nsecsElapsed() - m_frameStart);
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
    return {
        {QStringLiteral("frames"), static_cast<int>(frames.size())},
        {QStringLiteral("meanFrameMilliseconds"), mean(frames) / 1e6},
        {QStringLiteral("maxFrameMilliseconds"), max},
        {QStringLiteral("meanGpuMilliseconds"), mean(m_gpuMilliseconds)},
    };
}

} // namespace omaweb::test
