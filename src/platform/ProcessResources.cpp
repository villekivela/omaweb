#include "ProcessResources.h"

#include <QQmlEngine>

#if defined(Q_OS_MACOS)
#include <libproc.h>
#include <sys/resource.h>
#elif defined(Q_OS_LINUX)
#include <QByteArray>
#include <QFile>

#include <unistd.h>
#endif

namespace tanto {

ProcessResources::ProcessResources(QObject *parent)
    : QObject(parent)
{
}

bool ProcessResources::available() const
{
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

qint64 ProcessResources::residentBytes(qint64 pid) const
{
    if (pid <= 0) {
        return 0;
    }
#if defined(Q_OS_MACOS)
    // The engine's renderers are Tanto's own children and so run as the same
    // user, which is all this call needs; it answers for no one else's.
    rusage_info_current usage {};
    if (proc_pid_rusage(static_cast<int>(pid), RUSAGE_INFO_CURRENT,
            reinterpret_cast<rusage_info_t *>(&usage))
        != 0) {
        return 0;
    }
    return static_cast<qint64>(usage.ri_resident_size);
#elif defined(Q_OS_LINUX)
    QFile statm(QStringLiteral("/proc/%1/statm").arg(pid));
    if (!statm.open(QIODevice::ReadOnly)) {
        return 0;
    }
    // The second field is resident pages.
    const auto fields = statm.readLine().simplified().split(' ');
    if (fields.size() < 2) {
        return 0;
    }
    bool read = false;
    const auto pages = fields.at(1).toLongLong(&read);
    return read ? pages * static_cast<qint64>(::sysconf(_SC_PAGESIZE)) : 0;
#else
    return 0;
#endif
}

void registerProcessResources()
{
    qmlRegisterSingletonType<ProcessResources>("Tanto", 1, 0, "ProcessResources",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new ProcessResources; });
}

} // namespace tanto
