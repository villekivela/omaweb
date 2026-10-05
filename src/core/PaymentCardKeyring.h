#pragma once

#include <QByteArray>
#include <QList>
#include <QMutex>
#include <QString>

#include <expected>
#include <memory>
#include <optional>

namespace omaweb {

// A saved card as the keyring keeps it: the random identifier Omaweb drew for
// it, which is all a keyring stores where it can be read without unlocking,
// and the secret, which holds everything about the card.
struct KeyringItem {
    QString id;
    QByteArray secret;
};

// Why a keyring gave no cards up.
enum class KeyringFailure {
    // The bus offered a Secret Service and nothing answered for it, as on a
    // session bus of its own that cannot start the desktop's keyring.
    Unreachable,
    // The reader did not unlock it.
    Locked,
    // It answered with an error.
    Failed,
};

// Where payment cards are kept: the desktop's Secret Service (ADR 0053).
// PaymentCards calls one from a single worker thread, because the desktop's
// store may hold a call open while it asks the reader to unlock it.
class PaymentCardKeyring {
public:
    virtual ~PaymentCardKeyring() = default;

    PaymentCardKeyring(const PaymentCardKeyring &) = delete;
    PaymentCardKeyring &operator=(const PaymentCardKeyring &) = delete;

    // Whether the desktop offers a secret store at all.
    virtual bool available() = 0;
    // Every saved card, unlocking the keyring if it is locked, or why the
    // keyring could not be read.
    virtual std::expected<QList<KeyringItem>, KeyringFailure> items() = 0;
    // Saves the secret under the id, replacing whatever was saved under it.
    virtual bool store(const QString &id, const QByteArray &secret) = 0;
    virtual bool remove(const QString &id) = 0;

protected:
    PaymentCardKeyring() = default;
};

// The desktop's Secret Service in a build with libsecret, and otherwise a
// keyring that is never available: a machine with no secret store has no
// payment cards.
std::unique_ptr<PaymentCardKeyring> makeDesktopKeyring();

// A keyring held in memory, for the UI lab and the tests. What it holds is
// shared, so the owner can look at it from another thread.
class MemoryPaymentCardKeyring final : public PaymentCardKeyring {
public:
    struct Contents {
        mutable QMutex mutex;
        QList<KeyringItem> items;
        bool available = true;
        // A keyring the reader did not unlock gives no cards up, until saving a
        // card asks again and the reader unlocks it, unless the reader
        // dismisses every prompt.
        bool locked = false;
        bool dismisses = false;
        // A keyring that did not answer, or answered with an error.
        std::optional<KeyringFailure> failure;
        // How many times the cards were read, which is when a real keyring
        // would ask to be unlocked.
        int reads = 0;

        qsizetype size() const;
        QByteArray secret(const QString &id) const;
        QStringList ids() const;
    };

    explicit MemoryPaymentCardKeyring(
        std::shared_ptr<Contents> contents = std::make_shared<Contents>());

    bool available() override;
    std::expected<QList<KeyringItem>, KeyringFailure> items() override;
    bool store(const QString &id, const QByteArray &secret) override;
    bool remove(const QString &id) override;

private:
    std::shared_ptr<Contents> m_contents;
};

} // namespace omaweb
