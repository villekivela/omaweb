#include "KeyboardNavigation.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

namespace tanto {

namespace {

constexpr int supportedVersion = 1;

const QSet<QString> supportedCommands = {
    QStringLiteral("scroll-down"),
    QStringLiteral("scroll-up"),
    QStringLiteral("scroll-top"),
    QStringLiteral("scroll-bottom"),
    QStringLiteral("open-link"),
    QStringLiteral("open-link-background"),
};

// Commands Tanto itself performs. The page never sees these, so they may be
// bound to chords as well as to single keys.
const QSet<QString> supportedBrowserCommands = {
    QStringLiteral("back"),
    QStringLiteral("forward"),
    QStringLiteral("reload"),
    QStringLiteral("open-address"),
    QStringLiteral("command-panel"),
    QStringLiteral("new-tab"),
    QStringLiteral("close-tab"),
    QStringLiteral("reopen-tab"),
    QStringLiteral("next-tab"),
    QStringLiteral("previous-tab"),
    QStringLiteral("select-tab"),
    QStringLiteral("pin-tab"),
    QStringLiteral("move-tab"),
    QStringLiteral("next-space"),
    QStringLiteral("select-space"),
    QStringLiteral("new-space"),
    QStringLiteral("toggle-sidebar"),
    QStringLiteral("widen-sidebar"),
    QStringLiteral("narrow-sidebar"),
    QStringLiteral("reset-sidebar"),
    QStringLiteral("focus-sidebar"),
    QStringLiteral("focus-page"),
    QStringLiteral("settings"),
    QStringLiteral("private-window"),
    QStringLiteral("minimize-window"),
};

} // namespace

KeyboardNavigation::KeyboardNavigation(QString configurationPath, QObject *parent)
    : KeyboardNavigation(std::move(configurationPath), {}, parent)
{
}

KeyboardNavigation::KeyboardNavigation(QString configurationPath, QString pageScriptPath,
    QObject *parent)
    : QObject(parent)
    , m_configurationPath(std::move(configurationPath))
{
    if (!pageScriptPath.isEmpty()) {
        QFile script(pageScriptPath);
        if (script.open(QIODevice::ReadOnly)) {
            m_pageScript = QString::fromUtf8(script.readAll());
        }
    }
    load();
}

bool KeyboardNavigation::enabled() const
{
    return m_enabled;
}

bool KeyboardNavigation::valid() const
{
    return m_valid;
}

QVariantMap KeyboardNavigation::bindings() const
{
    return m_bindings;
}

QVariantMap KeyboardNavigation::browserBindings() const
{
    return m_browserBindings;
}

QString KeyboardNavigation::errorMessage() const
{
    return m_errorMessage;
}

QString KeyboardNavigation::pageScript() const
{
    return m_pageScript;
}

bool KeyboardNavigation::setEnabled(bool enabled)
{
    if (!m_valid) {
        return false;
    }
    if (m_enabled == enabled) {
        return true;
    }
    const auto previous = m_enabled;
    m_enabled = enabled;
    if (!save()) {
        m_enabled = previous;
        return false;
    }
    emit configurationChanged();
    return true;
}

QVariantMap KeyboardNavigation::configurationForUrl(const QUrl &url) const
{
    QVariantMap configuration = {
        {QStringLiteral("version"), supportedVersion},
        {QStringLiteral("enabled"), m_valid && m_enabled},
        {QStringLiteral("bindings"), m_bindings},
        {QStringLiteral("passthroughAll"), false},
        {QStringLiteral("passthroughKeys"), QStringList{}},
    };
    const auto host = url.host().toLower();
    qsizetype bestMatchLength = -1;
    for (auto it = m_siteRules.cbegin(); it != m_siteRules.cend(); ++it) {
        if (!hostMatches(host, it.key()) || it.key().size() <= bestMatchLength) {
            continue;
        }
        bestMatchLength = it.key().size();
        configuration.insert(QStringLiteral("passthroughAll"), it->passthroughAll);
        configuration.insert(QStringLiteral("passthroughKeys"), it->passthroughKeys);
    }
    return configuration;
}

bool KeyboardNavigation::load()
{
    QFile file(m_configurationPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_errorMessage = QStringLiteral("Could not read Keyboard navigation configuration: %1")
                             .arg(file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_errorMessage = QStringLiteral("Invalid Keyboard navigation JSON: %1")
                             .arg(parseError.errorString());
        return false;
    }
    const auto root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != supportedVersion) {
        m_errorMessage = QStringLiteral("Unsupported Keyboard navigation configuration version");
        return false;
    }
    const auto bindings = root.value(QStringLiteral("bindings")).toObject();
    if (bindings.isEmpty()) {
        m_errorMessage = QStringLiteral("Keyboard navigation requires at least one binding");
        return false;
    }
    QVariantMap parsedBindings;
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        const auto command = it.value().toString();
        if (it.key().isEmpty() || !supportedCommands.contains(command)) {
            m_errorMessage = QStringLiteral("Unsupported Keyboard navigation binding: %1")
                                 .arg(it.key());
            return false;
        }
        parsedBindings.insert(it.key(), command);
    }

    QVariantMap parsedBrowserBindings;
    const auto browser = root.value(QStringLiteral("browser")).toObject();
    for (auto it = browser.begin(); it != browser.end(); ++it) {
        const auto command = it.value().toString();
        if (it.key().isEmpty() || !supportedBrowserCommands.contains(command)) {
            m_errorMessage = QStringLiteral("Unsupported browser binding: %1").arg(it.key());
            return false;
        }
        parsedBrowserBindings.insert(it.key(), command);
    }

    QHash<QString, SiteRule> parsedRules;
    const auto passthrough = root.value(QStringLiteral("passthrough")).toObject();
    for (auto it = passthrough.begin(); it != passthrough.end(); ++it) {
        const auto host = it.key().trimmed().toLower();
        if (host.isEmpty() || host.contains(QLatin1Char('/'))) {
            m_errorMessage = QStringLiteral("Invalid passthrough host: %1").arg(it.key());
            return false;
        }
        const auto value = it.value().toObject();
        SiteRule rule;
        rule.passthroughAll = value.value(QStringLiteral("all")).toBool(false);
        const auto keys = value.value(QStringLiteral("keys")).toArray();
        for (const auto &key : keys) {
            if (!key.isString()) {
                m_errorMessage = QStringLiteral("Invalid passthrough key for %1").arg(host);
                return false;
            }
            rule.passthroughKeys.append(key.toString());
        }
        parsedRules.insert(host, rule);
    }

    m_bindings = std::move(parsedBindings);
    m_browserBindings = std::move(parsedBrowserBindings);
    m_siteRules = std::move(parsedRules);
    m_enabled = root.value(QStringLiteral("enabled")).toBool(false);
    m_valid = true;
    m_errorMessage.clear();
    return true;
}

bool KeyboardNavigation::save() const
{
    QFile source(m_configurationPath);
    if (!source.open(QIODevice::ReadOnly)) {
        return false;
    }
    auto document = QJsonDocument::fromJson(source.readAll());
    if (!document.isObject()) {
        return false;
    }
    auto root = document.object();
    root.insert(QStringLiteral("enabled"), m_enabled);
    QSaveFile destination(m_configurationPath);
    if (!destination.open(QIODevice::WriteOnly)) {
        return false;
    }
    destination.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return destination.commit();
}

bool KeyboardNavigation::hostMatches(const QString &host, const QString &ruleHost)
{
    return host == ruleHost || host.endsWith(QLatin1Char('.') + ruleHost);
}

} // namespace tanto
