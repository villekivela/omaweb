#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <functional>

namespace omaweb {

// What the Telemetry Scene draws: the machine's readings and the browser's own,
// sampled while the reader has Telemetry chosen, whether or not a Start page is
// on show, so its graphs are full when a tab opens (#814). The machine is read
// from local files the way Omarchy's bar reads it, under /proc and /sys, and
// nothing leaves the machine or is written to disk: the last five minutes are
// kept in memory and the rest is let go.
//
// One per process, shared by every window: a Private window asks for its own
// Spaces' refusals and leaves the rest out.
class TelemetrySampler final : public QObject {
    Q_OBJECT
    // Sampling runs only while this is on: the ordinary window turns it on
    // while Telemetry is the chosen Scene. Turned off, what was kept goes.
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    // How many samples are taken a second, and how many a chart's five
    // minutes hold.
    Q_PROPERTY(int samplesPerSecond READ samplesPerSecond CONSTANT)
    Q_PROPERTY(int fastKept READ fastKeptSamples CONSTANT)
    Q_PROPERTY(int slowKept READ slowKeptSamples CONSTANT)

public:
    // CPU and network are read four times a second and the rest once, each
    // kept for five minutes.
    static constexpr int fastInterval = 250;
    static constexpr int fastPerSecond = 1000 / fastInterval;
    static constexpr int fastKept = 5 * 60 * 1000 / fastInterval;
    static constexpr int slowKept = 5 * 60;

    // `root` is the directory /proc and /sys are read under: "/" on a machine,
    // a directory of fixtures in a test.
    explicit TelemetrySampler(QString root = QStringLiteral("/"), QObject *parent = nullptr);

    bool running() const;
    static int samplesPerSecond() { return fastPerSecond; }
    static int fastKeptSamples() { return fastKept; }
    static int slowKeptSamples() { return slowKept; }
    void setRunning(bool running);

    // One sample at `at`, milliseconds on a steady clock: CPU and network, and
    // with `slow` memory, the battery and the second's refusals too. The timer
    // calls it; a test calls it on a clock of its own.
    void sample(qint64 at, bool slow);

    // Everything the Scene draws, with refusals counted for `spaceIds` only.
    // A new tab reads it on the interface thread, so a chart's series is a
    // point a second, the mean of that second's samples, and the latest
    // sample and the five minutes' highest come worked out: `cores` (each
    // core's load now, 0 to 1), the whole processor's load as `cpu`,
    // `cpuNow` and `cpuPeak`, the network's bytes a second as `down`,
    // `downNow`, `downPeak`, `up`, `upNow` and `upPeak`, `memory` (KiB from
    // /proc/meminfo: total, available, free, cache, swapTotal, swapFree),
    // `omaweb` (this process's resident KiB), `battery` (null on a machine
    // without one, else capacity 0 to 1, status, and the `watts` and `charge`
    // series), `blocked` (`lastMinute`, `total` since sampling started, and
    // `hosts`, the three most refused over the five minutes, each `host` and
    // `count`) and, unless `history` is off as a Private window asks, `pages`
    // (pages loaded in each hour of today).
    Q_INVOKABLE QVariantMap readings(const QStringList &spaceIds, bool history = true) const;

    // Requests Content blocking refused for one Space, by address.
    void countRefusals(const QString &spaceId, const QStringList &addresses);

    // Where today's earlier visits are read from, once a day: their times, in
    // milliseconds since the epoch, from `since` on.
    void setVisitTimes(std::function<QList<qint64>(qint64 since)> visitTimes);
    // A page was loaded at `at`, milliseconds since the epoch.
    void countVisit(qint64 at);
    // History was deleted: today's visits are read again.
    void forgetVisits();

    // Five minutes of samples taken at once, as a tab opened after five
    // minutes of Telemetry finds them. Only a test asks for it.
    Q_INVOKABLE void fillForTests();

    // The wall clock the day and its hours are read from. Only a test sets it.
    void setWallClockForTests(std::function<QDateTime()> clock);

signals:
    void runningChanged();
    // A sample was taken, so readings() has something new.
    void sampled();

private:
    struct CpuTimes {
        qint64 total = 0;
        qint64 idle = 0;
    };
    struct RefusalSecond {
        int count = 0;
        QHash<QString, int> hosts;
    };

    void tick();
    QString read(const QString &path) const;
    void readCpu();
    void readNetwork(qint64 at);
    void readMemory();
    void readBattery();
    QVariant battery() const;
    QVariantMap blocked(const QStringList &spaceIds) const;
    void closeSecond();
    void readPages();
    QVariantList pages() const;
    QDateTime wallClock() const;
    void clear();

    QString m_root;
    bool m_running = false;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_ticks = 0;

    QList<CpuTimes> m_lastCpu;
    QList<double> m_cores;
    // Every sample of the five minutes, and a point a second for the charts
    // with the sum and count of the second under way.
    QList<double> m_cpu;
    QList<double> m_cpuSeconds;

    qint64 m_lastNetworkAt = -1;
    qint64 m_lastReceived = 0;
    qint64 m_lastSent = 0;
    QList<double> m_down;
    QList<double> m_up;
    QList<double> m_downSeconds;
    QList<double> m_upSeconds;
    double m_secondCpu = 0;
    double m_secondDown = 0;
    double m_secondUp = 0;
    int m_secondCpuSamples = 0;
    int m_secondNetworkSamples = 0;

    QVariantMap m_memory;
    qint64 m_omaweb = 0;

    QVariantMap m_battery;
    QList<double> m_watts;
    QList<double> m_charge;

    // The second being counted, by Space, and the five minutes before it,
    // oldest first.
    QHash<QString, RefusalSecond> m_refusing;
    QList<QHash<QString, RefusalSecond>> m_refused;
    QHash<QString, qint64> m_refusedTotal;

    std::function<QList<qint64>(qint64)> m_visitTimes;
    std::function<QDateTime()> m_wallClock;
    // Today's pages by hour, read on the first sample of a new day.
    QDate m_pagesDay;
    QList<int> m_pages;
};

// Makes `telemetry` available to QML as `import Omaweb`'s `TelemetrySampler`. Call
// once per process, before loading QML that uses it.
void registerTelemetrySampler(TelemetrySampler *telemetry);

} // namespace omaweb
