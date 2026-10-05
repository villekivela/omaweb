#include "PaymentCardKeyring.h"

#include <QMutexLocker>
#include <QStringList>

#include <algorithm>

namespace omaweb {

qsizetype MemoryPaymentCardKeyring::Contents::size() const
{
    const QMutexLocker locker(&mutex);
    return items.size();
}

QByteArray MemoryPaymentCardKeyring::Contents::secret(const QString &id) const
{
    const QMutexLocker locker(&mutex);
    const auto found = std::find_if(
        items.cbegin(), items.cend(), [&id](const KeyringItem &item) { return item.id == id; });
    return found == items.cend() ? QByteArray() : found->secret;
}

QStringList MemoryPaymentCardKeyring::Contents::ids() const
{
    const QMutexLocker locker(&mutex);
    QStringList ids;
    for (const auto &item : items) {
        ids.append(item.id);
    }
    return ids;
}

MemoryPaymentCardKeyring::MemoryPaymentCardKeyring(std::shared_ptr<Contents> contents)
    : m_contents(std::move(contents))
{
}

bool MemoryPaymentCardKeyring::available()
{
    const QMutexLocker locker(&m_contents->mutex);
    return m_contents->available;
}

std::optional<QList<KeyringItem>> MemoryPaymentCardKeyring::items()
{
    const QMutexLocker locker(&m_contents->mutex);
    if (!m_contents->available) {
        return std::nullopt;
    }
    ++m_contents->reads;
    return m_contents->items;
}

bool MemoryPaymentCardKeyring::store(const QString &id, const QByteArray &secret)
{
    const QMutexLocker locker(&m_contents->mutex);
    if (!m_contents->available) {
        return false;
    }
    auto &items = m_contents->items;
    const auto found = std::find_if(
        items.begin(), items.end(), [&id](const KeyringItem &item) { return item.id == id; });
    if (found == items.end()) {
        items.append({.id = id, .secret = secret});
    } else {
        found->secret = secret;
    }
    return true;
}

bool MemoryPaymentCardKeyring::remove(const QString &id)
{
    const QMutexLocker locker(&m_contents->mutex);
    return m_contents->available
        && m_contents->items.removeIf([&id](const KeyringItem &item) { return item.id == id; }) > 0;
}

#ifndef OMAWEB_SECRET_SERVICE
namespace {

    class NoKeyring final : public PaymentCardKeyring {
    public:
        bool available() override { return false; }
        std::optional<QList<KeyringItem>> items() override { return std::nullopt; }
        bool store(const QString &, const QByteArray &) override { return false; }
        bool remove(const QString &) override { return false; }
    };

} // namespace

std::unique_ptr<PaymentCardKeyring> makeDesktopKeyring() { return std::make_unique<NoKeyring>(); }
#endif

} // namespace omaweb
