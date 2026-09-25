#include "SecureDns.h"

#include "PrivacyFile.h"

#include <QUrl>

#include <array>
#include <utility>

namespace omaweb {
namespace {

    constexpr QLatin1StringView resolverKey("secure-dns");
    constexpr QLatin1StringView customTemplateKey("secure-dns-template");
    constexpr QLatin1StringView customId("custom");

    struct NamedResolver {
        QLatin1StringView id;
        QLatin1StringView title;
        QLatin1StringView serverTemplate;
    };

    // Three operators that publish a no-logging policy for their public DoH
    // service. The order is the order Settings lists them in.
    constexpr std::array namedResolvers {
        NamedResolver {QLatin1StringView("quad9"), QLatin1StringView("Quad9"),
            QLatin1StringView("https://dns.quad9.net/dns-query")},
        NamedResolver {QLatin1StringView("cloudflare"), QLatin1StringView("Cloudflare"),
            QLatin1StringView("https://cloudflare-dns.com/dns-query")},
        NamedResolver {QLatin1StringView("mullvad"), QLatin1StringView("Mullvad"),
            QLatin1StringView("https://dns.mullvad.net/dns-query")},
    };

    const NamedResolver *namedResolver(const QString &id)
    {
        for (const auto &resolver : namedResolvers) {
            if (resolver.id == id) {
                return &resolver;
            }
        }
        return nullptr;
    }

    // A DoH template is an `https:` address with a host, optionally ending in
    // the `{?dns}` variable. Anything else would send names in the clear or
    // nowhere, so it is refused before it is saved.
    bool isServerTemplate(const QString &address)
    {
        auto candidate = address;
        candidate.remove(QLatin1StringView("{?dns}"));
        const QUrl url(candidate, QUrl::StrictMode);
        return url.isValid() && url.scheme() == QLatin1StringView("https") && !url.host().isEmpty()
            && !candidate.contains(QLatin1Char(' '));
    }

} // namespace

SecureDns::SecureDns(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_configRoot(std::move(configRoot))
{
    load();
}

QString SecureDns::resolver() const { return m_resolver; }

QString SecureDns::customTemplate() const { return m_customTemplate; }

QString SecureDns::serverTemplate() const
{
    if (m_resolver == customId) {
        return m_customTemplate;
    }
    const auto *named = namedResolver(m_resolver);
    return named ? QString(named->serverTemplate) : QString();
}

QString SecureDns::resolverTitle() const
{
    const auto *named = namedResolver(m_resolver);
    return named ? QString(named->title) : serverTemplate();
}

QVariantList SecureDns::resolvers() const
{
    QVariantList listed;
    for (const auto &resolver : namedResolvers) {
        listed.append(QVariantMap {
            {QStringLiteral("id"), QString(resolver.id)},
            {QStringLiteral("title"), QString(resolver.title)},
            {QStringLiteral("template"), QString(resolver.serverTemplate)},
        });
    }
    return listed;
}

bool SecureDns::useResolver(const QString &id)
{
    if (!namedResolver(id)) {
        return false;
    }
    if (m_resolver != id) {
        m_resolver = id;
        save();
        emit changed();
    }
    return true;
}

bool SecureDns::useCustom(const QString &address)
{
    const auto trimmed = address.trimmed();
    if (!isServerTemplate(trimmed)) {
        return false;
    }
    if (m_resolver != customId || m_customTemplate != trimmed) {
        m_resolver = customId;
        m_customTemplate = trimmed;
        save();
        emit changed();
    }
    return true;
}

void SecureDns::turnOff()
{
    if (m_resolver.isEmpty()) {
        return;
    }
    m_resolver.clear();
    save();
    emit changed();
}

// A value the file cannot vouch for leaves names to the system, which is the
// default: a resolver is only ever one the reader chose.
void SecureDns::load()
{
    const auto resolver = PrivacyFile::read(m_configRoot, resolverKey).toString();
    const auto customTemplate = PrivacyFile::read(m_configRoot, customTemplateKey).toString();
    if (isServerTemplate(customTemplate)) {
        m_customTemplate = customTemplate;
    }
    if (namedResolver(resolver) || (resolver == customId && !m_customTemplate.isEmpty())) {
        m_resolver = resolver;
    }
}

void SecureDns::save() const
{
    PrivacyFile::write(m_configRoot, resolverKey, m_resolver);
    if (!m_customTemplate.isEmpty()) {
        PrivacyFile::write(m_configRoot, customTemplateKey, m_customTemplate);
    }
}

} // namespace omaweb
