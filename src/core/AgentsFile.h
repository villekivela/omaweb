#pragma once

#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>

namespace omaweb {

// What an Agent may do, `agents.json` in the configuration directory, kept out
// of `settings.json` so a reader who copies their settings to another machine
// does not let Agents in there by accident (ADR 0051, ADR 0061). Each key is
// read and written by the class that owns it, and a write leaves every other
// key as it was.
namespace AgentsFile {

    QString fileName();
    // The stored value, or an undefined one when the file is missing or
    // cannot be read the way it is written. The caller keeps its default for
    // anything but the type it expects.
    QJsonValue read(const QString &configRoot, QLatin1StringView key);
    // A null or undefined value removes the key. False when the file could not
    // be written.
    bool write(const QString &configRoot, QLatin1StringView key, const QJsonValue &value);

} // namespace AgentsFile

} // namespace omaweb
