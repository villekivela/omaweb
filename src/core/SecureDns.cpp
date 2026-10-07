#include "SecureDns.h"

#include "SettingsFile.h"

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
    , m_settings(std::move(configRoot))
{
    connect(&m_settings, &SettingsFile::changed, this, [this](const QStringList &keys) {
        if (keys.contains(resolverKey) || keys.contains(customTemplateKey)) {
            load();
        }
    });
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
        save(id, m_customTemplate);
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
        save(customId, trimmed);
    }
    return true;
}

void SecureDns::turnOff()
{
    if (m_resolver.isEmpty()) {
        return;
    }
    save({}, m_customTemplate);
}

// A value the file cannot vouch for leaves names to the system, which is the
// default: a resolver is only ever one the reader chose.
void SecureDns::load()
{
    const auto resolver = m_settings.value(resolverKey).toString();
    const auto customTemplate = m_settings.value(customTemplateKey).toString();
    const auto chosenTemplate = isServerTemplate(customTemplate) ? customTemplate : QString();
    const auto chosenResolver
        = namedResolver(resolver) || (resolver == customId && !chosenTemplate.isEmpty())
        ? resolver
        : QString();
    if (chosenResolver != m_resolver || chosenTemplate != m_customTemplate) {
        m_resolver = chosenResolver;
        m_customTemplate = chosenTemplate;
        emit changed();
    }
}

// Applied when the file says it was written, the way an edit made there is.
void SecureDns::save(const QString &resolver, const QString &customTemplate)
{
    m_settings.merge({{QString(resolverKey), resolver},
        {QString(customTemplateKey),
            customTemplate.isEmpty() ? QJsonValue() : QJsonValue(customTemplate)}});
}

} // namespace omaweb
