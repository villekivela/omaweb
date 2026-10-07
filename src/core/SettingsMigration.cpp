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

#include <algorithm>
#include <array>

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

    QJsonObject readObject(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    // A row's string as the type the file holds, or undefined for one that
    // does not read as that type.
    QJsonValue typed(const QString &key, const QString &text)
    {
        const auto fallback = SettingsFile::defaultValue(key);
        if (fallback.isBool()) {
            if (text == QLatin1String("true") || text == QLatin1String("false")) {
                return text == QLatin1String("true");
            }
            return QJsonValue::Undefined;
        }
        if (fallback.isDouble()) {
            bool ok = false;
            const auto number = text.toInt(&ok);
            return ok ? QJsonValue(number) : QJsonValue(QJsonValue::Undefined);
        }
        return text;
    }

} // namespace

bool migrateSettings(const QString &configRoot, SessionStore &store)
{
    if (configRoot.isEmpty()) {
        return true;
    }
    const QDir config(configRoot);
    if (config.exists(SettingsFile::fileName())) {
        for (const auto &name : SettingsFile::retiredFileNames()) {
            if (config.exists(name)) {
                qWarning("Omaweb ignores %s, which an earlier version kept settings in: %s has "
                         "taken its place.",
                    qPrintable(config.filePath(name)), qPrintable(SettingsFile::fileName()));
            }
        }
        return true;
    }

    QJsonObject values;
    const auto carry = [&values](const QString &key, const QJsonValue &value) {
        if (!value.isUndefined() && SettingsFile::accepts(key, value)) {
            values.insert(key, value);
        }
    };
    const auto missing = QString(QChar(0));
    for (const auto &key : settingRows) {
        const auto text = store.preference(key, missing);
        if (text != missing) {
            carry(key, typed(key, text));
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
    carry(QStringLiteral("known-extensions"), extensions);

    const auto interface = readObject(config.filePath(QStringLiteral("interface.json")));
    carry(QStringLiteral("font-size"), interface.value(QStringLiteral("font-size")));
    carry(QStringLiteral("page-fonts"), interface.value(QStringLiteral("page-fonts")));
    const auto downloads = readObject(config.filePath(QStringLiteral("downloads.json")));
    carry(QStringLiteral("download-directory"), downloads.value(QStringLiteral("directory")));
    const auto privacy = readObject(config.filePath(QStringLiteral("privacy.json")));
    for (auto it = privacy.constBegin(); it != privacy.constEnd(); ++it) {
        if (std::ranges::find(agentKeys, it.key()) == agentKeys.end()) {
            carry(it.key(), it.value());
        }
    }

    SettingsFile settings(configRoot);
    if (!settings.merge(values)) {
        qWarning("Omaweb could not write %s, and keeps reading settings where it did before.",
            qPrintable(settings.path()));
        return false;
    }
    for (const auto &key : agentKeys) {
        if (privacy.contains(key)) {
            AgentsFile::write(configRoot, key, privacy.value(key));
        }
    }

    for (const auto &name : SettingsFile::retiredFileNames()) {
        if (config.exists(name) && !QFile::remove(config.filePath(name))) {
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
