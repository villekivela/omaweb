#pragma once

#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>

namespace omaweb {

// The reader's privacy decisions, one file in the configuration directory
// (ADR 0016) named for the Settings section they belong to. Each decision is
// one key, read and written by the class that owns it, and a write leaves
// every other key as it was.
namespace PrivacyFile {

    // The stored value, or an undefined one when the file is missing or
    // cannot be read the way it is written. The caller keeps its default for
    // anything but the type it expects.
    QJsonValue read(const QString &configRoot, QLatin1StringView key);
    void write(const QString &configRoot, QLatin1StringView key, const QJsonValue &value);

} // namespace PrivacyFile

} // namespace omaweb
