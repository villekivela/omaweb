#include "RuntimeSecurity.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using omaweb::meetsBaseline;
using omaweb::RuntimeSecurity;
using omaweb::sandboxDiagnostic;
using omaweb::SandboxHost;

namespace {

class ProcTree {
public:
    explicit ProcTree(const QString &root)
        : m_root(root)
    {
    }

    void write(const QString &relativePath, const QString &value) const
    {
        const auto path = QDir(m_root).filePath(relativePath);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning("cannot write the stand-in proc file %s", qPrintable(path));
            return;
        }
        file.write(value.toUtf8() + '\n');
    }

    void writeSandboxableHost() const
    {
        write(QStringLiteral("sys/user/max_user_namespaces"), QStringLiteral("15553"));
        write(QStringLiteral("sys/kernel/seccomp/actions_avail"),
            QStringLiteral("kill_process kill_thread trap errno user_notif trace log allow"));
    }

private:
    QString m_root;
};

RuntimeSecurity::EngineBuild approvedBuild()
{
    return {QStringLiteral(OMAWEB_APPROVED_QTWEBENGINE), QStringLiteral(OMAWEB_APPROVED_CHROMIUM),
        QStringLiteral(OMAWEB_APPROVED_CHROMIUM_SECURITY_PATCH)};
}

} // namespace

class RuntimeSecurityTest final : public QObject {
    Q_OBJECT

private slots:
    void saysNothingIsMissingOnAHostThatCanSandbox();
    void namesEverySandboxPrerequisiteItCannotFind_data();
    void namesEverySandboxPrerequisiteItCannotFind();
    void comparesAVersionAgainstTheApprovedBaseline_data();
    void comparesAVersionAgainstTheApprovedBaseline();
    void separatesRendererIsolationFromTheNetworkService();
    void callsABuildBelowTheBaselineAnUnsupportedPreview();
};

void RuntimeSecurityTest::saysNothingIsMissingOnAHostThatCanSandbox()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    ProcTree(root.path()).writeSandboxableHost();
    QCOMPARE(sandboxDiagnostic({root.path(), false}), QString());

    QCOMPARE(sandboxDiagnostic({}), QString());
    sandboxDiagnostic(SandboxHost::fromEnvironment());
}

void RuntimeSecurityTest::namesEverySandboxPrerequisiteItCannotFind_data()
{
    QTest::addColumn<QString>("setting");
    QTest::addColumn<QString>("value");
    QTest::addColumn<bool>("superuser");
    QTest::addColumn<QString>("expected");

    QTest::newRow("running as root")
        << QString() << QString() << true << QStringLiteral("superuser");
    QTest::newRow("user namespaces switched off")
        << QStringLiteral("sys/kernel/unprivileged_userns_clone") << QStringLiteral("0") << false
        << QStringLiteral("unprivileged_userns_clone");
    QTest::newRow("no user namespaces allowed")
        << QStringLiteral("sys/user/max_user_namespaces") << QStringLiteral("0") << false
        << QStringLiteral("max_user_namespaces");
}

void RuntimeSecurityTest::namesEverySandboxPrerequisiteItCannotFind()
{
    QFETCH(QString, setting);
    QFETCH(QString, value);
    QFETCH(bool, superuser);
    QFETCH(QString, expected);

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const ProcTree proc(root.path());
    proc.writeSandboxableHost();
    if (!setting.isEmpty()) {
        proc.write(setting, value);
    }

    const auto diagnostic = sandboxDiagnostic({root.path(), superuser});
    QVERIFY2(!diagnostic.isEmpty(), "a missing prerequisite is never silent");
    QVERIFY2(diagnostic.contains(expected), qPrintable(diagnostic));

    RuntimeSecurity blocked({root.path(), superuser}, approvedBuild());
    QVERIFY(!blocked.rendererIsolated());
    QVERIFY(!blocked.rendererIsolation().contains(QStringLiteral("sandboxed")));
}

void RuntimeSecurityTest::comparesAVersionAgainstTheApprovedBaseline_data()
{
    QTest::addColumn<QString>("running");
    QTest::addColumn<QString>("approved");
    QTest::addColumn<bool>("meets");

    QTest::newRow("the baseline itself")
        << QStringLiteral("6.11.2") << QStringLiteral("6.11.2") << true;
    QTest::newRow("a later patch") << QStringLiteral("6.11.3") << QStringLiteral("6.11.2") << true;
    QTest::newRow("a later minor") << QStringLiteral("6.12.0") << QStringLiteral("6.11.2") << true;
    QTest::newRow("an earlier patch")
        << QStringLiteral("6.11.1") << QStringLiteral("6.11.2") << false;
    QTest::newRow("an earlier minor")
        << QStringLiteral("6.10.9") << QStringLiteral("6.11.2") << false;
    QTest::newRow("a shorter equal version")
        << QStringLiteral("6.11") << QStringLiteral("6.11.0") << true;
    QTest::newRow("a four-part Chromium version")
        << QStringLiteral("151.0.7922.72") << QStringLiteral("151.0.7922.71") << true;
    QTest::newRow("a component that is not a number")
        << QStringLiteral("6.11.2-beta") << QStringLiteral("6.11.2") << false;
    QTest::newRow("no version at all") << QString() << QStringLiteral("6.11.2") << false;
    QTest::newRow("no baseline to meet") << QStringLiteral("6.11.2") << QString() << true;
    QTest::newRow("a baseline that cannot be read")
        << QStringLiteral("6.11.2") << QStringLiteral("six.eleven") << false;
}

void RuntimeSecurityTest::comparesAVersionAgainstTheApprovedBaseline()
{
    QFETCH(QString, running);
    QFETCH(QString, approved);
    QFETCH(bool, meets);
    QCOMPARE(meetsBaseline(running, approved), meets);
}

void RuntimeSecurityTest::separatesRendererIsolationFromTheNetworkService()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    ProcTree(root.path()).writeSandboxableHost();
    RuntimeSecurity security({root.path(), false}, approvedBuild());

    QVERIFY(security.rendererIsolated());
    QVERIFY(security.rendererIsolation().contains(QStringLiteral("renderer process")));
    QVERIFY(security.rendererIsolation().contains(QStringLiteral("sandboxed")));

    const auto network = security.networkService();
    QVERIFY(network.contains(QStringLiteral("network")));
    QVERIFY2(network.contains(QStringLiteral("not")), qPrintable(network));
    QVERIFY(!network.contains(QStringLiteral("is sandboxed")));
    QVERIFY(!network.contains(QStringLiteral("is isolated")));
    QVERIFY(security.rendererIsolation() != network);
}

void RuntimeSecurityTest::callsABuildBelowTheBaselineAnUnsupportedPreview()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    ProcTree(root.path()).writeSandboxableHost();
    const SandboxHost host {root.path(), false};

    RuntimeSecurity approved(host, approvedBuild());
    QVERIFY(approved.meetsSecurityBaseline());
    QVERIFY(!approved.securityBaseline().contains(QStringLiteral("unsupported preview")));
    QVERIFY(approved.securityBaseline().contains(approved.approvedEngineVersion()));

    RuntimeSecurity behind(host,
        {QStringLiteral("6.99.0"), QStringLiteral("140.0.7339.225"), QStringLiteral("100.0.1.1")});
    QVERIFY(!behind.meetsSecurityBaseline());
    QVERIFY(behind.securityBaseline().contains(QStringLiteral("unsupported preview")));
    QVERIFY(behind.securityBaseline().contains(QStringLiteral("100.0.1.1")));

    RuntimeSecurity engineless(host, {});
    QVERIFY(engineless.securityBaseline().contains(QStringLiteral("no web engine")));
}

QTEST_MAIN(RuntimeSecurityTest)
#include "tst_runtimesecurity.moc"
