#pragma once

#include <QObject>
#include <QString>

namespace omaweb {

struct SandboxHost {
    // Tests replace /proc with a fixture directory. An empty path skips checks
    // on platforms that do not expose Linux sandbox prerequisites.
    QString procRoot {};
    bool superuser = false;

    static SandboxHost fromEnvironment();
};

QString sandboxDiagnostic(const SandboxHost &host);

// Invalid versions fail the check. Missing components compare as zero.
bool meetsBaseline(const QString &running, const QString &approved);

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
    Q_PROPERTY(QString approvedChromiumSecurityPatchVersion READ
            approvedChromiumSecurityPatchVersion CONSTANT)
    Q_PROPERTY(bool meetsSecurityBaseline READ meetsSecurityBaseline CONSTANT)
    Q_PROPERTY(QString securityBaseline READ securityBaseline CONSTANT)

public:
    struct EngineBuild {
        QString engineVersion {};
        QString chromiumVersion {};
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

void registerRuntimeSecurity(RuntimeSecurity *runtimeSecurity);

} // namespace omaweb
