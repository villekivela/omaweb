#include "PaymentCards.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>
#include <optional>

namespace omaweb {

namespace {

    bool passesLuhn(const QString &digits)
    {
        int sum = 0;
        bool doubled = false;
        for (auto index = digits.size() - 1; index >= 0; --index) {
            int digit = digits.at(index).digitValue();
            if (doubled) {
                digit *= 2;
                if (digit > 9) {
                    digit -= 9;
                }
            }
            sum += digit;
            doubled = !doubled;
        }
        return sum % 10 == 0;
    }

    // MM/YY or MM/YYYY, with a slash, a dash or nothing between; empty is no
    // expiry. Anything else is not one.
    std::optional<std::pair<int, int>> expiryOf(const QString &typed)
    {
        const auto text = typed.trimmed();
        if (text.isEmpty()) {
            return std::pair {0, 0};
        }
        static const QRegularExpression pattern(
            QStringLiteral(R"(^(\d{1,2})\s*[/-]?\s*(\d{2}|\d{4})$)"));
        const auto match = pattern.match(text);
        if (!match.hasMatch()) {
            return std::nullopt;
        }
        return std::pair {match.captured(1).toInt(), match.captured(2).toInt()};
    }

    // A month and a year, or neither. A two-digit year is this century's.
    std::optional<std::pair<int, int>> validExpiry(int month, int year)
    {
        if (month == 0 && year == 0) {
            return std::pair {0, 0};
        }
        if (year >= 0 && year < 100) {
            year += 2000;
        }
        if (month < 1 || month > 12 || year < 2000 || year > 2099) {
            return std::nullopt;
        }
        return std::pair {month, year};
    }

} // namespace

PaymentCards::PaymentCards(std::unique_ptr<PaymentCardKeyring> keyring, QObject *parent)
    : QObject(parent)
    , m_keyring(std::move(keyring))
    , m_worker(new QObject)
{
    m_thread.setObjectName(QStringLiteral("omaweb-payment-cards"));
    m_worker->moveToThread(&m_thread);
    m_thread.start();
}

// The thread is stopped by the last job it is given, so a change still on its
// way to the keyring is not dropped when the browser closes.
PaymentCards::~PaymentCards()
{
    onWorker([] { QThread::currentThread()->quit(); });
    m_thread.wait();
    delete m_worker;
}

void PaymentCards::onWorker(std::function<void()> job)
{
    QMetaObject::invokeMethod(m_worker, std::move(job), Qt::QueuedConnection);
}

void PaymentCards::setState(State state)
{
    m_state = state;
    emit changed();
}

void PaymentCards::read()
{
    if (m_state != State::Unread) {
        return;
    }
    setState(State::Reading);
    reread();
}

void PaymentCards::readAgain()
{
    if (m_state != State::Unreadable) {
        return;
    }
    setState(State::Reading);
    reread();
}

void PaymentCards::reread()
{
    auto *keyring = m_keyring.get();
    onWorker([this, keyring, generation = m_generation] {
        const bool available = keyring->available();
        const auto items = available ? keyring->items() : std::nullopt;
        QMetaObject::invokeMethod(
            this,
            [this, available, items, generation] {
                if (generation != m_generation) {
                    return;
                }
                if (!available) {
                    m_cards.clear();
                    setState(State::Unavailable);
                    return;
                }
                if (!items) {
                    setState(State::Unreadable);
                    return;
                }
                QList<Card> cards;
                for (const auto &item : *items) {
                    if (const auto card = cardOf(item)) {
                        cards.append(*card);
                    }
                }
                std::stable_sort(cards.begin(), cards.end(),
                    [](const Card &left, const Card &right) { return left.added < right.added; });
                m_cards = cards;
                setState(State::Ready);
            },
            Qt::QueuedConnection);
    });
}

QByteArray PaymentCards::secretOf(const Card &card)
{
    return QJsonDocument(QJsonObject {
                             {QStringLiteral("number"), card.number},
                             {QStringLiteral("name"), card.name},
                             {QStringLiteral("expiryMonth"), card.expiryMonth},
                             {QStringLiteral("expiryYear"), card.expiryYear},
                             {QStringLiteral("nickname"), card.nickname},
                             {QStringLiteral("added"), card.added},
                         })
        .toJson(QJsonDocument::Compact);
}

std::optional<PaymentCards::Card> PaymentCards::cardOf(const KeyringItem &item)
{
    const auto object = QJsonDocument::fromJson(item.secret).object();
    const auto number = cardNumber(object.value(QStringLiteral("number")).toString());
    if (item.id.isEmpty() || number.isEmpty()) {
        return std::nullopt;
    }
    return Card {.id = item.id,
        .number = number,
        .name = object.value(QStringLiteral("name")).toString(),
        .expiryMonth = object.value(QStringLiteral("expiryMonth")).toInt(),
        .expiryYear = object.value(QStringLiteral("expiryYear")).toInt(),
        .nickname = object.value(QStringLiteral("nickname")).toString(),
        .added = object.value(QStringLiteral("added")).toInteger()};
}

QString PaymentCards::cardNumber(const QString &typed)
{
    QString digits;
    for (const auto character : typed) {
        if (character.isDigit() && character.unicode() < 128) {
            digits.append(character);
        } else if (character != QLatin1Char(' ') && character != QLatin1Char('-')) {
            return {};
        }
    }
    if (digits.size() < 12 || digits.size() > 19 || !passesLuhn(digits)) {
        return {};
    }
    return digits;
}

// The issuer, from the number's leading digits, for the networks a reader in
// Europe meets. Brand names are names, and stay as the networks write them.
QString PaymentCards::brand(const QString &number)
{
    const auto prefix = [&number](int length) { return number.left(length).toInt(); };
    if (number.startsWith(QLatin1Char('4'))) {
        return QStringLiteral("Visa");
    }
    if ((prefix(2) >= 51 && prefix(2) <= 55) || (prefix(4) >= 2221 && prefix(4) <= 2720)) {
        return QStringLiteral("Mastercard");
    }
    if (prefix(2) == 34 || prefix(2) == 37) {
        return QStringLiteral("American Express");
    }
    if (prefix(4) == 6011 || prefix(2) == 65 || (prefix(3) >= 644 && prefix(3) <= 649)) {
        return QStringLiteral("Discover");
    }
    if (prefix(4) >= 3528 && prefix(4) <= 3589) {
        return QStringLiteral("JCB");
    }
    if (prefix(2) == 36 || (prefix(3) >= 300 && prefix(3) <= 305)) {
        return QStringLiteral("Diners Club");
    }
    if (prefix(2) == 62) {
        return QStringLiteral("UnionPay");
    }
    return {};
}

QVariantMap PaymentCards::shown(const Card &card) const
{
    return {{QStringLiteral("id"), card.id}, {QStringLiteral("last4"), card.number.right(4)},
        {QStringLiteral("brand"), brand(card.number)}, {QStringLiteral("name"), card.name},
        {QStringLiteral("expiryMonth"), card.expiryMonth},
        {QStringLiteral("expiryYear"), card.expiryYear},
        {QStringLiteral("nickname"), card.nickname}};
}

QVariantList PaymentCards::cards() const
{
    QVariantList cards;
    for (const auto &card : m_cards) {
        cards.append(shown(card));
    }
    return cards;
}

// The cards in memory are changed at once, and the keyring after; a keyring
// that refuses the change is read again, so what is shown is what it holds.
QString PaymentCards::save(const QVariantMap &card)
{
    if (m_state != State::Ready) {
        return {};
    }
    const auto id = card.value(QStringLiteral("id")).toString();
    const auto saved = std::find_if(
        m_cards.begin(), m_cards.end(), [&id](const Card &kept) { return kept.id == id; });
    if (!id.isEmpty() && saved == m_cards.end()) {
        return {};
    }
    const auto typed = card.value(QStringLiteral("number")).toString().trimmed();
    auto number = cardNumber(typed);
    if (number.isEmpty()) {
        if (!typed.isEmpty() || saved == m_cards.end()) {
            return {};
        }
        number = saved->number;
    }
    const auto expiry = card.contains(QStringLiteral("expiry"))
        ? expiryOf(card.value(QStringLiteral("expiry")).toString())
        : std::pair {card.value(QStringLiteral("expiryMonth")).toInt(),
              card.value(QStringLiteral("expiryYear")).toInt()};
    const auto valid = expiry ? validExpiry(expiry->first, expiry->second) : std::nullopt;
    if (!valid) {
        return {};
    }
    Card kept {.id = id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id,
        .number = number,
        .name = card.value(QStringLiteral("name")).toString().trimmed(),
        .expiryMonth = valid->first,
        .expiryYear = valid->second,
        .nickname = card.value(QStringLiteral("nickname")).toString().trimmed(),
        .added = saved == m_cards.end() ? QDateTime::currentMSecsSinceEpoch() : saved->added};
    if (saved == m_cards.end()) {
        m_cards.append(kept);
    } else {
        *saved = kept;
    }
    emit changed();

    auto *keyring = m_keyring.get();
    onWorker([this, keyring, id = kept.id, secret = secretOf(kept)] {
        if (!keyring->store(id, secret)) {
            QMetaObject::invokeMethod(this, [this] { reread(); }, Qt::QueuedConnection);
        }
    });
    return kept.id;
}

bool PaymentCards::remove(const QString &id)
{
    if (m_state != State::Ready
        || m_cards.removeIf([&id](const Card &card) { return card.id == id; }) == 0) {
        return false;
    }
    emit changed();
    auto *keyring = m_keyring.get();
    onWorker([this, keyring, id] {
        if (!keyring->remove(id)) {
            QMetaObject::invokeMethod(this, [this] { reread(); }, Qt::QueuedConnection);
        }
    });
    return true;
}

// A read already on its way answers for the cards before they went, so it is
// let go and the keyring is read again once they have.
void PaymentCards::removeAll()
{
    ++m_generation;
    m_cards.clear();
    emit changed();
    auto *keyring = m_keyring.get();
    onWorker([keyring] {
        if (!keyring->available()) {
            return;
        }
        for (const auto &item : keyring->items().value_or(QList<KeyringItem> {})) {
            keyring->remove(item.id);
        }
    });
    if (m_state == State::Reading) {
        reread();
    }
}

QVariantMap PaymentCards::fill(const QString &id) const
{
    const auto card = std::find_if(
        m_cards.cbegin(), m_cards.cend(), [&id](const Card &kept) { return kept.id == id; });
    if (card == m_cards.cend()) {
        return {};
    }
    auto filled = shown(*card);
    filled.insert(QStringLiteral("number"), card->number);
    return filled;
}

bool PaymentCards::holds(const QString &number) const
{
    const auto digits = cardNumber(number);
    return !digits.isEmpty()
        && std::any_of(m_cards.cbegin(), m_cards.cend(),
            [&digits](const Card &card) { return card.number == digits; });
}

} // namespace omaweb
