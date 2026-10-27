#include "TelemetrySampler.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQmlEngine>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace omaweb {

TelemetrySampler::TelemetrySampler(QString root, QObject *parent)
    : QObject(parent)
    , m_root(std::move(root))
{
    m_timer.setInterval(fastInterval);
    connect(&m_timer, &QTimer::timeout, this, [this] { tick(); });
}

bool TelemetrySampler::running() const { return m_running; }

void TelemetrySampler::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }
    m_running = running;
    if (m_running) {
        m_clock.start();
        m_ticks = 0;
        m_timer.start();
        tick();
    } else {
        m_timer.stop();
        clear();
    }
    emit runningChanged();
}

// The slow readings are taken once a second, on the first of its ticks.
void TelemetrySampler::tick()
{
    sample(m_clock.elapsed(), m_ticks % fastPerSecond == 0);
    ++m_ticks;
    emit sampled();
}

void TelemetrySampler::clear()
{
    m_lastCpu.clear();
    m_cores.clear();
    m_cpu.clear();
    m_cpuSeconds.clear();
    m_lastNetworkAt = -1;
    m_down.clear();
    m_up.clear();
    m_downSeconds.clear();
    m_upSeconds.clear();
    m_secondCpu = m_secondDown = m_secondUp = 0;
    m_secondCpuSamples = m_secondNetworkSamples = 0;
    m_memory.clear();
    m_omaweb = 0;
    m_battery.clear();
    m_watts.clear();
    m_charge.clear();
    m_refusing.clear();
    m_refused.clear();
    m_refusedTotal.clear();
    m_pagesDay = {};
    m_pages.clear();
}

namespace {

    QVariantList listOf(const QList<double> &series)
    {
        QVariantList list;
        list.reserve(series.size());
        for (const auto value : series) {
            list.append(value);
        }
        return list;
    }

    double peakOf(const QList<double> &series)
    {
        return series.isEmpty() ? 0.0 : *std::ranges::max_element(series);
    }

    // Appends `value`, keeping the last `kept`.
    void keep(QList<double> &series, double value, int kept)
    {
        series.append(value);
        if (series.size() > kept) {
            series.remove(0, series.size() - kept);
        }
    }

} // namespace

QString TelemetrySampler::read(const QString &path) const
{
    QFile file(QDir(m_root).filePath(path));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

void TelemetrySampler::sample(qint64 at, bool slow)
{
    readCpu();
    readNetwork(at);
    if (slow) {
        readMemory();
        readBattery();
        closeSecond();
        readPages();
    }
}

// The first line is the whole processor and the next one a core each. Idle
// and iowait are idle; the rest of the first eight fields are work.
void TelemetrySampler::readCpu()
{
    QList<CpuTimes> now;
    const auto lines = read(QStringLiteral("proc/stat")).split(u'\n');
    for (const auto &line : lines) {
        if (!line.startsWith(QStringLiteral("cpu"))) {
            continue;
        }
        const auto fields = line.simplified().split(u' ');
        CpuTimes times;
        for (qsizetype i = 1; i < fields.size() && i <= 8; ++i) {
            times.total += fields.at(i).toLongLong();
        }
        times.idle = fields.value(4).toLongLong() + fields.value(5).toLongLong();
        now.append(times);
    }
    if (!now.isEmpty() && m_lastCpu.size() == now.size()) {
        QList<double> loads;
        for (qsizetype i = 0; i < now.size(); ++i) {
            const auto total = now.at(i).total - m_lastCpu.at(i).total;
            const auto idle = now.at(i).idle - m_lastCpu.at(i).idle;
            loads.append(total > 0 ? 1.0 - static_cast<double>(idle) / total : 0.0);
        }
        const auto load = loads.takeFirst();
        keep(m_cpu, load, fastKept);
        m_secondCpu += load;
        ++m_secondCpuSamples;
        m_cores = loads;
    }
    m_lastCpu = now;
}

// An interface whose device lives under /sys/devices/virtual is loopback, a
// bridge or a tunnel: what it carries, a physical interface carries too.
void TelemetrySampler::readNetwork(qint64 at)
{
    qint64 received = 0;
    qint64 sent = 0;
    const auto lines = read(QStringLiteral("proc/net/dev")).split(u'\n');
    for (const auto &line : lines) {
        const auto colon = line.indexOf(u':');
        if (colon < 0) {
            continue;
        }
        const auto name = line.left(colon).trimmed();
        const QFileInfo device(QDir(m_root).filePath(QStringLiteral("sys/class/net/") + name));
        if (name == QStringLiteral("lo")
            || device.canonicalFilePath().contains(QStringLiteral("/devices/virtual/"))) {
            continue;
        }
        const auto fields = line.mid(colon + 1).simplified().split(u' ');
        received += fields.value(0).toLongLong();
        sent += fields.value(8).toLongLong();
    }
    if (m_lastNetworkAt >= 0 && at > m_lastNetworkAt) {
        const auto seconds = static_cast<double>(at - m_lastNetworkAt) / 1000.0;
        const auto down = std::max<qint64>(0, received - m_lastReceived) / seconds;
        const auto up = std::max<qint64>(0, sent - m_lastSent) / seconds;
        keep(m_down, down, fastKept);
        keep(m_up, up, fastKept);
        m_secondDown += down;
        m_secondUp += up;
        ++m_secondNetworkSamples;
    }
    m_lastNetworkAt = at;
    m_lastReceived = received;
    m_lastSent = sent;
}

namespace {

    // The fields of a /proc file of `Name: value kB` lines, by name.
    QHash<QString, qint64> fieldsOf(const QString &text)
    {
        QHash<QString, qint64> fields;
        const auto lines = text.split(u'\n');
        for (const auto &line : lines) {
            const auto colon = line.indexOf(u':');
            if (colon > 0) {
                fields.insert(line.left(colon),
                    line.mid(colon + 1).simplified().section(u' ', 0, 0).toLongLong());
            }
        }
        return fields;
    }

} // namespace

// The cache is what the kernel lends to files and takes back when a program
// asks: page cache, buffers and the slab it can reclaim.
void TelemetrySampler::readMemory()
{
    const auto info = fieldsOf(read(QStringLiteral("proc/meminfo")));
    m_memory = {
        {QStringLiteral("total"), info.value(QStringLiteral("MemTotal"))},
        {QStringLiteral("available"), info.value(QStringLiteral("MemAvailable"))},
        {QStringLiteral("free"), info.value(QStringLiteral("MemFree"))},
        {QStringLiteral("cache"),
            info.value(QStringLiteral("Cached")) + info.value(QStringLiteral("Buffers"))
                + info.value(QStringLiteral("SReclaimable"))},
        {QStringLiteral("swapTotal"), info.value(QStringLiteral("SwapTotal"))},
        {QStringLiteral("swapFree"), info.value(QStringLiteral("SwapFree"))},
    };
    m_omaweb = fieldsOf(read(QStringLiteral("proc/self/status"))).value(QStringLiteral("VmRSS"));
}

// The machine's own battery: a mouse's or a trackpad's has the scope Device.
// Its draw is power_now, in microwatts, or current_now times voltage_now
// where the firmware gives those instead.
void TelemetrySampler::readBattery()
{
    const QString supplies = QStringLiteral("sys/class/power_supply/");
    const auto names = QDir(QDir(m_root).filePath(supplies))
                           .entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto &name : names) {
        const auto field = [this, &supplies, &name](const char *file) {
            return read(supplies + name + u'/' + QLatin1StringView(file)).trimmed();
        };
        if (field("type") != QStringLiteral("Battery")
            || field("scope") == QStringLiteral("Device")) {
            continue;
        }
        const auto power = field("power_now");
        const auto watts = !power.isEmpty()
            ? power.toDouble() / 1e6
            : field("current_now").toDouble() * field("voltage_now").toDouble() / 1e12;
        const auto charge = field("capacity").toDouble() / 100.0;
        keep(m_watts, watts, slowKept);
        keep(m_charge, charge, slowKept);
        m_battery = {
            {QStringLiteral("capacity"), charge},
            {QStringLiteral("status"), field("status")},
        };
        return;
    }
    m_battery.clear();
    m_watts.clear();
    m_charge.clear();
}

QVariantMap TelemetrySampler::readings(const QStringList &spaceIds, bool history) const
{
    QVariantMap readings {
        {QStringLiteral("cores"), listOf(m_cores)},
        {QStringLiteral("cpu"), listOf(m_cpuSeconds)},
        {QStringLiteral("cpuNow"), m_cpu.isEmpty() ? 0.0 : m_cpu.last()},
        {QStringLiteral("cpuPeak"), peakOf(m_cpu)},
        {QStringLiteral("down"), listOf(m_downSeconds)},
        {QStringLiteral("downNow"), m_down.isEmpty() ? 0.0 : m_down.last()},
        {QStringLiteral("downPeak"), peakOf(m_down)},
        {QStringLiteral("up"), listOf(m_upSeconds)},
        {QStringLiteral("upNow"), m_up.isEmpty() ? 0.0 : m_up.last()},
        {QStringLiteral("upPeak"), peakOf(m_up)},
        {QStringLiteral("memory"), m_memory},
        {QStringLiteral("omaweb"), m_omaweb},
        {QStringLiteral("battery"), battery()},
        {QStringLiteral("blocked"), blocked(spaceIds)},
    };
    if (history) {
        readings.insert(QStringLiteral("pages"), pages());
    }
    return readings;
}

// Until today's pages have been read, none: yesterday's are not today's.
QVariantList TelemetrySampler::pages() const
{
    const auto today = m_pagesDay == wallClock().date();
    QVariantList hours;
    for (int hour = 0; hour < 24; ++hour) {
        hours.append(today ? m_pages.value(hour) : 0);
    }
    return hours;
}

QVariant TelemetrySampler::battery() const
{
    if (m_battery.isEmpty()) {
        return QVariant::fromValue(nullptr);
    }
    auto battery = m_battery;
    battery.insert(QStringLiteral("watts"), listOf(m_watts));
    battery.insert(QStringLiteral("charge"), listOf(m_charge));
    return battery;
}

// A refusal is counted by its host alone: the address beyond it says where
// the reader was, which the sampler has no reason to hold.
void TelemetrySampler::countRefusals(const QString &spaceId, const QStringList &addresses)
{
    if (!m_running) {
        return;
    }
    auto &second = m_refusing[spaceId];
    second.count += static_cast<int>(addresses.size());
    for (const auto &address : addresses) {
        second.hosts[QUrl(address).host()] += 1;
    }
    m_refusedTotal[spaceId] += addresses.size();
}

void TelemetrySampler::closeSecond()
{
    if (m_secondCpuSamples > 0) {
        keep(m_cpuSeconds, m_secondCpu / m_secondCpuSamples, slowKept);
    }
    if (m_secondNetworkSamples > 0) {
        keep(m_downSeconds, m_secondDown / m_secondNetworkSamples, slowKept);
        keep(m_upSeconds, m_secondUp / m_secondNetworkSamples, slowKept);
    }
    m_secondCpu = m_secondDown = m_secondUp = 0;
    m_secondCpuSamples = m_secondNetworkSamples = 0;
    m_refused.append(std::exchange(m_refusing, {}));
    if (m_refused.size() > slowKept) {
        m_refused.remove(0, m_refused.size() - slowKept);
    }
}

QVariantMap TelemetrySampler::blocked(const QStringList &spaceIds) const
{
    // The last minute's seconds are the newest sixty.
    const auto minuteFrom = m_refused.size() - 60;
    qint64 lastMinute = 0;
    QHash<QString, int> hosts;
    for (qsizetype index = 0; index < m_refused.size(); ++index) {
        const auto &second = m_refused.at(index);
        int count = 0;
        for (const auto &spaceId : spaceIds) {
            const auto refused = second.constFind(spaceId);
            if (refused == second.cend()) {
                continue;
            }
            count += refused->count;
            for (auto host = refused->hosts.cbegin(); host != refused->hosts.cend(); ++host) {
                hosts[host.key()] += host.value();
            }
        }
        if (index >= minuteFrom) {
            lastMinute += count;
        }
    }
    qint64 total = 0;
    for (const auto &spaceId : spaceIds) {
        total += m_refusedTotal.value(spaceId);
    }
    QList<std::pair<QString, int>> ranked;
    for (auto host = hosts.cbegin(); host != hosts.cend(); ++host) {
        ranked.append({host.key(), host.value()});
    }
    std::ranges::sort(ranked, [](const auto &a, const auto &b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    QVariantList top;
    for (qsizetype i = 0; i < ranked.size() && i < 3; ++i) {
        top.append(QVariantMap({{QStringLiteral("host"), ranked.at(i).first},
            {QStringLiteral("count"), ranked.at(i).second}}));
    }
    return {
        {QStringLiteral("lastMinute"), lastMinute},
        {QStringLiteral("total"), total},
        {QStringLiteral("hosts"), top},
    };
}

void TelemetrySampler::setVisitTimes(std::function<QList<qint64>(qint64)> visitTimes)
{
    m_visitTimes = std::move(visitTimes);
}

QDateTime TelemetrySampler::wallClock() const
{
    return m_wallClock ? m_wallClock() : QDateTime::currentDateTime();
}

// Read from History on the first sample of a day, or after History was
// deleted, and counted as pages load after that. A sample runs on the timer, so
// opening a tab reads no History.
void TelemetrySampler::readPages()
{
    const auto today = wallClock().date();
    if (m_pagesDay == today) {
        return;
    }
    m_pagesDay = today;
    m_pages = QList<int>(24, 0);
    if (m_visitTimes) {
        const auto since = today.startOfDay().toMSecsSinceEpoch();
        for (const auto at : m_visitTimes(since)) {
            const auto visited = QDateTime::fromMSecsSinceEpoch(at);
            if (visited.date() == today) {
                m_pages[visited.time().hour()] += 1;
            }
        }
    }
}

// A visit before today's pages were read is in History when they are.
void TelemetrySampler::countVisit(qint64 at)
{
    const auto visited = QDateTime::fromMSecsSinceEpoch(at);
    if (m_pagesDay.isValid() && visited.date() == m_pagesDay) {
        m_pages[visited.time().hour()] += 1;
    }
}

void TelemetrySampler::forgetVisits() { m_pagesDay = {}; }

void TelemetrySampler::fillForTests()
{
    for (int tick = 0; tick <= fastKept; ++tick) {
        sample(static_cast<qint64>(tick) * fastInterval, tick % fastPerSecond == 0);
    }
}

void TelemetrySampler::setWallClockForTests(std::function<QDateTime()> clock)
{
    m_wallClock = std::move(clock);
}

void registerTelemetrySampler(TelemetrySampler *telemetry)
{
    qmlRegisterSingletonInstance("Omaweb", 1, 0, "TelemetrySampler", telemetry);
}

} // namespace omaweb
