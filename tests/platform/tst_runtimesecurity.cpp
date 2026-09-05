#include "RuntimeSecurity.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using omaweb::meetsBaseline;
using omaweb::RuntimeSecurity;
using omaweb::SandboxHost;
using omaweb::sandboxDiagnostic;

namespace {

// A kernel's answers, written where the real ones are read from.
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
        // A tree this cannot write is a test that would go on asserting against
        // a host nobody described, and read whatever an absent file is taken to
        // mean. It stops here instead.
        if (!file.open(QIODevice::WriteOnly)) {
            qFatal("cannot write the stand-in proc file %s", qPrintable(path));
        }
        file.write(value.toUtf8() + '\n');
    }

    // Everything Chromium's renderer sandbox needs, present and permissive.
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
    return {QStringLiteral(OMAWEB_APPROVED_QTWEBENGINE),
        QStringLiteral(OMAWEB_APPROVED_CHROMIUM),
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

    // A platform with no prerequisites to read is not a host that failed one.
    QCOMPARE(sandboxDiagnostic({}), QString());
    // And the host this suite is running on answers without crashing, whatever
    // it happens to say.
    sandboxDiagnostic(SandboxHost::fromEnvironment());
}

void RuntimeSecurityTest::namesEverySandboxPrerequisiteItCannotFind_data()
{
    QTest::addColumn<QString>("setting");
    QTest::addColumn<QString>("value");
    QTest::addColumn<bool>("superuser");
    QTest::addColumn<QString>("expected");

    QTest::newRow("running as root") << QString() << QString() << true
        << QStringLiteral("superuser");
    QTest::newRow("user namespaces switched off")
        << QStringLiteral("sys/kernel/unprivileged_userns_clone") << QStringLiteral("0")
        << false << QStringLiteral("unprivileged_userns_clone");
    QTest::newRow("no user namespaces allowed")
        << QStringLiteral("sys/user/max_user_namespaces") << QStringLiteral("0")
        << false << QStringLiteral("max_user_namespaces");
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

    // A host that cannot be checked is not a host that passed, and a browser
    // that could not verify the isolation does not claim it.
    RuntimeSecurity blocked({root.path(), superuser}, approvedBuild());
    QVERIFY(!blocked.rendererIsolated());
    QVERIFY(!blocked.rendererIsolation().contains(QStringLiteral("sandboxed")));
}

void RuntimeSecurityTest::comparesAVersionAgainstTheApprovedBaseline_data()
{
    QTest::addColumn<QString>("running");
    QTest::addColumn<QString>("approved");
    QTest::addColumn<bool>("meets");

    QTest::newRow("the baseline itself") << QStringLiteral("6.11.2")
        << QStringLiteral("6.11.2") << true;
    QTest::newRow("a later patch") << QStringLiteral("6.11.3")
        << QStringLiteral("6.11.2") << true;
    QTest::newRow("a later minor") << QStringLiteral("6.12.0")
        << QStringLiteral("6.11.2") << true;
    QTest::newRow("an earlier patch") << QStringLiteral("6.11.1")
        << QStringLiteral("6.11.2") << false;
    QTest::newRow("an earlier minor") << QStringLiteral("6.10.9")
        << QStringLiteral("6.11.2") << false;
    QTest::newRow("a shorter equal version") << QStringLiteral("6.11")
        << QStringLiteral("6.11.0") << true;
    QTest::newRow("a four-part Chromium version") << QStringLiteral("151.0.7922.72")
        << QStringLiteral("151.0.7922.71") << true;
    QTest::newRow("a component that is not a number") << QStringLiteral("6.11.2-beta")
        << QStringLiteral("6.11.2") << false;
    QTest::newRow("no version at all") << QString() << QStringLiteral("6.11.2") << false;
    QTest::newRow("no baseline to meet") << QStringLiteral("6.11.2") << QString() << true;
    // A baseline file that has been corrupted is not permission to run: the
    // difference between no baseline and an unreadable one is the difference
    // between nothing to meet and nothing that can be checked.
    QTest::newRow("a baseline that cannot be read") << QStringLiteral("6.11.2")
        << QStringLiteral("six.eleven") << false;
}

void RuntimeSecurityTest::comparesAVersionAgainstTheApprovedBaseline()
{
    QFETCH(QString, running);
    QFETCH(QString, approved);
    QFETCH(bool, meets);
    QCOMPARE(meetsBaseline(running, approved), meets);
}

// Each page's renderer is sandboxed and Omaweb says so. QtWebEngine's network
// service runs inside the browser process, and Omaweb says that too rather than
// borrowing what Chromium's own builds do.
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
    // Whatever the wording becomes, the network service is never described as
    // isolated or sandboxed without being denied in the same breath.
    QVERIFY(!network.contains(QStringLiteral("is sandboxed")));
    QVERIFY(!network.contains(QStringLiteral("is isolated")));
    QVERIFY(security.rendererIsolation() != network);
}

void RuntimeSecurityTest::callsABuildBelowTheBaselineAnUnsupportedPreview()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    ProcTree(root.path()).writeSandboxableHost();
    const SandboxHost host{root.path(), false};

    RuntimeSecurity approved(host, approvedBuild());
    QVERIFY(approved.meetsSecurityBaseline());
    QVERIFY(!approved.securityBaseline().contains(QStringLiteral("unsupported preview")));
    QVERIFY(approved.securityBaseline().contains(approved.approvedEngineVersion()));

    // An engine carrying older Chromium security fixes is below the baseline
    // however new the Qt release around it is.
    RuntimeSecurity behind(host, {QStringLiteral("6.99.0"), QStringLiteral("140.0.7339.225"),
        QStringLiteral("100.0.1.1")});
    QVERIFY(!behind.meetsSecurityBaseline());
    QVERIFY(behind.securityBaseline().contains(QStringLiteral("unsupported preview")));
    QVERIFY(behind.securityBaseline().contains(QStringLiteral("100.0.1.1")));

    // A build that links no engine has no engine baseline to fall short of.
    RuntimeSecurity engineless(host, {});
    QVERIFY(engineless.securityBaseline().contains(QStringLiteral("no web engine")));
}

QTEST_MAIN(RuntimeSecurityTest)
#include "tst_runtimesecurity.moc"
