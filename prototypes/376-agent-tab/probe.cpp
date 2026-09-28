#include "probe.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTextStream>
#include <rhi/qrhi.h>

#include <functional>
#include <map>
#include <numeric>

#if defined(Q_OS_MACOS)
#include <libproc.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif

namespace {

// The delegate is the item Chromium's input and frames go through. Qt creates it as a child of the
// WebEngineView and does not export its type, so it is found by class name.
QQuickItem *delegateOf(QQuickItem *view)
{
    if (view == nullptr)
        return nullptr;
    for (QQuickItem *child : view->childItems()) {
        if (QByteArray(child->metaObject()->className())
                .contains("RenderWidgetHostViewQtDelegateItem"))
            return child;
        if (QQuickItem *found = delegateOf(child))
            return found;
    }
    return nullptr;
}

struct ProcessTimes {
    qint64 parent = 0;
    double seconds = 0;
    QString type;
};

QString typeFromArguments(const QString &arguments)
{
    for (const auto type : {"gpu-process", "renderer", "utility", "zygote"}) {
        if (arguments.contains(QStringLiteral("--type=") + QLatin1StringView(type)))
            return QString::fromLatin1(type);
    }
    return QStringLiteral("browser");
}

#if defined(Q_OS_MACOS)
std::map<qint64, ProcessTimes> processes()
{
    std::map<qint64, ProcessTimes> out;
    mach_timebase_info_data_t base;
    mach_timebase_info(&base);
    std::vector<pid_t> pids(4096);
    const int count = proc_listallpids(pids.data(), int(pids.size() * sizeof(pid_t)));
    for (int i = 0; i < count; ++i) {
        const pid_t pid = pids[i];
        proc_bsdinfo bsd {};
        proc_taskinfo task {};
        if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsd, sizeof bsd) != sizeof bsd)
            continue;
        if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &task, sizeof task) != sizeof task)
            continue;
        ProcessTimes times;
        times.parent = bsd.pbi_ppid;
        times.seconds
            = double(task.pti_total_user + task.pti_total_system) * base.numer / base.denom / 1e9;
        out[pid] = times;
    }
    return out;
}

QString argumentsOf(qint64 pid)
{
    int mib[3] = {CTL_KERN, KERN_PROCARGS2, int(pid)};
    size_t size = 0;
    if (sysctl(mib, 3, nullptr, &size, nullptr, 0) != 0)
        return {};
    QByteArray buffer(qsizetype(size), '\0');
    if (sysctl(mib, 3, buffer.data(), &size, nullptr, 0) != 0)
        return {};
    buffer.replace('\0', ' ');
    return QString::fromLocal8Bit(buffer);
}
#else
std::map<qint64, ProcessTimes> processes()
{
    std::map<qint64, ProcessTimes> out;
    const double ticks = double(sysconf(_SC_CLK_TCK));
    for (const QString &entry : QDir(QStringLiteral("/proc")).entryList(QDir::Dirs)) {
        bool numeric = false;
        const qint64 pid = entry.toLongLong(&numeric);
        if (!numeric)
            continue;
        QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
        if (!stat.open(QIODevice::ReadOnly))
            continue;
        const QByteArray line = stat.readAll();
        // The command name is in parentheses and may hold spaces, so fields are counted from the
        // last closing parenthesis: state, ppid, ... utime is the 12th after it, stime the 13th.
        const QList<QByteArray> fields = line.mid(line.lastIndexOf(')') + 2).split(' ');
        if (fields.size() < 13)
            continue;
        ProcessTimes times;
        times.parent = fields[1].toLongLong();
        times.seconds = (fields[11].toDouble() + fields[12].toDouble()) / ticks;
        out[pid] = times;
    }
    return out;
}

QString argumentsOf(qint64 pid)
{
    QFile file(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray buffer = file.readAll();
    buffer.replace('\0', ' ');
    return QString::fromLocal8Bit(buffer);
}
#endif

} // namespace

Probe::Probe(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
}

QString Probe::delegateClass(QQuickItem *view) const
{
    QQuickItem *delegate = delegateOf(view);
    return delegate ? QString::fromLatin1(delegate->metaObject()->className()) : QString();
}

bool Probe::mouseClick(QQuickItem *view, qreal x, qreal y)
{
    QQuickItem *delegate = delegateOf(view);
    if (delegate == nullptr)
        return false;
    const QPointF local(x, y);
    const QPointF scene = delegate->mapToScene(local);
    const QPointF global = delegate->mapToGlobal(local);
    const QPointingDevice *mouse = QPointingDevice::primaryPointingDevice();
    QMouseEvent move(
        QEvent::MouseMove, local, scene, global, Qt::NoButton, Qt::NoButton, Qt::NoModifier, mouse);
    QCoreApplication::sendEvent(delegate, &move);
    QMouseEvent press(QEvent::MouseButtonPress, local, scene, global, Qt::LeftButton,
        Qt::LeftButton, Qt::NoModifier, mouse);
    QCoreApplication::sendEvent(delegate, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, local, scene, global, Qt::LeftButton,
        Qt::NoButton, Qt::NoModifier, mouse);
    QCoreApplication::sendEvent(delegate, &release);
    return press.isAccepted();
}

bool Probe::typeText(QQuickItem *view, const QString &text)
{
    QQuickItem *delegate = delegateOf(view);
    if (delegate == nullptr)
        return false;
    for (const QChar character : text) {
        const int key = character.toUpper().unicode();
        // A real key press is preceded by ShortcutOverride, which is where the delegate works out
        // an editing command, so the sequence is sent the way a window would deliver it.
        QKeyEvent shortcut(QEvent::ShortcutOverride, key, Qt::NoModifier, QString(character));
        QCoreApplication::sendEvent(delegate, &shortcut);
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(character));
        QCoreApplication::sendEvent(delegate, &press);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString(character));
        QCoreApplication::sendEvent(delegate, &release);
    }
    return true;
}

bool Probe::quietClick(QQuickItem *view, qreal x, qreal y)
{
    QQuickItem *delegate = delegateOf(view);
    if (delegate == nullptr || view->window() == nullptr)
        return false;
    QQuickWindow *window = view->window();
    QQuickItem *previous = window->activeFocusItem();
    // Filter every delegate in the window, since the one losing focus is whichever page had it.
    QList<QQuickItem *> delegates;
    std::function<void(QQuickItem *)> collect = [&](QQuickItem *item) {
        for (QQuickItem *child : item->childItems()) {
            if (QByteArray(child->metaObject()->className())
                    .contains("RenderWidgetHostViewQtDelegateItem"))
                delegates.append(child);
            collect(child);
        }
    };
    collect(window->contentItem());
    for (QQuickItem *item : delegates)
        item->installEventFilter(this);
    const bool accepted = mouseClick(view, x, y);
    if (previous != nullptr)
        previous->forceActiveFocus(Qt::OtherFocusReason);
    for (QQuickItem *item : delegates)
        item->removeEventFilter(this);
    return accepted;
}

bool Probe::eventFilter(QObject *, QEvent *event)
{
    return event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut;
}

bool Probe::keyTap(QQuickItem *view, int key)
{
    QQuickItem *delegate = delegateOf(view);
    if (delegate == nullptr)
        return false;
    const QString text = key == Qt::Key_Return ? QStringLiteral("\r")
        : key == Qt::Key_Space                 ? QStringLiteral(" ")
                                               : QString();
    QKeyEvent shortcut(QEvent::ShortcutOverride, key, Qt::NoModifier, text);
    QCoreApplication::sendEvent(delegate, &shortcut);
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
    QCoreApplication::sendEvent(delegate, &press);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, text);
    QCoreApplication::sendEvent(delegate, &release);
    return true;
}

bool Probe::sendFocusIn(QQuickItem *view)
{
    QQuickItem *delegate = delegateOf(view);
    if (delegate == nullptr)
        return false;
    QFocusEvent focus(QEvent::FocusIn, Qt::OtherFocusReason);
    QCoreApplication::sendEvent(delegate, &focus);
    return true;
}

QString Probe::activeFocus(QQuickWindow *window) const
{
    QQuickItem *item = window ? window->activeFocusItem() : nullptr;
    while (item != nullptr && item->objectName().isEmpty())
        item = item->parentItem();
    return item ? item->objectName() : QStringLiteral("none");
}

QVariantMap Probe::cpuSeconds(qint64 agentRenderer, qint64 readerRenderer) const
{
    const auto all = processes();
    std::map<qint64, bool> ours;
    const qint64 self = QCoreApplication::applicationPid();
    ours[self] = true;
    // Descendants: repeat until no new child is found, since a zygote's children are grandchildren.
    for (bool grew = true; grew;) {
        grew = false;
        for (const auto &[pid, times] : all) {
            if (!ours.contains(pid) && ours.contains(times.parent)) {
                ours[pid] = true;
                grew = true;
            }
        }
    }
    QVariantMap out;
    double total = 0;
    for (const auto &[pid, flag] : ours) {
        const auto found = all.find(pid);
        if (found == all.end())
            continue;
        QString type
            = pid == self ? QStringLiteral("browser") : typeFromArguments(argumentsOf(pid));
        if (pid == agentRenderer)
            type = QStringLiteral("agentRenderer");
        else if (pid == readerRenderer)
            type = QStringLiteral("readerRenderer");
        out[type] = out.value(type).toDouble() + found->second.seconds;
        total += found->second.seconds;
    }
    out[QStringLiteral("total")] = total;
    return out;
}

void Probe::watchFrames(QQuickWindow *window)
{
    if (m_watched != nullptr)
        m_watched->disconnect(this);
    m_watched = window;
    m_frameNanoseconds.clear();
    m_gpuMilliseconds.clear();
    m_frameStart = 0;
    connect(
        window, &QQuickWindow::beforeFrameBegin, this,
        [this] { m_frameStart = m_clock.nsecsElapsed(); }, Qt::DirectConnection);
    connect(
        window, &QQuickWindow::afterFrameEnd, this,
        [this] {
            if (m_frameStart <= 0)
                return;
            m_frameNanoseconds.push_back(m_clock.nsecsElapsed() - m_frameStart);
            // Zero unless QSG_RHI_PROFILE=1 turned the GPU timestamps on, as in
            // tests/ui/ProbeClock.cpp.
            auto *renderer = m_watched->rendererInterface();
            auto *swapChain = static_cast<QRhiSwapChain *>(
                renderer->getResource(m_watched, QSGRendererInterface::RhiSwapchainResource));
            if (swapChain != nullptr)
                m_gpuMilliseconds.push_back(
                    swapChain->currentFrameCommandBuffer()->lastCompletedGpuTime() * 1e3);
        },
        Qt::DirectConnection);
}

QVariantMap Probe::frameReport()
{
    if (m_watched != nullptr) {
        m_watched->disconnect(this);
        m_watched = nullptr;
    }
    const auto mean = [](const auto &values) {
        return values.empty() ? 0.0
                              : std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    };
    return {
        {QStringLiteral("frames"), int(m_frameNanoseconds.size())},
        {QStringLiteral("cpuMsPerFrame"), mean(m_frameNanoseconds) / 1e6},
        {QStringLiteral("gpuMsPerFrame"), mean(m_gpuMilliseconds)},
    };
}

QString Probe::pixel(const QImage &image, qreal fx, qreal fy) const
{
    if (image.isNull())
        return QStringLiteral("null");
    const int x = qBound(0, int(fx * image.width()), image.width() - 1);
    const int y = qBound(0, int(fy * image.height()), image.height() - 1);
    return image.pixelColor(x, y).name();
}

QString Probe::windowPixel(QQuickWindow *window, qreal fx, qreal fy) const
{
    return pixel(window->grabWindow(), fx, fy);
}

bool Probe::saveImage(const QImage &image, const QString &path) const { return image.save(path); }

double Probe::milliseconds() const { return m_clock.nsecsElapsed() / 1e6; }

void Probe::emit_(const QString &line) const
{
    QTextStream out(stdout);
    out << line << Qt::endl;
}
