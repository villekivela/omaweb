#include "HttpsOnly.h"

#include "BrowserController.h"
#include "SettingsFile.h"

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

    // The origin as the browser keeps a site's permissions under it, so a
    // choice the reader made for good is found again.
    QString originOf(const QUrl &url) { return BrowserController::normalizedOrigin(url); }

    bool within(const QDateTime &since, qint64 windowMs)
    {
        return since.isValid() && since.msecsTo(QDateTime::currentDateTimeUtc()) <= windowMs;
    }

    QString addressOf(const QUrl &url) { return url.adjusted(QUrl::RemoveFragment).toString(); }

} // namespace

HttpsOnly::HttpsOnly(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_settings(std::move(configRoot))
{
    connect(&m_settings, &SettingsFile::changed, this, [this](const QStringList &keys) {
        if (keys.contains(enabledKey)) {
            load();
        }
    });
    load();
}

bool HttpsOnly::enabled() const { return m_enabled; }

void HttpsOnly::setEnabled(bool enabled)
{
    if (enabled == m_enabled) {
        return;
    }
    // Applied when the file says it was written, the way an edit made there is.
    // A write the file refuses changes nothing, and saying so draws a switch
    // the reader flipped back to where it stands.
    if (!m_settings.set(enabledKey, enabled)) {
        emit enabledChanged();
    }
}

void HttpsOnly::setRemembered(Remembered remembered) { m_remembered = std::move(remembered); }

// The hosts the Omnibar sends a typed address to over plain HTTP: a
// Local-development site's, and a name without a dot given a port, such as
// `devbox:8080`, which is a machine on the reader's network.
bool HttpsOnly::exempt(const QUrl &url, const QString &spaceId)
{
    const auto host = url.host();
    if (BrowserController::localDevelopmentHost(host)
        || (url.port() >= 0 && !host.contains(QLatin1Char('.')))) {
        return true;
    }
    auto &space = m_spaces[spaceId];
    const auto origin = originOf(url);
    if (within(space.allowedOnce.value(origin), kLoopWindowMs)) {
        return true;
    }
    space.allowedOnce.remove(origin);
    return m_remembered && m_remembered(spaceId, origin);
}

bool HttpsOnly::upgradable(const QUrl &url, const QString &spaceId)
{
    return m_enabled && url.scheme() == QStringLiteral("http") && !url.host().isEmpty()
        && !exempt(url, spaceId);
}

// A site sending an upgraded load straight back is refused, not upgraded again.
bool HttpsOnly::sendsOverHttps(const QString &spaceId, const QUrl &url)
{
    return !within(m_spaces.value(spaceId).pending.value(url.host().toLower()), kLoopWindowMs)
        && upgradable(url, spaceId);
}

QUrl HttpsOnly::upgrade(const QUrl &url, const QString &spaceId)
{
    if (!upgradable(url, spaceId)) {
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
    const auto since = space.pending.take(host);
    if (!within(since, kLoopWindowMs)) {
        return false;
    }
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

void HttpsOnly::forgetLoad(Space &space, const QString &host)
{
    space.pending.remove(host);
    space.upgrades.removeIf(
        [&host](const auto &upgrade) { return QUrl(upgrade.key()).host().toLower() == host; });
}

QVariantMap HttpsOnly::failed(const QString &spaceId, const QUrl &url)
{
    const auto found = m_spaces.find(spaceId);
    if (found == m_spaces.end()) {
        return {};
    }
    auto &space = *found;
    const auto address = addressOf(url);
    QVariantMap failure;
    if (const auto refused = space.refused.take(address); !refused.isEmpty()) {
        failure = {{QStringLiteral("plainUrl"), url}, {QStringLiteral("host"), url.host()},
            {QStringLiteral("reason"), refused}};
    } else if (const auto upgraded = space.upgrades.constFind(address);
        upgraded != space.upgrades.cend()) {
        failure = {{QStringLiteral("plainUrl"), upgraded->plainUrl},
            {QStringLiteral("host"), upgraded->plainUrl.host()},
            {QStringLiteral("reason"), QStringLiteral("unreachable")}};
    }
    forgetLoad(space, url.host().toLower());
    return failure;
}

bool HttpsOnly::arrived(const QString &spaceId, const QUrl &url)
{
    auto &space = m_spaces[spaceId];
    const auto host = url.host().toLower();
    const auto upgraded = url.scheme() == QStringLiteral("https")
        && within(space.pending.value(host), kLoopWindowMs);
    forgetLoad(space, host);
    space.refused.remove(addressOf(url));
    space.allowedOnce.remove(originOf(url));
    return upgraded;
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
    const auto value = m_settings.value(enabledKey).toBool(true);
    if (value != m_enabled) {
        m_enabled = value;
        emit enabledChanged();
    }
}

} // namespace omaweb
