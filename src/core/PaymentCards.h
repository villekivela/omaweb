#pragma once

#include "PaymentCardKeyring.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <memory>

namespace omaweb {

// The reader's payment cards, kept in the desktop's keyring and nowhere else
// (ADR 0053). They are the reader's rather than a Space's. The cards are read
// from the keyring the first time something needs them, so a locked keyring
// asks to be unlocked then rather than when the browser starts, and are held
// in memory from then on; every change goes to the keyring on a thread of its
// own, because the keyring may hold a call open while it asks the reader.
//
// A card is shown by everything but its number: Settings and the suggestion
// list are given its last four digits. The number leaves only for a fill the
// reader picked.
class PaymentCards final : public QObject {
    Q_OBJECT

public:
    enum class State {
        Unread,
        Reading,
        Ready,
        // The desktop offers no secret store.
        Unavailable,
        // The keyring would not give the cards up, for the reason
        // KeyringFailure names.
        Unreachable,
        Locked,
        Failed,
    };

    explicit PaymentCards(std::unique_ptr<PaymentCardKeyring> keyring, QObject *parent = nullptr);
    ~PaymentCards() override;

    PaymentCards(const PaymentCards &) = delete;
    PaymentCards &operator=(const PaymentCards &) = delete;

    State state() const { return m_state; }
    // Reads the cards from the keyring, once: asking again while it is read,
    // or after it stayed locked, does nothing, so a reader who declined the
    // desktop's unlock prompt is not asked again by everything that looks.
    void read();
    // Asks the keyring again after it gave no cards up, which only the
    // reader's own asking does.
    void readAgain();

    // Each card as `id`, `last4`, `brand` (empty when the number's issuer is
    // not one Omaweb knows), `name`, `expiryMonth`, `expiryYear` (0 when not
    // given) and `nickname`, in the order they were added.
    QVariantList cards() const;
    // Adds the card, or edits the one its `id` names, and answers its id, or
    // nothing when it was not kept. A card needs a number that is one: 12 to 19
    // digits that pass the Luhn check, spaces and dashes aside. An edit with
    // no number keeps the saved one. The expiry is `expiry` as MM/YY or
    // MM/YYYY, or `expiryMonth` and `expiryYear`. The security code is never
    // read.
    QString save(const QVariantMap &card);
    bool remove(const QString &id);
    // Deletes every card from the keyring, read or not.
    void removeAll();
    // The card with its number, for the fill the reader picked.
    QVariantMap fill(const QString &id) const;
    // Whether a card with this number is saved.
    bool holds(const QString &number) const;

    // The number with spaces and dashes taken out, or nothing when it is not a
    // card number.
    static QString cardNumber(const QString &typed);
    static QString brand(const QString &number);

signals:
    void changed();

private:
    struct Card {
        QString id;
        QString number;
        QString name;
        int expiryMonth = 0;
        int expiryYear = 0;
        QString nickname;
        qint64 added = 0;
    };

    static QByteArray secretOf(const Card &card);
    static std::optional<Card> cardOf(const KeyringItem &item);
    QVariantMap shown(const Card &card) const;
    void setState(State state);
    void onWorker(std::function<void()> job);
    void reread();

    std::unique_ptr<PaymentCardKeyring> m_keyring;
    QThread m_thread;
    QObject *m_worker = nullptr;
    State m_state = State::Unread;
    // Counts the times every card was deleted, so a read that was on its way
    // before is not taken for what the keyring holds.
    int m_generation = 0;
    QList<Card> m_cards;
};

} // namespace omaweb
