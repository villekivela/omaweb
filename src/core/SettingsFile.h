#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QStringList>

namespace omaweb {

// The reader's settings, `settings.json` in the configuration directory
// (ADR 0061). The file is the source of truth: every read, write and merge of
// it goes through this class, and so does a change made outside Omaweb, which
// the watch on the file turns into `changed` with the keys it touched.
//
// The file holds only what the reader changed. Setting a key to its default
// removes it, so a default that changes in a later build reaches a reader who
// never chose otherwise. A write keeps every key Omaweb does not know and the
// order the reader put the keys in, puts a new key at the end, and is atomic.
//
// A file that does not parse, or names a version this build does not read, is
// never written: the values read last stand, or the defaults at start, and
// every write is refused until the reader fixes it. A single value of the
// wrong type falls back to its default alone. Both are logged and reported for
// Settings to show.
//
// Several instances may share a file, in one process or several. Each write
// reads the file again under a lock file, so two writers never drop each
// other's keys.
class SettingsFile final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path CONSTANT)
    // The path as a reader would type it, with their home as `~`.
    Q_PROPERTY(QString displayPath READ displayPath CONSTANT)
    Q_PROPERTY(bool readable READ readable NOTIFY statusChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY statusChanged)
    Q_PROPERTY(bool needsAttention READ needsAttention NOTIFY statusChanged)

public:
    static constexpr int version = 1;

    // An empty configuration root keeps the settings in memory alone, which
    // is what a test without a directory gets.
    explicit SettingsFile(QString configRoot, QObject *parent = nullptr);

    static QString fileName();
    // Every key the file holds for this build, in no particular order.
    static QStringList keys();
    // The value a key reads as when the reader has not set it, or an
    // undefined one for a key whose default is not Omaweb's to state, such as
    // a size the theme chooses.
    static QJsonValue defaultValue(const QString &key);

    QString path() const;
    QString displayPath() const;

    // The reader's value for a key, or its default where they set none or set
    // one this build cannot read.
    QJsonValue value(const QString &key) const;
    // Whether the reader set the key to a value this build can read.
    bool isSet(const QString &key) const;
    // Writes one key, or removes it when `value` is null, undefined or the
    // default.
    // False for a key this build does not name, a value of the wrong type, or
    // a file that could not be read or written; nothing changes then.
    bool set(const QString &key, const QJsonValue &value);
    // Writes several keys at once, each as `set` would, or none of them.
    bool merge(const QJsonObject &values);
    // Whether `set` would take this value for this key.
    static bool accepts(const QString &key, const QJsonValue &value);

    // Reads the file again, as the watch does on its own.
    void reload();

    bool readable() const;
    // What Settings says about the file: why it could not be read, which
    // values fell back, which keys were ignored, and which files from an
    // earlier version are still beside it. Empty when there is nothing.
    QString problem() const;
    // Whether the Settings button is marked for the file: it could not be
    // read, or a value in it could not be.
    bool needsAttention() const;
    QStringList invalidKeys() const;
    QStringList ignoredKeys() const;
    // The files an earlier version kept settings in, still present beside a
    // `settings.json` that replaced them and so never read.
    QStringList leftoverFiles() const;

    // The files settings lived in before this one, named for the migration.
    static QStringList retiredFileNames();

signals:
    void changed(const QStringList &keys);
    void statusChanged();

private:
    void apply(const QJsonObject &stored);
    void watch();

    QString m_configRoot;
    // The valid values the reader set, by key.
    QJsonObject m_values;
    bool m_readable = true;
    QString m_parseError;
    QStringList m_invalidKeys;
    QStringList m_ignoredKeys;
    QStringList m_leftoverFiles;
    QFileSystemWatcher m_watcher;
};

} // namespace omaweb
