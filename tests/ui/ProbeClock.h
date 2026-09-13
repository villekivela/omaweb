#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVariantMap>

#include <vector>

class QQuickWindow;

namespace omaweb::test {

// The clock the QML probes read. QML's own `Date.now()` counts whole
// milliseconds, and a tab switch is over in a few of them, so the number would
// be mostly rounding. This one counts from the monotonic clock in nanoseconds.
//
// It is also the frame timer: asked to watch a window, it brackets every
// scene-graph frame from `beforeFrameBegin` to `afterFrameEnd` and keeps the
// cost of each, which is what a frame-time probe reads rather than the
// interval between frames, which the animation timer sets.
class ProbeClock final : public QObject {
    Q_OBJECT

public:
    explicit ProbeClock(QObject *parent = nullptr);

    Q_INVOKABLE double milliseconds() const;

    // Prints the probe line and returns the failure message, the same way the
    // C++ probes do through `omaweb::probe::report`.
    Q_INVOKABLE QString report(
        const QString &name, double measured, const QString &unit, double threshold) const;

    // Runs the event loop until `window` swaps its next frame, or `timeout`
    // milliseconds pass, and says which. A test loop that sleeps between
    // polls would add its own sleep to a latency it is measuring; the event
    // loop wakes on the frame itself.
    Q_INVOKABLE bool waitForFrame(QQuickWindow *window, int timeout);

    // Starts keeping the cost of every frame `window` draws from now on.
    Q_INVOKABLE void watchFrames(QQuickWindow *window);
    // Stops watching and reports `frames`, `meanFrameMilliseconds`,
    // `maxFrameMilliseconds` and `meanGpuMilliseconds`. The mean is the
    // number held; the slowest frame is printed beside it so a failure says
    // whether the cost was even or one hitch. The GPU mean is what the frames
    // cost the GPU itself, which the CPU bracket does not include, and is
    // zero unless a GPU drew them with timestamps on.
    Q_INVOKABLE QVariantMap frameReport();

private:
    QElapsedTimer m_clock;
    QQuickWindow *m_watched = nullptr;
    qint64 m_frameStart = 0;
    std::vector<qint64> m_frameNanoseconds;
    std::vector<double> m_gpuMilliseconds;
};

} // namespace omaweb::test
