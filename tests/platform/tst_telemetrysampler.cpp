#include "TelemetrySampler.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace omaweb;

class TelemetrySamplerTest final : public QObject {
    Q_OBJECT

private slots:
    void readsEachCoresLoadBetweenTwoSamples();
    void readsTheNetworkThroughTheMachinesOwnInterfaces();
    void readsMemoryAndSwap();
    void readsTheMachinesBatteryNotADevices();
    void aMachineWithoutABatteryHasNone();
    void countsRefusalsForTheSpacesAskedFor();
    void keepsFiveMinutes();
    void samplesOnlyWhileRunning();
    void countsTodaysPagesByHour();

private:
    static void write(const QTemporaryDir &root, const QString &path, const QByteArray &text);
};

void TelemetrySamplerTest::write(
    const QTemporaryDir &root, const QString &path, const QByteArray &text)
{
    const auto full = root.filePath(path);
    QVERIFY(QDir().mkpath(QFileInfo(full).path()));
    QFile file(full);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(text);
}

// /proc/stat counts each core's time in jiffies since boot: user, nice,
// system, idle, iowait, irq, softirq and steal. A core's load is the share of
// the time between two samples it spent on anything but idle and iowait.
void TelemetrySamplerTest::readsEachCoresLoadBetweenTwoSamples()
{
    QTemporaryDir root;
    write(root, QStringLiteral("proc/stat"),
        "cpu  100 0 100 800 0 0 0 0 0 0\n"
        "cpu0 50 0 50 400 0 0 0 0 0 0\n"
        "cpu1 50 0 50 400 0 0 0 0 0 0\n"
        "intr 12345\n");
    TelemetrySampler telemetry(root.path());
    telemetry.sample(0, true);
    // Nothing to compare the first sample with yet.
    QCOMPARE(telemetry.readings({}).value(QStringLiteral("cpu")).toList().size(), 0);

    // cpu0 spent 50 of 100 jiffies working, cpu1 25 of 100, of which 25
    // waiting for the disk, which is idle too.
    write(root, QStringLiteral("proc/stat"),
        "cpu  175 0 100 900 25 0 0 0 0 0\n"
        "cpu0 100 0 50 450 0 0 0 0 0 0\n"
        "cpu1 75 0 50 450 25 0 0 0 0 0\n");
    telemetry.sample(250, false);
    auto readings = telemetry.readings({});
    QCOMPARE(readings.value(QStringLiteral("cores")).toList(), QVariantList({0.5, 0.25}));
    QCOMPARE(readings.value(QStringLiteral("cpuNow")).toDouble(), 0.375);

    // The chart has a point a second, the mean of the second's samples, and
    // the gauge's peak is the highest sample of the five minutes.
    write(root, QStringLiteral("proc/stat"),
        "cpu  300 0 100 975 25 0 0 0 0 0\n"
        "cpu0 175 0 50 475 0 0 0 0 0 0\n"
        "cpu1 125 0 50 500 25 0 0 0 0 0\n");
    telemetry.sample(1000, true);
    readings = telemetry.readings({});
    QCOMPARE(readings.value(QStringLiteral("cpuNow")).toDouble(), 0.625);
    QCOMPARE(readings.value(QStringLiteral("cpu")).toList(), QVariantList {0.5});
    QCOMPARE(readings.value(QStringLiteral("cpuPeak")).toDouble(), 0.625);
}

// The bytes each interface has received and sent since boot, in
// /proc/net/dev. Only the machine's own interfaces count: loopback is the
// machine talking to itself, and a virtual one (a bridge, a VPN's tunnel)
// carries traffic a physical one carries too. /sys/class/net names each
// interface by where its device lives.
void TelemetrySamplerTest::readsTheNetworkThroughTheMachinesOwnInterfaces()
{
    QTemporaryDir root;
    const auto device = [&root](const QString &name, const QString &under) {
        QVERIFY(QDir().mkpath(root.filePath(under + name)));
        QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("sys/class/net"))));
        QVERIFY(QFile::link(
            root.filePath(under + name), root.filePath(QStringLiteral("sys/class/net/") + name)));
    };
    device(QStringLiteral("wlan0"), QStringLiteral("sys/devices/pci0000:00/net/"));
    device(QStringLiteral("lo"), QStringLiteral("sys/devices/virtual/net/"));
    device(QStringLiteral("docker0"), QStringLiteral("sys/devices/virtual/net/"));
    const auto counters = [&root](qint64 received, qint64 sent) {
        write(root, QStringLiteral("proc/net/dev"),
            QStringLiteral(
                "Inter-|   Receive                                                |  Transmit\n"
                " face |bytes    packets errs drop fifo frame compressed multicast|bytes    "
                "packets errs drop fifo colls carrier compressed\n"
                "    lo: 9000000 100 0 0 0 0 0 0 9000000 100 0 0 0 0 0 0\n"
                "  wlan0: %1 200 0 0 0 0 0 0 %2 150 0 0 0 0 0 0\n"
                "docker0: %3 10 0 0 0 0 0 0 %4 10 0 0 0 0 0 0\n")
                .arg(received)
                .arg(sent)
                // A bridge carries a container's traffic, which the
                // physical interface carries too.
                .arg(received / 2)
                .arg(sent / 2)
                .toUtf8());
    };
    counters(1000000, 20000);
    TelemetrySampler telemetry(root.path());
    telemetry.sample(0, true);
    counters(1500000, 25000);
    telemetry.sample(250, false);
    // 500000 bytes in a quarter of a second is two million a second.
    auto readings = telemetry.readings({});
    QCOMPARE(readings.value(QStringLiteral("downNow")).toDouble(), 2000000.0);
    QCOMPARE(readings.value(QStringLiteral("upNow")).toDouble(), 20000.0);
    counters(3000000, 40000);
    telemetry.sample(1000, true);
    readings = telemetry.readings({});
    QCOMPARE(readings.value(QStringLiteral("down")).toList(), QVariantList {2000000.0});
    QCOMPARE(readings.value(QStringLiteral("up")).toList(), QVariantList {20000.0});
    QCOMPARE(readings.value(QStringLiteral("downPeak")).toDouble(), 2000000.0);

    // A counter that went back, as an interface taken down and up again
    // starts from zero, is no traffic rather than a negative rate.
    counters(100, 100);
    telemetry.sample(1250, false);
    readings = telemetry.readings({});
    QCOMPARE(readings.value(QStringLiteral("downNow")).toDouble(), 0.0);
}

// Memory as /proc/meminfo has it, in KiB: the cache is what the kernel
// lends to files and gives back when asked, page cache, buffers and the
// slab it can reclaim. Omaweb's own is this process's resident set.
void TelemetrySamplerTest::readsMemoryAndSwap()
{
    QTemporaryDir root;
    write(root, QStringLiteral("proc/meminfo"),
        "MemTotal:       16000000 kB\n"
        "MemFree:         4000000 kB\n"
        "MemAvailable:    9000000 kB\n"
        "Buffers:          200000 kB\n"
        "Cached:          4500000 kB\n"
        "SwapCached:            0 kB\n"
        "SwapTotal:       4000000 kB\n"
        "SwapFree:        3000000 kB\n"
        "SReclaimable:     300000 kB\n");
    write(root, QStringLiteral("proc/self/status"),
        "Name:\tomaweb-browser\n"
        "VmRSS:\t  430080 kB\n"
        "Threads:\t42\n");
    TelemetrySampler telemetry(root.path());
    telemetry.sample(0, true);
    const auto readings = telemetry.readings({});
    QCOMPARE(readings.value(QStringLiteral("memory")).toMap(),
        QVariantMap({
            {QStringLiteral("total"), 16000000},
            {QStringLiteral("available"), 9000000},
            {QStringLiteral("free"), 4000000},
            {QStringLiteral("cache"), 5000000},
            {QStringLiteral("swapTotal"), 4000000},
            {QStringLiteral("swapFree"), 3000000},
        }));
    QCOMPARE(readings.value(QStringLiteral("omaweb")).toLongLong(), 430080);
}

// The machine's own battery, as Omarchy's bar reads it from
// /sys/class/power_supply: not a mouse's or a trackpad's, whose scope is
// Device, and not the charger's. Its draw is power_now in microwatts, or
// current_now times voltage_now where the firmware gives those instead.
void TelemetrySamplerTest::readsTheMachinesBatteryNotADevices()
{
    QTemporaryDir root;
    const QString supply = QStringLiteral("sys/class/power_supply/");
    write(root, supply + QStringLiteral("AC/type"), "Mains\n");
    // A mouse's battery, listed before the machine's.
    write(root, supply + QStringLiteral("hid-04:b5:battery/type"), "Battery\n");
    write(root, supply + QStringLiteral("hid-04:b5:battery/scope"), "Device\n");
    write(root, supply + QStringLiteral("hid-04:b5:battery/capacity"), "15\n");
    write(root, supply + QStringLiteral("hid-04:b5:battery/power_now"), "100000\n");
    write(root, supply + QStringLiteral("macsmc-battery/type"), "Battery\n");
    write(root, supply + QStringLiteral("macsmc-battery/capacity"), "64\n");
    write(root, supply + QStringLiteral("macsmc-battery/status"), "Discharging\n");
    write(root, supply + QStringLiteral("macsmc-battery/current_now"), "800000\n");
    write(root, supply + QStringLiteral("macsmc-battery/voltage_now"), "12000000\n");
    TelemetrySampler telemetry(root.path());
    telemetry.sample(0, true);
    auto battery = telemetry.readings({}).value(QStringLiteral("battery")).toMap();
    QCOMPARE(battery.value(QStringLiteral("capacity")).toDouble(), 0.64);
    QCOMPARE(battery.value(QStringLiteral("status")).toString(), QStringLiteral("Discharging"));
    QCOMPARE(battery.value(QStringLiteral("watts")).toList(), QVariantList {9.6});
    QCOMPARE(battery.value(QStringLiteral("charge")).toList(), QVariantList {0.64});

    write(root, supply + QStringLiteral("macsmc-battery/power_now"), "7500000\n");
    write(root, supply + QStringLiteral("macsmc-battery/capacity"), "63\n");
    telemetry.sample(1000, true);
    battery = telemetry.readings({}).value(QStringLiteral("battery")).toMap();
    QCOMPARE(battery.value(QStringLiteral("watts")).toList(), QVariantList({9.6, 7.5}));
    QCOMPARE(battery.value(QStringLiteral("charge")).toList(), QVariantList({0.64, 0.63}));
}

void TelemetrySamplerTest::aMachineWithoutABatteryHasNone()
{
    QTemporaryDir root;
    write(root, QStringLiteral("sys/class/power_supply/AC/type"), "Mains\n");
    TelemetrySampler telemetry(root.path());
    telemetry.sample(0, true);
    const auto readings = telemetry.readings({});
    QVERIFY(readings.contains(QStringLiteral("battery")));
    QVERIFY(readings.value(QStringLiteral("battery")).isNull());
}

// Refusals are counted a second at a time for each Space, so a window asks
// for its own: a Private window's Space is not an ordinary window's. The
// total runs from when sampling started; the most refused hosts are the
// last five minutes'.
void TelemetrySamplerTest::countsRefusalsForTheSpacesAskedFor()
{
    QTemporaryDir root;
    TelemetrySampler telemetry(root.path());
    telemetry.setRunning(true);
    telemetry.countRefusals(QStringLiteral("work"),
        {QStringLiteral("https://ads.example/a.js"), QStringLiteral("https://ads.example/b.js"),
            QStringLiteral("https://pixel.example:8443/p?x=1")});
    telemetry.countRefusals(QStringLiteral("private"), {QStringLiteral("https://ads.example/c")});
    telemetry.sample(1000, true);
    telemetry.countRefusals(QStringLiteral("work"), {QStringLiteral("https://pixel.example/q")});
    telemetry.sample(2000, true);

    auto blocked
        = telemetry.readings({QStringLiteral("work")}).value(QStringLiteral("blocked")).toMap();
    QCOMPARE(blocked.value(QStringLiteral("lastMinute")).toLongLong(), 4);
    QCOMPARE(blocked.value(QStringLiteral("total")).toLongLong(), 4);
    QCOMPARE(blocked.value(QStringLiteral("hosts")).toList(),
        QVariantList({
            QVariantMap({{QStringLiteral("host"), QStringLiteral("ads.example")},
                {QStringLiteral("count"), 2}}),
            QVariantMap({{QStringLiteral("host"), QStringLiteral("pixel.example")},
                {QStringLiteral("count"), 2}}),
        }));
    blocked
        = telemetry.readings({QStringLiteral("private")}).value(QStringLiteral("blocked")).toMap();
    QCOMPARE(blocked.value(QStringLiteral("lastMinute")).toLongLong(), 1);
    QCOMPARE(blocked.value(QStringLiteral("total")).toLongLong(), 1);

    // A minute on, they have left the minute's count; five minutes on, the
    // hosts too; and they are still in the total.
    for (int second = 3; second <= 61; ++second) {
        telemetry.sample(second * 1000, true);
    }
    blocked = telemetry.readings({QStringLiteral("work")}).value(QStringLiteral("blocked")).toMap();
    QCOMPARE(blocked.value(QStringLiteral("lastMinute")).toLongLong(), 1);
    QCOMPARE(blocked.value(QStringLiteral("hosts")).toList().size(), 2);
    telemetry.sample(62 * 1000, true);
    blocked = telemetry.readings({QStringLiteral("work")}).value(QStringLiteral("blocked")).toMap();
    QCOMPARE(blocked.value(QStringLiteral("lastMinute")).toLongLong(), 0);
    for (int second = 63; second <= 2 + TelemetrySampler::slowKept; ++second) {
        telemetry.sample(second * 1000, true);
    }
    blocked = telemetry.readings({QStringLiteral("work")}).value(QStringLiteral("blocked")).toMap();
    QCOMPARE(blocked.value(QStringLiteral("hosts")).toList(), QVariantList());
    QCOMPARE(blocked.value(QStringLiteral("total")).toLongLong(), 4);
}

// Four samples a second of CPU and network, and one a second of the rest,
// each kept for five minutes.
void TelemetrySamplerTest::keepsFiveMinutes()
{
    QTemporaryDir root;
    write(root, QStringLiteral("proc/stat"), "cpu  1 0 0 1 0 0 0 0\n");
    write(root, QStringLiteral("proc/net/dev"), "wlan0: 0 0 0 0 0 0 0 0 0 0\n");
    write(root, QStringLiteral("sys/class/power_supply/BAT0/type"), "Battery\n");
    write(root, QStringLiteral("sys/class/power_supply/BAT0/power_now"), "1000000\n");
    TelemetrySampler telemetry(root.path());
    for (int tick = 0; tick <= TelemetrySampler::fastKept + 40; ++tick) {
        telemetry.sample(tick * TelemetrySampler::fastInterval, tick % 4 == 0);
    }
    const auto readings = telemetry.readings({});
    QCOMPARE(TelemetrySampler::fastKept, 1200);
    QCOMPARE(TelemetrySampler::slowKept, 300);
    QCOMPARE(readings.value(QStringLiteral("cpu")).toList().size(), TelemetrySampler::slowKept);
    QCOMPARE(readings.value(QStringLiteral("down")).toList().size(), TelemetrySampler::slowKept);
    QCOMPARE(readings.value(QStringLiteral("battery"))
                 .toMap()
                 .value(QStringLiteral("watts"))
                 .toList()
                 .size(),
        TelemetrySampler::slowKept);
}

// Sampling costs nothing until Telemetry is chosen, and what it kept goes
// when it no longer is: nothing outlives the reader's choice.
void TelemetrySamplerTest::samplesOnlyWhileRunning()
{
    QTemporaryDir root;
    write(root, QStringLiteral("proc/stat"), "cpu  1 0 0 1 0 0 0 0\n");
    TelemetrySampler telemetry(root.path());
    QSignalSpy sampled(&telemetry, &TelemetrySampler::sampled);
    QTest::qWait(3 * TelemetrySampler::fastInterval);
    QCOMPARE(sampled.count(), 0);
    telemetry.countRefusals(QStringLiteral("work"), {QStringLiteral("https://ads.example/")});

    telemetry.setRunning(true);
    // The first sample is taken at once, so a tab opened now has readings.
    QCOMPARE(sampled.count(), 1);
    // Past the first second, so the charts have a point.
    QTRY_VERIFY(sampled.count() >= 6);
    QVERIFY(!telemetry.readings({}).value(QStringLiteral("cpu")).toList().isEmpty());
    // A refusal from before is not counted.
    QCOMPARE(telemetry.readings({QStringLiteral("work")})
                 .value(QStringLiteral("blocked"))
                 .toMap()
                 .value(QStringLiteral("total"))
                 .toLongLong(),
        0);

    telemetry.setRunning(false);
    const auto stopped = sampled.count();
    QTest::qWait(3 * TelemetrySampler::fastInterval);
    QCOMPARE(sampled.count(), stopped);
    QVERIFY(telemetry.readings({}).value(QStringLiteral("cpu")).toList().isEmpty());
}

// Today's pages, an hour a bar: read from History once a day, and counted
// as pages load after that, so a new tab reads no History.
void TelemetrySamplerTest::countsTodaysPagesByHour()
{
    QTemporaryDir root;
    TelemetrySampler telemetry(root.path());
    QDateTime now(QDate(2026, 10, 27), QTime(10, 30));
    telemetry.setWallClockForTests([&now] { return now; });
    const auto at = [](int day, int hour, int minute) {
        return QDateTime(QDate(2026, 10, day), QTime(hour, minute)).toMSecsSinceEpoch();
    };
    QList<qint64> asked;
    telemetry.setVisitTimes([&](qint64 since) {
        asked.append(since);
        return since == at(27, 0, 0)
            ? QList<qint64> {at(27, 8, 5), at(27, 8, 40), at(27, 9, 10), at(27, 10, 1)}
            : QList<qint64> {};
    });
    const auto pages = [&telemetry] {
        QList<int> hours;
        for (const auto &count : telemetry.readings({}).value(QStringLiteral("pages")).toList()) {
            hours.append(count.toInt());
        }
        return hours;
    };
    // A new tab's read asks nothing of History: it is read on a sample.
    QCOMPARE(pages(), QList<int>(24, 0));
    QVERIFY(asked.isEmpty());
    telemetry.sample(0, true);
    auto expected = QList<int>(24, 0);
    expected[8] = 2;
    expected[9] = 1;
    expected[10] = 1;
    QCOMPARE(pages(), expected);
    QCOMPARE(asked, QList<qint64>({at(27, 0, 0)}));
    // A Private window's read leaves History out.
    QVERIFY(!telemetry.readings({}, false).contains(QStringLiteral("pages")));

    telemetry.countVisit(at(27, 10, 20));
    telemetry.countVisit(at(26, 23, 50));
    expected[10] = 2;
    QCOMPARE(pages(), expected);
    // Counted, not read again.
    QCOMPARE(asked.size(), 1);

    // History deleted: today's visits are read again, on the next sample.
    telemetry.forgetVisits();
    pages();
    QCOMPARE(asked.size(), 1);
    telemetry.sample(1000, true);
    expected[10] = 1;
    QCOMPARE(pages(), expected);
    QCOMPARE(asked.size(), 2);

    // A new day starts empty, before and after its first sample.
    now = QDateTime(QDate(2026, 10, 28), QTime(0, 10));
    QCOMPARE(pages(), QList<int>(24, 0));
    telemetry.sample(2000, true);
    QCOMPARE(pages(), QList<int>(24, 0));
    QCOMPARE(asked.last(), at(28, 0, 0));
}

QTEST_GUILESS_MAIN(TelemetrySamplerTest)
#include "tst_telemetrysampler.moc"
