#include "SettingsMigration.h"

#include "AgentsFile.h"
#include "KnownExtensions.h"
#include "SessionStore.h"
#include "SettingsFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>
#include <array>
#include <optional>

namespace omaweb {
namespace {

    // The rows the session store held settings in, as the store wrote them:
    // every value a string.
    const QStringList settingRows {
        QStringLiteral("use-favicons"),
        QStringLiteral("tint-favicons"),
        QStringLiteral("sidebar-side"),
        QStringLiteral("floating-controls"),
        QStringLiteral("glance"),
        QStringLiteral("start-page-scene"),
        QStringLiteral("start-page-glass"),
        QStringLiteral("put-away-unused-tabs-after"),
        QStringLiteral("release-check"),
    };
    // The switch the Start page had before it had Scenes. Read once more here,
    // so a reader who turned it off arrives on None.
    const auto startPageRoad = QStringLiteral("start-page-road");
    constexpr std::array agentKeys {
        QLatin1StringView("allow-agents"), QLatin1StringView("agent-command")};

    QString extensionRow(const QString &key)
    {
        return QStringLiteral("known-extension-%1-enabled").arg(key);
    }

    // An old file's settings: none where there is no file, and no answer
    // where there is one that does not read as an object of settings, which
    // is a file the reader still has to look at.
    std::optional<QJsonObject> readObject(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QJsonObject {};
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning("Omaweb could not read %s, so it does not carry its settings and leaves it "
                     "for the reader to fix or delete.",
                qPrintable(path));
            return std::nullopt;
        }
        return document.object();
    }

} // namespace

bool migrateSettings(const QString &configRoot, SessionStore &store)
{
    if (configRoot.isEmpty()) {
        return true;
    }
    const QDir config(configRoot);
    // The reader's own file stands. An old file beside it is theirs to delete,
    // which SettingsFile logs and Settings names.
    if (config.exists(SettingsFile::fileName())) {
        return true;
    }

    QJsonObject values;
    // A value that is not one the key takes is said and left behind, rather
    // than carried into a file that would read it as the default anyway.
    const auto carry = [&values](
                           const QString &key, const QJsonValue &value, const QString &source) {
        if (value.isUndefined()) {
            return;
        }
        if (!SettingsFile::accepts(key, value)) {
            qWarning("Omaweb does not carry \"%s\" from %s into settings.json: the value is not "
                     "one it can use.",
                qPrintable(key), qPrintable(source));
            return;
        }
        values.insert(key, value);
    };
    const auto rowsSource = QStringLiteral("the session store");
    const auto missing = QString(QChar(0));
    for (const auto &key : settingRows) {
        const auto text = store.preference(key, missing);
        if (text != missing) {
            const auto typed = SettingsFile::fromText(key, text);
            carry(key, typed.isUndefined() ? QJsonValue(text) : typed, rowsSource);
        }
    }
    if (!values.contains(QStringLiteral("start-page-scene"))
        && store.preference(startPageRoad) == QLatin1String("false")) {
        values.insert(QStringLiteral("start-page-scene"), QStringLiteral("none"));
    }
    QJsonObject extensions;
    for (const auto &extension : knownExtensions()) {
        const auto text = store.preference(extensionRow(extension.key));
        if (text == QLatin1String("true")) {
            extensions.insert(extension.key, true);
        }
    }
    carry(QStringLiteral("known-extensions"), extensions, rowsSource);

    // Only an old file that was read is deleted once its settings are carried.
    QStringList carried;
    const auto read = [&config, &carried](const QString &name) {
        const auto object = readObject(config.filePath(name));
        if (object && config.exists(name)) {
            carried.append(name);
        }
        return object.value_or(QJsonObject {});
    };
    const auto interfaceName = QStringLiteral("interface.json");
    const auto interface = read(interfaceName);
    carry(QStringLiteral("font-size"), interface.value(QStringLiteral("font-size")), interfaceName);
    carry(
        QStringLiteral("page-fonts"), interface.value(QStringLiteral("page-fonts")), interfaceName);
    const auto downloadsName = QStringLiteral("downloads.json");
    const auto downloads = read(downloadsName);
    carry(QStringLiteral("download-directory"), downloads.value(QStringLiteral("directory")),
        downloadsName);
    const auto privacyName = QStringLiteral("privacy.json");
    const auto privacy = read(privacyName);
    for (auto it = privacy.constBegin(); it != privacy.constEnd(); ++it) {
        if (std::ranges::find(agentKeys, it.key()) == agentKeys.end()) {
            carry(it.key(), it.value(), privacyName);
        }
    }

    // The agent keys first: privacy.json is deleted once settings.json is
    // there, and a key that did not reach agents.json would go with it.
    for (const auto &key : agentKeys) {
        if (privacy.contains(key) && !AgentsFile::write(configRoot, key, privacy.value(key))) {
            qWarning("Omaweb could not write %s, and leaves its earlier settings where they are "
                     "to move them on the next start.",
                qPrintable(config.filePath(AgentsFile::fileName())));
            return false;
        }
    }
    SettingsFile settings(configRoot);
    if (!settings.merge(values)) {
        qWarning("Omaweb could not write %s, and leaves its earlier settings where they are to "
                 "move them on the next start.",
            qPrintable(settings.path()));
        return false;
    }

    for (const auto &name : std::as_const(carried)) {
        if (!QFile::remove(config.filePath(name))) {
            qWarning("Omaweb could not delete %s, which it no longer reads.",
                qPrintable(config.filePath(name)));
        }
    }
    QStringList rows = settingRows;
    rows.append(startPageRoad);
    for (const auto &extension : knownExtensions()) {
        rows.append(extensionRow(extension.key));
    }
    for (const auto &row : rows) {
        store.deletePreference(row);
    }
    return true;
}

} // namespace omaweb
