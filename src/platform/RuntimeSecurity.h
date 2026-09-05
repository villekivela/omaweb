#pragma once

#include <QObject>
#include <QString>

namespace omaweb {

// The host facts a renderer sandbox depends on, read from one place so a test
// can hand over a different one.
//
// `procRoot` is where the kernel's own settings are read from. An empty one
// means this platform has no prerequisites to check — macOS sandboxes without
// being asked — rather than a host that failed the check.
struct SandboxHost {
    QString procRoot {};
    bool superuser = false;

    static SandboxHost fromEnvironment();
};

// Empty when this host can isolate renderers. Otherwise what is missing and
// what the reader has to do about it, because a browser that quietly carried on
// without the sandbox would be claiming an isolation it does not have.
QString sandboxDiagnostic(const SandboxHost &host);

// Whether a dotted version is at or above another. Missing components count as
// zero, so "6.11" meets "6.11.0"; anything unparsable meets nothing, because a
// version Omaweb cannot read is not one it can vouch for.
bool meetsBaseline(const QString &running, const QString &approved);

// What Omaweb says about its own runtime security, and what it refuses to say.
//
// Two things are separate and are kept separate in the wording. Each page runs
// in its own renderer process that the operating system sandboxes, which is
// isolation Omaweb can verify. QtWebEngine's network service runs inside the
// browser process, which is not sandboxed at all — Chromium's own builds put it
// in a process of its own, and describing QtWebEngine's as isolated because
// Chromium's is would be describing someone else's build.
//
// The engine's version is the third fact: a build below the approved security
// baseline is an unsupported preview and says so, rather than looking like
// every other build.
class RuntimeSecurity final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString sandboxDiagnostic READ sandboxDiagnostic CONSTANT)
    Q_PROPERTY(bool rendererIsolated READ rendererIsolated CONSTANT)
    Q_PROPERTY(QString rendererIsolation READ rendererIsolation CONSTANT)
    Q_PROPERTY(QString networkService READ networkService CONSTANT)
    Q_PROPERTY(QString engineVersion READ engineVersion CONSTANT)
    Q_PROPERTY(QString chromiumVersion READ chromiumVersion CONSTANT)
    Q_PROPERTY(QString chromiumSecurityPatchVersion READ chromiumSecurityPatchVersion CONSTANT)
    Q_PROPERTY(QString approvedEngineVersion READ approvedEngineVersion CONSTANT)
    Q_PROPERTY(QString approvedChromiumSecurityPatchVersion
        READ approvedChromiumSecurityPatchVersion CONSTANT)
    Q_PROPERTY(bool meetsSecurityBaseline READ meetsSecurityBaseline CONSTANT)
    Q_PROPERTY(QString securityBaseline READ securityBaseline CONSTANT)

public:
    // What the engine says about itself. Handed in rather than read here: the
    // browser's own build is the only one that links a web engine, and this
    // has to answer for a build that links none.
    struct EngineBuild {
        QString engineVersion {};
        QString chromiumVersion {};
        // The Chromium release whose security fixes this engine carries, which
        // is a later release than the one it is built on and the only one of
        // the two a baseline can be written against.
        QString chromiumSecurityPatchVersion {};
    };

    RuntimeSecurity(SandboxHost host, EngineBuild build, QObject *parent = nullptr);

    QString sandboxDiagnostic() const;
    bool rendererIsolated() const;
    QString rendererIsolation() const;
    QString networkService() const;
    QString engineVersion() const;
    QString chromiumVersion() const;
    QString chromiumSecurityPatchVersion() const;
    QString approvedEngineVersion() const;
    QString approvedChromiumSecurityPatchVersion() const;
    bool meetsSecurityBaseline() const;
    QString securityBaseline() const;

private:
    QString m_diagnostic;
    EngineBuild m_build;
};

// Makes one `RuntimeSecurity` available to QML as `import Omaweb`. The instance
// is the caller's; QML never creates one, because the engine facts come from
// the application that links the engine.
void registerRuntimeSecurity(RuntimeSecurity *runtimeSecurity);

} // namespace omaweb
