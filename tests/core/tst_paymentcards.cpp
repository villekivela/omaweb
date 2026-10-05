#include "BrowserController.h"
#include "PaymentCardKeyring.h"
#include "PaymentCards.h"
#include "PrivateSessionFixture.h"
#include "SpaceStorage.h"

#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include <memory>
#include <optional>

using omaweb::BrowserController;
using omaweb::MemoryPaymentCardKeyring;
using omaweb::PaymentCards;
using omaweb::SpaceStorage;
using omaweb::test::PrivateSessionFixture;

namespace {

const QString visaNumber = QStringLiteral("4242424242424242");

QVariantMap visa()
{
    return {{QStringLiteral("number"), QStringLiteral("4242 4242 4242 4242")},
        {QStringLiteral("name"), QStringLiteral("Meri Laine")},
        {QStringLiteral("expiry"), QStringLiteral("08/29")},
        {QStringLiteral("nickname"), QStringLiteral("Everyday")}};
}

// What Settings and the suggestion list are given for a card: never its number.
QVariantMap shown(const QString &id, const QString &last4, const QString &brand,
    const QString &name, int month, int year, const QString &nickname)
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("last4"), last4},
        {QStringLiteral("brand"), brand}, {QStringLiteral("name"), name},
        {QStringLiteral("expiryMonth"), month}, {QStringLiteral("expiryYear"), year},
        {QStringLiteral("nickname"), nickname}};
}

} // namespace

Q_DECLARE_METATYPE(std::optional<omaweb::KeyringFailure>)

class PaymentCardsTest : public QObject {
    Q_OBJECT

private slots:
    void listsAddsEditsAndRemovesCardsShowingOnlyTheLastFour();
    void savesOnlyACardNumberThatIsOne();
    void keepsNoCardOutsideTheKeyring();
    void readsTheKeyringOnlyWhenACardIsNeeded();
    void offersNoCardInAPrivateWindow();
    void hasNoCardsWithoutASecretService();
    void knowsASavedCardByItsNumber();
    void clearsCardsOnlyWhenAsked();
    void asksALockedKeyringAgainOnlyWhenTheReaderDoes();
    void namesWhyTheKeyringGaveNoCards_data();
    void namesWhyTheKeyringGaveNoCards();
    void addingACardAsksALockedKeyringAgain();
    void clearingWhileTheKeyringIsReadKeepsNoCard();
};

// Settings adds a card, lists it by its last four digits and never by its
// number, edits it without the number being typed again or with a new one,
// and removes it from the keyring.
void PaymentCardsTest::listsAddsEditsAndRemovesCardsShowingOnlyTheLastFour()
{
    QTemporaryDir root;
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.setPaymentCards(&cards);

    QVERIFY(controller.paymentCards().isEmpty());
    QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("ready"));

    const auto id = controller.savePaymentCard(visa());
    QVERIFY(!id.isEmpty());
    QTRY_COMPARE(controller.paymentCards(),
        QVariantList {shown(id, QStringLiteral("4242"), QStringLiteral("Visa"),
            QStringLiteral("Meri Laine"), 8, 2029, QStringLiteral("Everyday"))});
    QTRY_COMPARE(keyring->size(), 1);

    // An edit that leaves the number empty keeps the one saved.
    QCOMPARE(controller.savePaymentCard(
                 {{QStringLiteral("id"), id}, {QStringLiteral("number"), QString()},
                     {QStringLiteral("name"), QStringLiteral("Meri A. Laine")},
                     {QStringLiteral("expiry"), QStringLiteral("09/2030")},
                     {QStringLiteral("nickname"), QString()}}),
        id);
    QTRY_COMPARE(controller.paymentCards(),
        QVariantList {shown(id, QStringLiteral("4242"), QStringLiteral("Visa"),
            QStringLiteral("Meri A. Laine"), 9, 2030, QString())});
    QCOMPARE(
        controller.paymentCardForFill(id).value(QStringLiteral("number")).toString(), visaNumber);

    // A new number replaces it.
    QCOMPARE(controller.savePaymentCard({{QStringLiteral("id"), id},
                 {QStringLiteral("number"), QStringLiteral("5555-5555-5555-4444")},
                 {QStringLiteral("name"), QStringLiteral("Meri A. Laine")},
                 {QStringLiteral("expiry"), QStringLiteral("09/30")}}),
        id);
    QTRY_COMPARE(controller.paymentCards(),
        QVariantList {shown(id, QStringLiteral("4444"), QStringLiteral("Mastercard"),
            QStringLiteral("Meri A. Laine"), 9, 2030, QString())});
    QCOMPARE(keyring->size(), 1);

    // An id nothing was saved under is not a way to add a card.
    auto unknown = visa();
    unknown.insert(QStringLiteral("id"), QStringLiteral("unknown"));
    QVERIFY(controller.savePaymentCard(unknown).isEmpty());

    QVERIFY(controller.removePaymentCard(id));
    QTRY_VERIFY(controller.paymentCards().isEmpty());
    QTRY_COMPARE(keyring->size(), 0);
}

// A number is 12 to 19 digits that pass the Luhn check, with spaces and dashes
// aside, and an expiry is a month and a year, or neither.
void PaymentCardsTest::savesOnlyACardNumberThatIsOne()
{
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    cards.read();
    QTRY_COMPARE(cards.state(), PaymentCards::State::Ready);

    const auto with = [](const QString &key, const QString &value) {
        auto card = visa();
        card.insert(key, value);
        return card;
    };
    QVERIFY(cards.save(with(QStringLiteral("number"), QStringLiteral("4242 4242 4242 4241")))
            .isEmpty());
    QVERIFY(cards.save(with(QStringLiteral("number"), QStringLiteral("4242 4242 42"))).isEmpty());
    QVERIFY(cards.save(with(QStringLiteral("number"), QStringLiteral("4242x4242x4242x4242")))
            .isEmpty());
    QVERIFY(cards.save(with(QStringLiteral("number"), QString())).isEmpty());
    QVERIFY(cards.save(with(QStringLiteral("expiry"), QStringLiteral("13/29"))).isEmpty());
    QVERIFY(cards.save(with(QStringLiteral("expiry"), QStringLiteral("soon"))).isEmpty());
    QVERIFY(cards.cards().isEmpty());

    // No expiry is an expiry the reader did not give.
    QVERIFY(!cards.save(with(QStringLiteral("expiry"), QString())).isEmpty());
    // An American Express number is fifteen digits.
    QVERIFY(
        !cards.save(with(QStringLiteral("number"), QStringLiteral("3782 822463 10005"))).isEmpty());
    QCOMPARE(cards.cards().size(), 2);
    QCOMPARE(cards.cards().at(0).toMap().value(QStringLiteral("expiryMonth")).toInt(), 0);
    QCOMPARE(cards.cards().at(1).toMap().value(QStringLiteral("brand")).toString(),
        QStringLiteral("American Express"));
    QTRY_COMPARE(keyring->size(), 2);
}

// The keyring keeps the card under a random identifier, with everything
// about it in the secret, and Omaweb writes nothing about it anywhere else:
// not the number, not the last four digits.
void PaymentCardsTest::keepsNoCardOutsideTheKeyring()
{
    QTemporaryDir root;
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    QString id;
    {
        PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
        BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")),
            root.filePath(QStringLiteral("config")));
        controller.setPaymentCards(&cards);
        controller.paymentCards();
        QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("ready"));
        id = controller.savePaymentCard(visa());
        QVERIFY(!id.isEmpty());
        QVERIFY(controller.clearBrowsingData({QStringLiteral("history")}, 0));
    }
    QCOMPARE(keyring->ids(), QStringList {id});
    QVERIFY(!QUuid::fromString(id).isNull());
    const auto secret = QJsonDocument::fromJson(keyring->secret(id)).object();
    QCOMPARE(secret.value(QStringLiteral("number")).toString(), visaNumber);
    QCOMPARE(secret.value(QStringLiteral("name")).toString(), QStringLiteral("Meri Laine"));
    QCOMPARE(secret.value(QStringLiteral("expiryMonth")).toInt(), 8);
    QCOMPARE(secret.value(QStringLiteral("expiryYear")).toInt(), 2029);
    QCOMPARE(secret.value(QStringLiteral("nickname")).toString(), QStringLiteral("Everyday"));

    QDirIterator files(root.path(), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    int read = 0;
    while (files.hasNext()) {
        QFile file(files.next());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto contents = file.readAll();
        ++read;
        QVERIFY2(!contents.contains(visaNumber.toLatin1()), qPrintable(file.fileName()));
        QVERIFY2(!contents.contains("4242 4242"), qPrintable(file.fileName()));
        QVERIFY2(!contents.contains("Everyday"), qPrintable(file.fileName()));
    }
    QVERIFY(read > 0);
}

// A locked keyring asks to be unlocked when it is read, so it is read when a
// card is first needed and not when the browser starts.
void PaymentCardsTest::readsTheKeyringOnlyWhenACardIsNeeded()
{
    QTemporaryDir root;
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.setPaymentCards(&cards);
    QTest::qWait(100);
    QCOMPARE(controller.paymentCardsState(), QStringLiteral("unread"));
    QCOMPARE(keyring->reads, 0);
    QVERIFY(controller.savePaymentCard(visa()).isEmpty());

    QSignalSpy changed(&controller, &BrowserController::paymentCardsChanged);
    controller.paymentCards();
    QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("ready"));
    QVERIFY(changed.count() > 0);
    controller.paymentCards();
    QTest::qWait(100);
    QCOMPARE(keyring->reads, 1);
}

// A Private window has no identity to keep a card for: it neither lists nor
// saves one, whatever it is handed.
void PaymentCardsTest::offersNoCardInAPrivateWindow()
{
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    keyring->items.append({.id = QStringLiteral("saved"),
        .secret = QByteArrayLiteral(R"({"number":"4242424242424242"})")});
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    cards.read();
    QTRY_COMPARE(cards.state(), PaymentCards::State::Ready);
    QCOMPARE(cards.cards().size(), 1);

    PrivateSessionFixture session;
    const auto controller = session.createController();
    controller->setPaymentCards(&cards);
    QCOMPARE(controller->paymentCardsState(), QStringLiteral("unavailable"));
    QVERIFY(controller->paymentCards().isEmpty());
    QVERIFY(controller->savePaymentCard(visa()).isEmpty());
    QVERIFY(controller->paymentCardForFill(QStringLiteral("saved")).isEmpty());
    QVERIFY(!controller->paymentCardSaved(visaNumber));
    QVERIFY(!controller->removePaymentCard(QStringLiteral("saved")));
    QCOMPARE(keyring->size(), 1);
}

// A machine with no Secret Service has no payment cards, and Omaweb keeps
// none of its own.
void PaymentCardsTest::hasNoCardsWithoutASecretService()
{
    QTemporaryDir root;
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    keyring->available = false;
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.setPaymentCards(&cards);
    QVERIFY(controller.paymentCards().isEmpty());
    QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("unavailable"));
    QVERIFY(controller.savePaymentCard(visa()).isEmpty());

    // A window given no cards at all, like a build without libsecret, has none.
    BrowserController bare(SpaceStorage(root.path(), QStringLiteral("other")));
    QCOMPARE(bare.paymentCardsState(), QStringLiteral("unavailable"));
    QVERIFY(bare.savePaymentCard(visa()).isEmpty());
}

// Typing a saved card into a form again is no reason to offer saving it.
void PaymentCardsTest::knowsASavedCardByItsNumber()
{
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    cards.read();
    QTRY_COMPARE(cards.state(), PaymentCards::State::Ready);
    QVERIFY(!cards.holds(visaNumber));
    QVERIFY(!cards.save(visa()).isEmpty());
    QVERIFY(cards.holds(visaNumber));
    QVERIFY(cards.holds(QStringLiteral("4242-4242-4242-4242")));
    QVERIFY(!cards.holds(QStringLiteral("5555555555554444")));
    QVERIFY(!cards.holds(QStringLiteral("4242")));

    QTemporaryDir root;
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY(controller.isPaymentCardNumber(QStringLiteral("4242 4242 4242 4242")));
    QVERIFY(!controller.isPaymentCardNumber(QStringLiteral("4242 4242 4242 4241")));
    QVERIFY(!controller.isPaymentCardNumber(QStringLiteral("12345")));
}

// A card is not browsing history: clearing a day's history keeps the cards,
// and Payment cards, asked for by name, deletes every one from the keyring.
void PaymentCardsTest::clearsCardsOnlyWhenAsked()
{
    QTemporaryDir root;
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.setPaymentCards(&cards);
    controller.paymentCards();
    QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("ready"));
    QVERIFY(!controller.savePaymentCard(visa()).isEmpty());
    QTRY_COMPARE(keyring->size(), 1);

    QVERIFY(controller.clearBrowsingData(
        {QStringLiteral("history"), QStringLiteral("forms"), QStringLiteral("cookies")}, 0));
    QTest::qWait(100);
    QCOMPARE(keyring->size(), 1);
    QCOMPARE(controller.paymentCards().size(), 1);

    QVERIFY(controller.clearBrowsingData({QStringLiteral("cards")}, 86400000));
    QVERIFY(controller.paymentCards().isEmpty());
    QTRY_COMPARE(keyring->size(), 0);
}

// A reader who left the desktop's unlock prompt unanswered is not asked again
// by everything that looks at the cards, only by asking again themselves.
void PaymentCardsTest::asksALockedKeyringAgainOnlyWhenTheReaderDoes()
{
    QTemporaryDir root;
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    keyring->locked = true;
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.setPaymentCards(&cards);
    controller.paymentCards();
    QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("locked"));
    for (int asked = 0; asked < 3; ++asked) {
        QVERIFY(controller.paymentCards().isEmpty());
    }
    QTest::qWait(100);
    QCOMPARE(controller.paymentCardsState(), QStringLiteral("locked"));
    QCOMPARE(keyring->reads, 1);

    {
        const QMutexLocker locker(&keyring->mutex);
        keyring->locked = false;
    }
    controller.readPaymentCardsAgain();
    QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("ready"));
    QCOMPARE(keyring->reads, 2);
}

// Settings says why the keyring gave no cards: nothing answered for it, the
// reader left it locked, or it answered with an error. Each can be asked again.
void PaymentCardsTest::namesWhyTheKeyringGaveNoCards_data()
{
    QTest::addColumn<std::optional<omaweb::KeyringFailure>>("failure");
    QTest::addColumn<bool>("locked");
    QTest::addColumn<QString>("state");
    QTest::newRow("unreachable") << std::optional(omaweb::KeyringFailure::Unreachable) << false
                                 << "unreachable";
    QTest::newRow("locked") << std::optional<omaweb::KeyringFailure>() << true << "locked";
    QTest::newRow("failed") << std::optional(omaweb::KeyringFailure::Failed) << false << "failed";
}

void PaymentCardsTest::namesWhyTheKeyringGaveNoCards()
{
    QFETCH(std::optional<omaweb::KeyringFailure>, failure);
    QFETCH(bool, locked);
    QFETCH(QString, state);
    QTemporaryDir root;
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    keyring->failure = failure;
    keyring->locked = locked;
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.setPaymentCards(&cards);
    controller.paymentCards();
    QTRY_COMPARE(controller.paymentCardsState(), state);

    {
        const QMutexLocker locker(&keyring->mutex);
        keyring->failure.reset();
        keyring->locked = false;
    }
    controller.readPaymentCardsAgain();
    QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("ready"));
}

// A keyring the reader left locked takes a new card: saving it asks the
// desktop to unlock the keyring again, and once the reader does, the cards it
// held are read with the new one.
void PaymentCardsTest::addingACardAsksALockedKeyringAgain()
{
    QTemporaryDir root;
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    keyring->items.append({.id = QStringLiteral("saved"),
        .secret = QByteArrayLiteral(R"({"number":"5555555555554444","added":1})")});
    keyring->locked = true;
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.setPaymentCards(&cards);
    controller.paymentCards();
    QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("locked"));

    const auto id = controller.savePaymentCard(visa());
    QVERIFY(!id.isEmpty());
    QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("ready"));
    const auto shownCards = controller.paymentCards();
    QCOMPARE(shownCards.size(), 2);
    QCOMPARE(shownCards.at(0).toMap().value(QStringLiteral("id")), QStringLiteral("saved"));
    QCOMPARE(shownCards.at(1).toMap().value(QStringLiteral("id")), id);
    QCOMPARE(keyring->size(), 2);
}

// Clearing the cards while the keyring is still being read does not let the
// read bring back what was deleted.
void PaymentCardsTest::clearingWhileTheKeyringIsReadKeepsNoCard()
{
    QTemporaryDir root;
    auto keyring = std::make_shared<MemoryPaymentCardKeyring::Contents>();
    keyring->items.append({.id = QStringLiteral("saved"),
        .secret = QByteArrayLiteral(R"({"number":"4242424242424242"})")});
    PaymentCards cards(std::make_unique<MemoryPaymentCardKeyring>(keyring));
    BrowserController controller(SpaceStorage(root.path(), QStringLiteral("test")));
    controller.setPaymentCards(&cards);
    controller.paymentCards();
    QVERIFY(controller.clearBrowsingData({QStringLiteral("cards")}, 0));
    QTRY_COMPARE(controller.paymentCardsState(), QStringLiteral("ready"));
    QTest::qWait(100);
    QVERIFY(controller.paymentCards().isEmpty());
    QCOMPARE(keyring->size(), 0);
}

QTEST_GUILESS_MAIN(PaymentCardsTest)

#include "tst_paymentcards.moc"
