#include "HttpsOnly.h"

#include "BrowserController.h"
#include "PrivacyFile.h"

#include <algorithm>
#include <utility>

namespace omaweb {
namespace {

    constexpr QLatin1StringView enabledKey("https-only");

    // How long after an upgrade a plain request to the same host counts as the
    // site sending the load back. A redirect arrives within the load; a new
    // navigation the reader makes later is theirs.
    constexpr qint64 kLoopWindowMs = 15000;
    // How many upgraded addresses a Space keeps an account of. Only the last
    // few loads are ever asked about.
    constexpr qsizetype kKeptUpgrades = 64;

    QString originOf(const QUrl &url)
    {
        return url.scheme() + QStringLiteral("://") + url.host().toLower()
            + (url.port() >= 0 ? QStringLiteral(":%1").arg(url.port()) : QString());
    }

    QString addressOf(const QUrl &url) { return url.adjusted(QUrl::RemoveFragment).toString(); }

} // namespace

HttpsOnly::HttpsOnly(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_configRoot(std::move(configRoot))
{
    load();
}

bool HttpsOnly::enabled() const { return m_enabled; }

void HttpsOnly::setEnabled(bool enabled)
{
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    save();
    emit enabledChanged();
}

void HttpsOnly::setRemembered(Remembered remembered) { m_remembered = std::move(remembered); }

bool HttpsOnly::exempt(const QUrl &url, const QString &spaceId)
{
    if (BrowserController::localDevelopmentHost(url.host())) {
        return true;
    }
    auto &space = m_spaces[spaceId];
    const auto origin = originOf(url);
    const auto allowed = space.allowedOnce.value(origin);
    if (allowed.isValid() && allowed.msecsTo(QDateTime::currentDateTimeUtc()) <= kLoopWindowMs) {
        return true;
    }
    space.allowedOnce.remove(origin);
    return m_remembered && m_remembered(spaceId, origin);
}

QUrl HttpsOnly::upgrade(const QUrl &url, const QString &spaceId)
{
    if (!m_enabled || url.scheme() != QStringLiteral("http") || url.host().isEmpty()
        || exempt(url, spaceId)) {
        return {};
    }
    auto upgraded = url;
    upgraded.setScheme(QStringLiteral("https"));
    auto &space = m_spaces[spaceId];
    const auto now = QDateTime::currentDateTimeUtc();
    if (space.upgrades.size() >= kKeptUpgrades) {
        const auto oldest = std::ranges::min_element(space.upgrades,
            [](const Upgrade &left, const Upgrade &right) { return left.at < right.at; });
        space.upgrades.erase(oldest);
    }
    space.upgrades.insert(addressOf(upgraded), {url, now});
    space.pending.insert(url.host().toLower(), now);
    space.refused.remove(addressOf(url));
    return upgraded;
}

bool HttpsOnly::refusesDowngrade(const QUrl &url, const QString &spaceId)
{
    if (!m_enabled || url.scheme() != QStringLiteral("http")) {
        return false;
    }
    auto &space = m_spaces[spaceId];
    const auto host = url.host().toLower();
    const auto since = space.pending.value(host);
    if (!since.isValid() || since.msecsTo(QDateTime::currentDateTimeUtc()) > kLoopWindowMs) {
        space.pending.remove(host);
        return false;
    }

    space.pending.remove(host);
    space.refused.insert(addressOf(url), QStringLiteral("downgrade"));
    return true;
}

void HttpsOnly::refuse(const QUrl &url, const QString &spaceId)
{
    // Nothing went out, so there is no upgraded load for the site to send back.
    auto &space = m_spaces[spaceId];
    auto upgraded = url;
    upgraded.setScheme(QStringLiteral("https"));
    space.upgrades.remove(addressOf(upgraded));
    space.pending.remove(url.host().toLower());
    space.refused.insert(addressOf(url), QStringLiteral("form"));
}

QVariantMap HttpsOnly::failure(const QString &spaceId, const QUrl &url) const
{
    const auto space = m_spaces.constFind(spaceId);
    if (space == m_spaces.cend()) {
        return {};
    }
    const auto address = addressOf(url);
    const auto refused = space->refused.constFind(address);
    if (refused != space->refused.cend()) {
        return {{QStringLiteral("plainUrl"), url}, {QStringLiteral("host"), url.host()},
            {QStringLiteral("reason"), *refused}};
    }
    const auto upgraded = space->upgrades.constFind(address);
    if (upgraded == space->upgrades.cend()) {
        return {};
    }
    return {{QStringLiteral("plainUrl"), upgraded->plainUrl},
        {QStringLiteral("host"), upgraded->plainUrl.host()},
        {QStringLiteral("reason"), QStringLiteral("unreachable")}};
}

bool HttpsOnly::upgradedTo(const QString &spaceId, const QUrl &url) const
{
    const auto space = m_spaces.constFind(spaceId);
    return space != m_spaces.cend() && space->upgrades.contains(addressOf(url));
}

void HttpsOnly::arrived(const QString &spaceId, const QUrl &url)
{
    auto &space = m_spaces[spaceId];
    space.pending.remove(url.host().toLower());
    space.refused.remove(addressOf(url));
    space.allowedOnce.remove(originOf(url));
}

void HttpsOnly::allowOnce(const QString &spaceId, const QUrl &url)
{
    if (url.scheme() == QStringLiteral("http") && !url.host().isEmpty()) {
        auto &space = m_spaces[spaceId];
        space.allowedOnce.insert(originOf(url), QDateTime::currentDateTimeUtc());
        space.pending.remove(url.host().toLower());
        space.refused.remove(addressOf(url));
    }
}

// Only an explicit `false` turns the mode off; a file that cannot be read the
// way it is written leaves it on.
void HttpsOnly::load()
{
    const auto value = PrivacyFile::read(m_configRoot, enabledKey);
    m_enabled = value.isBool() ? value.toBool() : true;
}

void HttpsOnly::save() const { PrivacyFile::write(m_configRoot, enabledKey, m_enabled); }

} // namespace omaweb
