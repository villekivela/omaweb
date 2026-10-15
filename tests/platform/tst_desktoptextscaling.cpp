#include "DesktopTextScaling.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusVariant>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <limits>

using omaweb::DesktopTextScaling;

namespace {

const auto kService = QStringLiteral("org.freedesktop.portal.Desktop");
const auto kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const auto kInterface = QStringLiteral("org.gnome.desktop.interface");
const auto kKey = QStringLiteral("text-scaling-factor");

// The desktop's settings portal, holding the values a test gives it and
// announcing a change the way the real one does when a reader runs
// `omarchy-display-text-size`.
class StubSettingsPortal final : public QObject {
    Q_OBJECT

public:
    QHash<QString, QVariant> values;
};

class SettingsPortalAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Settings")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    explicit SettingsPortalAdaptor(StubSettingsPortal *portal)
        : QDBusAbstractAdaptor(portal)
        , m_portal(portal)
    {
    }

    uint version() const { return 2; }

    void change(const QString &nameSpace, const QString &key, const QVariant &value)
    {
        m_portal->values.insert(nameSpace + u'/' + key, value);
        emit SettingChanged(nameSpace, key, QDBusVariant(value));
    }

public slots:
    // The older `Read` wraps the value in a second variant, as the portal
    // still answers it.
    QDBusVariant Read(const QString &nameSpace, const QString &key)
    {
        const auto value = m_portal->values.value(nameSpace + u'/' + key);
        return QDBusVariant(QVariant::fromValue(QDBusVariant(value)));
    }

signals:
    void SettingChanged(const QString &nameSpace, const QString &key, const QDBusVariant &value);

private:
    StubSettingsPortal *m_portal;
};

} // namespace

class DesktopTextScalingTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void readsOneWhereNoPortalAnswers();
    void readsTheFactorThePortalHolds();
    void followsAChangeWhileRunning();
    void readsAFactorItCannotUseAsOne();

private:
    QTemporaryDir m_root;
    QProcess m_bus;
    StubSettingsPortal *m_portal = nullptr;
    SettingsPortalAdaptor *m_adaptor = nullptr;

    void standUpThePortal();
};

// A bus of its own that starts no service: under `dbus-run-session` the bus
// would start the desktop's real portal for the first `Read`, and the test
// would read the factor of the machine it runs on.
void DesktopTextScalingTest::initTestCase()
{
    QVERIFY(m_root.isValid());
    QFile configuration(m_root.filePath(QStringLiteral("bus.conf")));
    QVERIFY(configuration.open(QIODevice::WriteOnly));
    configuration.write(QStringLiteral(R"(<!DOCTYPE busconfig PUBLIC
  "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
  "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:dir=%1</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
)")
            .arg(m_root.path())
            .toUtf8());
    configuration.close();

    m_bus.start(QStringLiteral("dbus-daemon"),
        {QStringLiteral("--config-file=") + configuration.fileName(), QStringLiteral("--nofork"),
            QStringLiteral("--print-address=1")});
    QVERIFY2(m_bus.waitForStarted(), "dbus-daemon did not start");
    QVERIFY(m_bus.waitForReadyRead(10000));
    const auto address = QString::fromUtf8(m_bus.readLine()).trimmed();
    QVERIFY(!address.isEmpty());
    qputenv("DBUS_SESSION_BUS_ADDRESS", address.toUtf8());
}

void DesktopTextScalingTest::cleanupTestCase()
{
    m_bus.kill();
    m_bus.waitForFinished();
}

void DesktopTextScalingTest::standUpThePortal()
{
    if (m_portal) {
        return;
    }
    auto bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    m_portal = new StubSettingsPortal;
    m_adaptor = new SettingsPortalAdaptor(m_portal);
    m_portal->values.insert(kInterface + u'/' + kKey, 1.3636);
    QVERIFY(bus.registerObject(kPath, m_portal));
    QVERIFY(bus.registerService(kService));
}

// Runs first, before the stub owns the portal's name: a desktop without one
// enlarges nothing.
void DesktopTextScalingTest::readsOneWhereNoPortalAnswers()
{
    QVERIFY(QDBusConnection::sessionBus().isConnected());
    DesktopTextScaling scaling;
    QSignalSpy changed(&scaling, &DesktopTextScaling::factorChanged);
    QTest::qWait(300);
    QCOMPARE(scaling.factor(), 1.0);
    QCOMPARE(changed.count(), 0);
}

void DesktopTextScalingTest::readsTheFactorThePortalHolds()
{
    standUpThePortal();
    DesktopTextScaling scaling;
    QTRY_COMPARE(scaling.factor(), 1.3636);
}

void DesktopTextScalingTest::followsAChangeWhileRunning()
{
    standUpThePortal();
    DesktopTextScaling scaling;
    QTRY_COMPARE(scaling.factor(), 1.3636);
    QSignalSpy changed(&scaling, &DesktopTextScaling::factorChanged);

    m_adaptor->change(kInterface, QStringLiteral("enable-animations"), false);
    m_adaptor->change(kInterface, kKey, 1.5);
    QTRY_COMPARE(scaling.factor(), 1.5);
    QCOMPARE(changed.count(), 1);

    // `omarchy-display-text-size reset` puts the factor back to 1.
    m_adaptor->change(kInterface, kKey, 1.0);
    QTRY_COMPARE(scaling.factor(), 1.0);
    QCOMPARE(changed.count(), 2);
}

// GSettings holds the key as a double above zero. Anything else enlarges
// nothing.
void DesktopTextScalingTest::readsAFactorItCannotUseAsOne()
{
    QCOMPARE(omaweb::portalTextScalingFactor(QVariant(1.25)), 1.25);
    QCOMPARE(omaweb::portalTextScalingFactor(QVariant(0.0)), 1.0);
    QCOMPARE(omaweb::portalTextScalingFactor(QVariant(-2.0)), 1.0);
    QCOMPARE(
        omaweb::portalTextScalingFactor(QVariant(std::numeric_limits<double>::quiet_NaN())), 1.0);
    QCOMPARE(
        omaweb::portalTextScalingFactor(QVariant(std::numeric_limits<double>::infinity())), 1.0);
    QCOMPARE(omaweb::portalTextScalingFactor(QVariant(QStringLiteral("1.5"))), 1.0);
    QCOMPARE(omaweb::portalTextScalingFactor(QVariant()), 1.0);
}

QTEST_GUILESS_MAIN(DesktopTextScalingTest)
#include "tst_desktoptextscaling.moc"
