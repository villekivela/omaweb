#include "PaymentCardKeyring.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

// The desktop keyring against a Secret Service of the test's own, on a
// session bus of the test's own. The bus is started before anything reads
// DBUS_SESSION_BUS_ADDRESS, from a configuration that names no service
// directory, so nothing here can reach or start the reader's real keyring.
class SecretServiceKeyringTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void hasNoKeyringWithoutASecretService();
    void keepsEachCardAsAnItemOnlyItsSecretDescribes();

private:
    QJsonArray storedItems();

    QTemporaryDir m_root;
    QProcess m_bus;
    QProcess m_service;
};

void SecretServiceKeyringTest::initTestCase()
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

void SecretServiceKeyringTest::cleanupTestCase()
{
    m_service.kill();
    m_service.waitForFinished();
    m_bus.kill();
    m_bus.waitForFinished();
}

// The items the fake Secret Service holds, as it would write them: each one's
// label and attributes, which a keyring keeps where anyone can read them, and
// its secret, which it keeps encrypted.
QJsonArray SecretServiceKeyringTest::storedItems()
{
    QDBusInterface fake(QStringLiteral("org.freedesktop.secrets"),
        QStringLiteral("/org/freedesktop/secrets"), QStringLiteral("dev.omaweb.FakeSecretService"),
        QDBusConnection::sessionBus());
    const QDBusReply<QString> reply = fake.call(QStringLiteral("Dump"));
    if (!reply.isValid()) {
        qWarning() << reply.error().message();
        return {};
    }
    return QJsonDocument::fromJson(reply.value().toUtf8()).array();
}

// A machine with no Secret Service has no keyring to keep a card in.
void SecretServiceKeyringTest::hasNoKeyringWithoutASecretService()
{
    const auto keyring = omaweb::makeDesktopKeyring();
    QVERIFY(!keyring->available());
    QVERIFY(!keyring->items().has_value());
    QVERIFY(!keyring->store(QStringLiteral("card"), QByteArrayLiteral("{}")));
}

// Each card is one item. Its attributes hold only Omaweb's schema name and the
// card's random identifier, and its label is the same for every card, so
// nothing about a card is readable without unlocking the keyring: the
// number, the name, the expiry and the nickname are all in the secret.
void SecretServiceKeyringTest::keepsEachCardAsAnItemOnlyItsSecretDescribes()
{
    m_service.start(QStringLiteral(OMAWEB_FAKE_SECRET_SERVICE_PATH), {});
    QVERIFY(m_service.waitForStarted());
    auto *bus = QDBusConnection::sessionBus().interface();
    QTRY_VERIFY_WITH_TIMEOUT(
        bus->isServiceRegistered(QStringLiteral("org.freedesktop.secrets")), 10000);

    const auto keyring = omaweb::makeDesktopKeyring();
    QVERIFY(keyring->available());
    QCOMPARE(keyring->items().value_or(QList<omaweb::KeyringItem> {}).size(), 0);

    const auto secret = QByteArrayLiteral(
        R"({"number":"4242424242424242","name":"Meri Laine","expiryMonth":8,"expiryYear":2029,"nickname":"Everyday"})");
    QVERIFY(keyring->store(QStringLiteral("9a3c1f0e-card"), secret));
    QVERIFY(keyring->store(QStringLiteral("other-card"), QByteArrayLiteral(R"({"number":"1"})")));

    const auto stored = storedItems();
    QCOMPARE(stored.size(), 2);
    const auto item = stored.at(0).toObject();
    QCOMPARE(item.value(QStringLiteral("label")).toString(), QStringLiteral("Omaweb payment card"));
    QCOMPARE(item.value(QStringLiteral("attributes")).toObject(),
        (QJsonObject {
            {QStringLiteral("xdg:schema"), QStringLiteral("dev.omaweb.browser.PaymentCard")},
            {QStringLiteral("id"), QStringLiteral("9a3c1f0e-card")}}));
    QCOMPARE(item.value(QStringLiteral("secret")).toString().toUtf8(), secret);
    for (const auto &value : stored) {
        const auto readable = QJsonDocument(
            QJsonObject {{QStringLiteral("label"), value.toObject().value(QStringLiteral("label"))},
                {QStringLiteral("attributes"),
                    value.toObject().value(QStringLiteral("attributes"))}})
                                  .toJson();
        QVERIFY2(!readable.contains("4242") && !readable.contains("Meri")
                && !readable.contains("Everyday") && !readable.contains("2029"),
            readable.constData());
    }

    const auto items = keyring->items();
    QVERIFY(items.has_value());
    QCOMPARE(items->size(), 2);
    for (const auto &read : *items) {
        if (read.id == QLatin1String("9a3c1f0e-card")) {
            QCOMPARE(read.secret, secret);
        } else {
            QCOMPARE(read.id, QStringLiteral("other-card"));
        }
    }

    // Saving under an id that holds a card replaces it rather than adding one.
    QVERIFY(
        keyring->store(QStringLiteral("9a3c1f0e-card"), QByteArrayLiteral(R"({"number":"2"})")));
    QCOMPARE(storedItems().size(), 2);

    QVERIFY(keyring->remove(QStringLiteral("9a3c1f0e-card")));
    const auto left = keyring->items();
    QVERIFY(left.has_value());
    QCOMPARE(left->size(), 1);
    QCOMPARE(left->first().id, QStringLiteral("other-card"));
    QCOMPARE(storedItems().size(), 1);
}

QTEST_GUILESS_MAIN(SecretServiceKeyringTest)

#include "tst_secretservicekeyring.moc"
