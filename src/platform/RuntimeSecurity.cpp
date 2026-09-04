#include "RuntimeSecurity.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQmlEngine>

#if defined(Q_OS_LINUX)
#include <unistd.h>
#endif

namespace omaweb {
namespace {

// A kernel setting, or an empty string where the file is not there at all. The
// two answers are not the same: a setting that is absent is a kernel that never
// had the knob, and one that reads zero is a kernel that has been told no.
QString kernelSetting(const QString &procRoot, const QString &relativePath)
{
    QFile file(QDir(procRoot).filePath(relativePath));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readLine()).trimmed();
}

QList<int> versionComponents(const QString &version)
{
    QList<int> components;
    const auto fields = version.trimmed().split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (fields.isEmpty()) {
        return components;
    }
    for (const auto &field : fields) {
        auto valid = false;
        const auto value = field.toInt(&valid);
        if (!valid || value < 0) {
            return {};
        }
        components.append(value);
    }
    return components;
}

} // namespace

SandboxHost SandboxHost::fromEnvironment()
{
    SandboxHost host;
#if defined(Q_OS_LINUX)
    host.procRoot = QStringLiteral("/proc");
    host.superuser = ::geteuid() == 0;
#endif
    return host;
}

QString sandboxDiagnostic(const SandboxHost &host)
{
    if (host.superuser) {
        return QStringLiteral(
            "Omaweb is running as the superuser. Chromium will not sandbox a renderer as "
            "root, and Omaweb does not run renderers without a sandbox. Start Omaweb as an "
            "ordinary user.");
    }
    // No prerequisites to read: a platform that sandboxes without being
    // configured to, not a host that failed.
    if (host.procRoot.isEmpty()) {
        return {};
    }
    if (!QFileInfo(host.procRoot).isDir()) {
        return QStringLiteral(
            "%1 is not readable, so Omaweb cannot check whether this host can isolate a "
            "renderer. Mount proc, or run Omaweb outside a container that hides it.")
            .arg(host.procRoot);
    }
    // Chromium's Linux sandbox puts each renderer in its own user namespace.
    // Older kernels gate that behind a switch of their own; every kernel caps
    // the number, and a cap of zero is the same as the switch being off.
    if (kernelSetting(host.procRoot, QStringLiteral("sys/kernel/unprivileged_userns_clone"))
        == QStringLiteral("0")) {
        return QStringLiteral(
            "Unprivileged user namespaces are turned off on this host "
            "(kernel.unprivileged_userns_clone=0). Chromium's renderer sandbox needs them. "
            "Set kernel.unprivileged_userns_clone=1.");
    }
    if (kernelSetting(host.procRoot, QStringLiteral("sys/user/max_user_namespaces"))
        == QStringLiteral("0")) {
        return QStringLiteral(
            "This host allows no user namespaces (user.max_user_namespaces=0). Chromium's "
            "renderer sandbox needs them. Raise user.max_user_namespaces above zero.");
    }
    // The second layer: the filter that decides which system calls a sandboxed
    // renderer may make at all. A kernel built without it leaves the renderer
    // holding the whole system-call surface.
    if (!QFileInfo::exists(
            QDir(host.procRoot).filePath(QStringLiteral("sys/kernel/seccomp/actions_avail")))) {
        return QStringLiteral(
            "This kernel reports no seccomp-bpf filtering, which Chromium's renderer "
            "sandbox needs. Use a kernel built with CONFIG_SECCOMP_FILTER.");
    }
    return {};
}

bool meetsBaseline(const QString &running, const QString &approved)
{
    // Nothing to meet: a build with no baseline written for it is not held
    // against one. A baseline that is there but unreadable is a different
    // thing — a corrupted file must not read as permission.
    if (approved.trimmed().isEmpty()) {
        return true;
    }
    const auto approvedComponents = versionComponents(approved);
    const auto runningComponents = versionComponents(running);
    if (approvedComponents.isEmpty() || runningComponents.isEmpty()) {
        return false;
    }
    const auto count = std::max(runningComponents.size(), approvedComponents.size());
    for (qsizetype index = 0; index < count; ++index) {
        const auto left = index < runningComponents.size() ? runningComponents.at(index) : 0;
        const auto right = index < approvedComponents.size() ? approvedComponents.at(index) : 0;
        if (left != right) {
            return left > right;
        }
    }
    return true;
}

RuntimeSecurity::RuntimeSecurity(SandboxHost host, EngineBuild build, QObject *parent)
    : QObject(parent)
    , m_diagnostic(omaweb::sandboxDiagnostic(host))
    , m_build(std::move(build))
{
}

QString RuntimeSecurity::sandboxDiagnostic() const
{
    return m_diagnostic;
}

bool RuntimeSecurity::rendererIsolated() const
{
    return m_diagnostic.isEmpty();
}

QString RuntimeSecurity::rendererIsolation() const
{
    if (!rendererIsolated()) {
        return QStringLiteral(
            "Renderer isolation is unverified on this host, so Omaweb does not claim it.");
    }
    return QStringLiteral(
        "Each page runs in its own renderer process, sandboxed by the operating system.");
}

QString RuntimeSecurity::networkService() const
{
    return QStringLiteral(
        "QtWebEngine handles the network inside the browser process. That network service is "
        "not a sandboxed process of its own, and Omaweb does not describe it as isolated.");
}

QString RuntimeSecurity::engineVersion() const
{
    return m_build.engineVersion;
}

QString RuntimeSecurity::chromiumVersion() const
{
    return m_build.chromiumVersion;
}

QString RuntimeSecurity::chromiumSecurityPatchVersion() const
{
    return m_build.chromiumSecurityPatchVersion;
}

QString RuntimeSecurity::approvedEngineVersion() const
{
    return QStringLiteral(OMAWEB_APPROVED_QTWEBENGINE);
}

QString RuntimeSecurity::approvedChromiumSecurityPatchVersion() const
{
    return QStringLiteral(OMAWEB_APPROVED_CHROMIUM_SECURITY_PATCH);
}

bool RuntimeSecurity::meetsSecurityBaseline() const
{
    return meetsBaseline(m_build.engineVersion, approvedEngineVersion())
        && meetsBaseline(m_build.chromiumSecurityPatchVersion,
            approvedChromiumSecurityPatchVersion());
}

QString RuntimeSecurity::securityBaseline() const
{
    if (m_build.engineVersion.isEmpty()) {
        return QStringLiteral(
            "This build links no web engine, so there is no engine security baseline to meet.");
    }
    if (!meetsSecurityBaseline()) {
        return QStringLiteral(
            "QtWebEngine %1, carrying Chromium security fixes up to %2, is below the approved "
            "baseline of QtWebEngine %3 and Chromium %4. This build is an unsupported preview.")
            .arg(m_build.engineVersion, m_build.chromiumSecurityPatchVersion,
                approvedEngineVersion(), approvedChromiumSecurityPatchVersion());
    }
    return QStringLiteral(
        "QtWebEngine %1 on Chromium %2, carrying security fixes up to %3, meets the approved "
        "baseline of QtWebEngine %4 and Chromium %5.")
        .arg(m_build.engineVersion, m_build.chromiumVersion,
            m_build.chromiumSecurityPatchVersion, approvedEngineVersion(),
            approvedChromiumSecurityPatchVersion());
}

void registerRuntimeSecurity(RuntimeSecurity *runtimeSecurity)
{
    qmlRegisterSingletonInstance("Omaweb", 1, 0, "RuntimeSecurity", runtimeSecurity);
}

} // namespace omaweb
