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

// Every default binding this file has already been offered, so a binding the
// user deleted is never handed back to them while a binding a later version
// introduces still arrives. It travels inside the configuration file, because
// the file is what a user copies to another machine.
const auto adoptedDefaultsKey = QStringLiteral("adoptedDefaults");

// The two maps that hold bindings. Other sections — "passthrough" — are site
// rules, and are only ever adopted whole.
const QStringList bindingSections = {
    QStringLiteral("bindings"),
    QStringLiteral("browser"),
};

QJsonObject readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

const QSet<QString> supportedCommands = {
    QStringLiteral("scroll-down"),
    QStringLiteral("scroll-up"),
    QStringLiteral("scroll-half-page-down"),
    QStringLiteral("scroll-half-page-up"),
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
    QStringLiteral("reload-bypassing-cache"),
    QStringLiteral("stop-loading"),
    QStringLiteral("open-address"),
    QStringLiteral("open-file"),
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
    QStringLiteral("copy-address"),
    QStringLiteral("find"),
    QStringLiteral("find-next"),
    QStringLiteral("find-previous"),
    QStringLiteral("zoom-in"),
    QStringLiteral("zoom-out"),
    QStringLiteral("zoom-reset"),
    QStringLiteral("print"),
    QStringLiteral("fullscreen"),
    QStringLiteral("developer-tools"),
    QStringLiteral("inspect-element"),
    QStringLiteral("open-page-context-menu"),
    QStringLiteral("shortcuts"),
    QStringLiteral("history"),
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
    // A binding this build cannot honour is dropped, and every other binding in
    // the file still loads. Refusing the whole file costs the reader their
    // keyboard entirely — in a browser driven from the keyboard — and one
    // configuration is shared by every Tanto on the machine: a command that
    // only one build knows about, or one retired since the file was written,
    // would otherwise take the rest of the keymap down with it. What was
    // dropped is named in the error message rather than passed over in silence.
    QStringList ignored;
    const auto parseSection = [&ignored](const QJsonObject &section,
                                  const QSet<QString> &supported) {
        QVariantMap parsed;
        for (auto it = section.begin(); it != section.end(); ++it) {
            const auto command = it.value().toString();
            if (it.key().isEmpty() || !supported.contains(command)) {
                ignored.append(QStringLiteral("%1 (%2)").arg(it.key(), command));
                continue;
            }
            parsed.insert(it.key(), command);
        }
        return parsed;
    };

    const auto parsedBindings = parseSection(bindings, supportedCommands);
    if (parsedBindings.isEmpty()) {
        m_errorMessage = QStringLiteral(
            "Keyboard navigation recognised none of its page bindings: %1")
                             .arg(ignored.join(QStringLiteral(", ")));
        return false;
    }

    const auto parsedBrowserBindings = parseSection(
        root.value(QStringLiteral("browser")).toObject(), supportedBrowserCommands);

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

    m_bindings = parsedBindings;
    m_browserBindings = parsedBrowserBindings;
    m_siteRules = std::move(parsedRules);
    m_enabled = root.value(QStringLiteral("enabled")).toBool(false);
    m_valid = true;
    m_errorMessage = ignored.isEmpty()
        ? QString{}
        : QStringLiteral("Ignored bindings this build does not know: %1")
              .arg(ignored.join(QStringLiteral(", ")));
    return true;
}

// A file written before a section existed — "browser", say — reads as "no
// bindings", which silently disables every shortcut in it; a file written
// before a binding existed simply never sees it. Both are upgrades, and both
// are handled here. Sections and bindings the user already has are left
// exactly as they are.
//
// The ledger is what separates "new in this release" from "the user deleted
// this". The first run after it arrives has nothing to go on but the file
// itself, so it offers every default that file lacks — once — and records the
// whole of the current defaults. From then on a deletion sticks.
bool KeyboardNavigation::adoptDefaults(const QString &configurationPath,
    const QString &defaultsPath)
{
    const auto defaults = readJsonObject(defaultsPath);
    auto settings = readJsonObject(configurationPath);
    if (defaults.isEmpty() || settings.isEmpty()) {
        return false;
    }

    const auto hadLedger = settings.contains(adoptedDefaultsKey);
    const auto ledger = settings.value(adoptedDefaultsKey).toObject();
    auto adopted = false;

    // Defaults Tanto has since changed its mind about. A key still carrying the
    // command Tanto shipped on it is Tanto's own former default rather than a
    // choice the reader made, so it follows the new decision: dropped where
    // there is no default for that key any more — `u` now scrolls the page, and
    // the old `g` sequences kept the page from ever receiving the first key of
    // `gg` — and replaced where the key now means something else. Any other
    // command on these keys is a user choice and stays put.
    //
    // Adoption alone cannot do this: it offers a default once and never argues
    // with a key the file already binds, which is right for the reader's own
    // bindings and wrong for Tanto's abandoned ones.
    auto browser = settings.value(QStringLiteral("browser")).toObject();
    const auto defaultBrowser = defaults.value(QStringLiteral("browser")).toObject();
    const QHash<QString, QString> retiredBrowserDefaults = {
        {QStringLiteral("u"), QStringLiteral("reopen-tab")},
        {QStringLiteral("gt"), QStringLiteral("next-tab")},
        {QStringLiteral("gT"), QStringLiteral("previous-tab")},
        {QStringLiteral("gs"), QStringLiteral("next-space")},
        {QStringLiteral("gn"), QStringLiteral("new-space")},
        // Copying the address is what this key does in every other browser, so
        // inspecting an element moved to the one Chromium uses for it.
        {QStringLiteral("Primary+Shift+C"), QStringLiteral("inspect-element")},
    };
    for (auto it = retiredBrowserDefaults.cbegin();
         it != retiredBrowserDefaults.cend(); ++it) {
        if (browser.value(it.key()).toString() != it.value()) {
            continue;
        }
        const auto replacement = defaultBrowser.value(it.key()).toString();
        if (replacement.isEmpty()) {
            browser.remove(it.key());
        } else {
            browser.insert(it.key(), replacement);
        }
        adopted = true;
    }
    if (adopted) {
        settings.insert(QStringLiteral("browser"), browser);
    }

    for (auto it = defaults.begin(); it != defaults.end(); ++it) {
        if (settings.contains(it.key())) {
            continue;
        }
        settings.insert(it.key(), it.value());
        adopted = true;
    }

    auto nextLedger = ledger;
    auto ledgerChanged = !hadLedger;
    for (const auto &section : bindingSections) {
        const auto defaultBindings = defaults.value(section).toObject();
        if (defaultBindings.isEmpty()) {
            continue;
        }
        auto bindings = settings.value(section).toObject();

        QSet<QString> offered;
        const auto recorded = ledger.value(section).toArray();
        for (const auto &entry : recorded) {
            offered.insert(entry.toString());
        }
        if (!hadLedger) {
            for (auto it = bindings.begin(); it != bindings.end(); ++it) {
                offered.insert(it.key());
            }
        }

        for (auto it = defaultBindings.begin(); it != defaultBindings.end(); ++it) {
            if (offered.contains(it.key()) || bindings.contains(it.key())) {
                continue;
            }
            bindings.insert(it.key(), it.value());
            adopted = true;
        }
        settings.insert(section, bindings);

        // The ledger holds every default this file has been offered, whether
        // it took it or already had it, so the same key is never offered twice.
        QSet<QString> record;
        for (const auto &entry : recorded) {
            record.insert(entry.toString());
        }
        for (auto it = defaultBindings.begin(); it != defaultBindings.end(); ++it) {
            record.insert(it.key());
        }
        auto names = QStringList(record.begin(), record.end());
        names.sort();
        if (names.size() != recorded.size()) {
            ledgerChanged = true;
        }
        nextLedger.insert(section, QJsonArray::fromStringList(names));
    }

    if (!adopted && !ledgerChanged) {
        return false;
    }
    settings.insert(adoptedDefaultsKey, nextLedger);

    QSaveFile destination(configurationPath);
    if (!destination.open(QIODevice::WriteOnly)) {
        return false;
    }
    destination.write(QJsonDocument(settings).toJson(QJsonDocument::Indented));
    return destination.commit();
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
