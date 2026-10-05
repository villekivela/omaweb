#include "PaymentCardKeyring.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

// The desktop keyring against a Secret Service of the test's own, on a
// session bus of the test's own. The bus is started before anything reads
// DBUS_SESSION_BUS_ADDRESS, from a configuration whose only service directory
// is the test's own, so nothing here can reach or start the reader's real
// keyring.
class SecretServiceKeyringTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void hasNoKeyringWithoutASecretService();
    void namesAKeyringThatNothingAnswersFor();
    void keepsEachCardAsAnItemOnlyItsSecretDescribes();
    void namesAKeyringThatRefuses();
    void namesAKeyringTheReaderLeftLocked();
    void savingAsksALockedKeyringToUnlock();
    void savingCreatesTheDefaultCollection();

private:
    QJsonArray storedItems();
    bool offerService(const QString &command);
    bool startService();
    bool behave(const QString &behaviour);

    QTemporaryDir m_root;
    QProcess m_bus;
    QProcess m_service;
};

void SecretServiceKeyringTest::initTestCase()
{
    QVERIFY(m_root.isValid());
    QVERIFY(QDir(m_root.path()).mkdir(QStringLiteral("services")));
    QFile configuration(m_root.filePath(QStringLiteral("bus.conf")));
    QVERIFY(configuration.open(QIODevice::WriteOnly));
    configuration.write(QStringLiteral(R"(<!DOCTYPE busconfig PUBLIC
  "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
  "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:dir=%1</listen>
  <auth>EXTERNAL</auth>
  <servicedir>%1/services</servicedir>
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

// Makes the Secret Service one the bus starts by running the command, or
// takes it away when the command is empty.
bool SecretServiceKeyringTest::offerService(const QString &command)
{
    QFile file(m_root.filePath(QStringLiteral("services/org.freedesktop.secrets.service")));
    if (command.isEmpty()) {
        file.remove();
    } else {
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(QStringLiteral("[D-BUS Service]\nName=org.freedesktop.secrets\nExec=%1\n")
                .arg(command)
                .toUtf8());
        file.close();
    }
    const auto reloaded = QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("ReloadConfig")));
    return reloaded.type() == QDBusMessage::ReplyMessage;
}

// Starts the fake Secret Service, unless it is running.
bool SecretServiceKeyringTest::startService()
{
    if (m_service.state() != QProcess::NotRunning) {
        return true;
    }
    m_service.start(QStringLiteral(OMAWEB_FAKE_SECRET_SERVICE_PATH), {});
    if (!m_service.waitForStarted()) {
        return false;
    }
    auto *bus = QDBusConnection::sessionBus().interface();
    return QTest::qWaitFor(
        [bus] { return bus->isServiceRegistered(QStringLiteral("org.freedesktop.secrets")); },
        10000);
}

// Sets how the fake Secret Service answers, as its header describes.
bool SecretServiceKeyringTest::behave(const QString &behaviour)
{
    QDBusInterface fake(QStringLiteral("org.freedesktop.secrets"),
        QStringLiteral("/org/freedesktop/secrets"), QStringLiteral("dev.omaweb.FakeSecretService"),
        QDBusConnection::sessionBus());
    return fake.call(QStringLiteral("Behave"), behaviour).type() == QDBusMessage::ReplyMessage;
}

// A machine with no Secret Service has no keyring to keep a card in.
void SecretServiceKeyringTest::hasNoKeyringWithoutASecretService()
{
    const auto keyring = omaweb::makeDesktopKeyring();
    QVERIFY(!keyring->available());
    QVERIFY(!keyring->items().has_value());
    QVERIFY(!keyring->store(QStringLiteral("card"), QByteArrayLiteral("{}")));
}

// A bus that offers a Secret Service it cannot start, as a session bus of
// Omaweb's own does when the desktop's keyring is already running on another,
// is a keyring nothing answers for: not one that stayed locked.
void SecretServiceKeyringTest::namesAKeyringThatNothingAnswersFor()
{
    // A program that exits without taking the name, as the desktop's keyring
    // daemon does when it finds itself already running. That one exits
    // cleanly and is answered after the bus's 25 seconds; this one fails, and
    // is answered at once.
    QVERIFY(offerService(QStringLiteral("/bin/false")));
    const auto keyring = omaweb::makeDesktopKeyring();
    QVERIFY(keyring->available());

    // The keyring's own message is logged, and nothing about the card is.
    const QRegularExpression message(
        QStringLiteral("^The keyring answered: .*StartServiceByName.*org\\.freedesktop\\.secrets"));
    QTest::failOnWarning(QRegularExpression(QStringLiteral("4242|Meri")));
    QTest::ignoreMessage(QtWarningMsg, message);
    const auto items = keyring->items();
    QVERIFY(!items.has_value());
    QCOMPARE(items.error(), omaweb::KeyringFailure::Unreachable);
    QTest::ignoreMessage(QtWarningMsg, message);
    QVERIFY(!keyring->store(QStringLiteral("card"),
        QByteArrayLiteral(R"({"number":"4242424242424242","name":"Meri Laine"})")));
    QVERIFY(offerService({}));
    QVERIFY(!keyring->available());
}

// Each card is one item. Its attributes hold only Omaweb's schema name and the
// card's random identifier, and its label is the same for every card, so
// nothing about a card is readable without unlocking the keyring: the
// number, the name, the expiry and the nickname are all in the secret.
void SecretServiceKeyringTest::keepsEachCardAsAnItemOnlyItsSecretDescribes()
{
    QVERIFY(startService());

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

// A keyring that answers with an error is named as one, and its error is
// logged.
void SecretServiceKeyringTest::namesAKeyringThatRefuses()
{
    QVERIFY(startService());
    QVERIFY(behave(QStringLiteral("refusing")));
    const auto keyring = omaweb::makeDesktopKeyring();
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("^The keyring answered: .*AccessDenied")));
    const auto items = keyring->items();
    QVERIFY(!items.has_value());
    QCOMPARE(items.error(), omaweb::KeyringFailure::Failed);
    QVERIFY(behave({}));
}

// A keyring whose unlock prompt the reader dismissed stayed locked, and says
// so, rather than answering with none of its cards.
void SecretServiceKeyringTest::namesAKeyringTheReaderLeftLocked()
{
    QVERIFY(startService());
    const auto keyring = omaweb::makeDesktopKeyring();
    QVERIFY(keyring->store(QStringLiteral("locked-card"), QByteArrayLiteral(R"({"number":"1"})")));
    QVERIFY(behave(QStringLiteral("locked")));
    const auto items = keyring->items();
    QVERIFY(!items.has_value());
    QCOMPARE(items.error(), omaweb::KeyringFailure::Locked);
    QVERIFY(behave({}));
    QVERIFY(keyring->remove(QStringLiteral("locked-card")));
}

// Saving a card into a locked keyring asks the desktop to unlock it, and the
// card is kept once the reader does.
void SecretServiceKeyringTest::savingAsksALockedKeyringToUnlock()
{
    QVERIFY(startService());
    QVERIFY(behave(QStringLiteral("unlocking")));
    const auto keyring = omaweb::makeDesktopKeyring();
    QVERIFY(
        keyring->store(QStringLiteral("unlocked-card"), QByteArrayLiteral(R"({"number":"3"})")));
    const auto items = keyring->items();
    QVERIFY(items.has_value());
    QVERIFY(std::ranges::any_of(*items,
        [](const omaweb::KeyringItem &item) { return item.id == QLatin1String("unlocked-card"); }));
    QVERIFY(keyring->remove(QStringLiteral("unlocked-card")));
    QVERIFY(behave({}));
}

// A keyring with no default collection yet, as a desktop's is before anything
// was saved in it, is asked to create one when the first card is saved.
void SecretServiceKeyringTest::savingCreatesTheDefaultCollection()
{
    QVERIFY(startService());
    QVERIFY(behave(QStringLiteral("no-default")));
    const auto keyring = omaweb::makeDesktopKeyring();
    QVERIFY(keyring->items().has_value());
    QVERIFY(keyring->store(QStringLiteral("first-card"), QByteArrayLiteral(R"({"number":"4"})")));
    const auto stored = storedItems();
    QVERIFY(std::ranges::any_of(stored, [](const QJsonValue &item) {
        return item.toObject()
                   .value(QStringLiteral("attributes"))
                   .toObject()
                   .value(QStringLiteral("id"))
            == QLatin1String("first-card");
    }));
    QVERIFY(keyring->remove(QStringLiteral("first-card")));
    QVERIFY(behave({}));
}

QTEST_GUILESS_MAIN(SecretServiceKeyringTest)

#include "tst_secretservicekeyring.moc"
