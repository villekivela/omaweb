#include "SettingsFile.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QMutex>
#include <QSaveFile>
#include <QThread>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace omaweb {

// The order an object's keys were written in, and the same for each object
// inside it, by position. QJsonObject sorts its keys, so a file read
// through it alone and written back would lose the reader's layout.
struct SettingsFile::Layout {
    QStringList keys;
    std::vector<Layout> children;

    void add(const QString &key)
    {
        if (!keys.contains(key)) {
            keys.append(key);
            children.emplace_back();
        }
    }

    const Layout *child(const QString &key) const
    {
        const auto index = keys.indexOf(key);
        return index < 0 ? nullptr : &children.at(static_cast<std::size_t>(index));
    }

    Layout *child(const QString &key)
    {
        const auto index = keys.indexOf(key);
        return index < 0 ? nullptr : &children.at(static_cast<std::size_t>(index));
    }
};

namespace {

    constexpr auto versionKey = "version";

    enum class Kind { Switch, Number, Fraction, Text, Fonts, Extensions };

    struct Key {
        QLatin1StringView name;
        Kind kind;
        // Undefined where the default is not Omaweb's to state.
        QJsonValue fallback;
        // For text or a number: the values it may hold, or none for any. For a
        // fraction: its lowest and highest.
        QJsonArray choices {};
    };

    const QJsonValue unstated(QJsonValue::Undefined);

    const QList<Key> &schema()
    {
        static const QList<Key> keys {
            {QLatin1StringView("use-favicons"), Kind::Switch, true},
            {QLatin1StringView("tint-favicons"), Kind::Switch, false},
            // The launcher's icon on Omarchy: the package's, or the theme's.
            {QLatin1StringView("app-icon"), Kind::Text, QStringLiteral("black-and-white"),
                {QStringLiteral("black-and-white"), QStringLiteral("theme")}},
            {QLatin1StringView("sidebar-side"), Kind::Text, QStringLiteral("left"),
                {QStringLiteral("left"), QStringLiteral("right")}},
            {QLatin1StringView("floating-controls"), Kind::Switch, true},
            {QLatin1StringView("glance"), Kind::Switch, true},
            {QLatin1StringView("start-page-scene"), Kind::Text, QStringLiteral("crt-road")},
            {QLatin1StringView("start-page-glass"), Kind::Switch, true},
            {QLatin1StringView("space-colours"), Kind::Switch, true},
            // The limits Settings offers, from never to a week.
            {QLatin1StringView("put-away-unused-tabs-after"), Kind::Number, 12 * 60 * 60,
                {0, 60 * 60, 12 * 60 * 60, 24 * 60 * 60, 7 * 24 * 60 * 60}},
            {QLatin1StringView("release-check"), Kind::Switch, true},
            {QLatin1StringView("known-extensions"), Kind::Extensions, unstated},
            {QLatin1StringView("font-size"), Kind::Number, unstated},
            // How much of the desktop shows through the sidebar, over the theme's.
            {QLatin1StringView("sidebar-opacity"), Kind::Fraction, unstated, {0.5, 1.0}},
            {QLatin1StringView("page-fonts"), Kind::Fonts, unstated},
            {QLatin1StringView("download-directory"), Kind::Text, unstated},
            {QLatin1StringView("engine-suggestions"), Kind::Switch, false},
            {QLatin1StringView("global-privacy-control"), Kind::Switch, true},
            {QLatin1StringView("https-only"), Kind::Switch, true},
            {QLatin1StringView("webrtc-public-interfaces-only"), Kind::Switch, true},
            {QLatin1StringView("secure-dns"), Kind::Text, QString()},
            {QLatin1StringView("secure-dns-template"), Kind::Text, unstated},
        };
        return keys;
    }

    const Key *find(const QString &name)
    {
        for (const auto &key : schema()) {
            if (key.name == name) {
                return &key;
            }
        }
        return nullptr;
    }

    bool isWholeNumber(const QJsonValue &value)
    {
        return value.isDouble() && value.toDouble() == static_cast<double>(value.toInteger());
    }

    bool fits(const Key &key, const QJsonValue &value)
    {
        switch (key.kind) {
        case Kind::Switch:
            return value.isBool();
        case Kind::Number:
            return isWholeNumber(value) && (key.choices.isEmpty() || key.choices.contains(value));
        case Kind::Fraction:
            return value.isDouble() && value.toDouble() >= key.choices.at(0).toDouble()
                && value.toDouble() <= key.choices.at(1).toDouble();
        case Kind::Text:
            return value.isString() && (key.choices.isEmpty() || key.choices.contains(value));
        case Kind::Fonts: {
            if (!value.isObject()) {
                return false;
            }
            const auto fonts = value.toObject();
            for (auto it = fonts.constBegin(); it != fonts.constEnd(); ++it) {
                const bool family = it.key() == QLatin1String("standard-family")
                    || it.key() == QLatin1String("fixed-family");
                const bool size = it.key() == QLatin1String("font-size")
                    || it.key() == QLatin1String("minimum-font-size");
                if ((family && !it.value().isString()) || (size && !isWholeNumber(it.value()))
                    || (!family && !size)) {
                    return false;
                }
            }
            return true;
        }
        case Kind::Extensions: {
            if (!value.isObject()) {
                return false;
            }
            const auto extensions = value.toObject();
            return std::ranges::all_of(
                extensions, [](const QJsonValue &enabled) { return enabled.isBool(); });
        }
        }
        return false;
    }

    using Order = SettingsFile::Layout;

    // Reads the key order out of text QJsonDocument has already accepted, so
    // it can skip over anything it does not need to understand.
    class OrderReader {
    public:
        explicit OrderReader(const QByteArray &text)
            : m_text(text)
        {
        }

        Order read() { return value(); }

    private:
        void skipSpace()
        {
            while (m_position < m_text.size() && QChar::isSpace(m_text.at(m_position))) {
                ++m_position;
            }
        }

        QString string()
        {
            const auto start = m_position++;
            while (m_position < m_text.size() && m_text.at(m_position) != '"') {
                m_position += m_text.at(m_position) == '\\' ? 2 : 1;
            }
            ++m_position;
            const auto quoted = m_text.mid(start, m_position - start);
            return QJsonDocument::fromJson('[' + quoted + ']').array().at(0).toString();
        }

        Order value()
        {
            skipSpace();
            Order order;
            if (m_position >= m_text.size()) {
                return order;
            }
            const auto first = m_text.at(m_position);
            if (first == '{') {
                ++m_position;
                for (skipSpace(); m_position < m_text.size() && m_text.at(m_position) != '}';
                    skipSpace()) {
                    if (m_text.at(m_position) == ',') {
                        ++m_position;
                        continue;
                    }
                    const auto key = string();
                    skipSpace();
                    ++m_position; // The colon.
                    auto child = value();
                    if (!order.keys.contains(key)) {
                        order.keys.append(key);
                        order.children.push_back(std::move(child));
                    }
                }
                ++m_position;
            } else if (first == '[') {
                ++m_position;
                for (skipSpace(); m_position < m_text.size() && m_text.at(m_position) != ']';
                    skipSpace()) {
                    if (m_text.at(m_position) == ',') {
                        ++m_position;
                        continue;
                    }
                    value();
                }
                ++m_position;
            } else if (first == '"') {
                string();
            } else {
                while (m_position < m_text.size()
                    && !QByteArrayView(",]} \t\r\n").contains(m_text.at(m_position))) {
                    ++m_position;
                }
            }
            return order;
        }

        const QByteArray &m_text;
        qsizetype m_position = 0;
    };

    QByteArray scalar(const QJsonValue &value)
    {
        const auto wrapped = QJsonDocument(QJsonArray {value}).toJson(QJsonDocument::Compact);
        return wrapped.mid(1, wrapped.size() - 2);
    }

    // Two spaces a level, which is what a reader's editor most often writes
    // JSON with, rather than the four QJsonDocument indents by.
    void write(QByteArray &out, const QJsonValue &value, const Order *order, int depth)
    {
        const QByteArray inner((depth + 1) * 2, ' ');
        const QByteArray outer(depth * 2, ' ');
        if (value.isObject()) {
            const auto object = value.toObject();
            if (object.isEmpty()) {
                out += "{}";
                return;
            }
            QStringList keys;
            if (order) {
                for (const auto &key : order->keys) {
                    if (object.contains(key)) {
                        keys.append(key);
                    }
                }
            }
            for (const auto &key : object.keys()) {
                if (!keys.contains(key)) {
                    keys.append(key);
                }
            }
            out += "{\n";
            for (qsizetype index = 0; index < keys.size(); ++index) {
                const auto &key = keys.at(index);
                out += inner + scalar(key) + ": ";
                write(out, object.value(key), order ? order->child(key) : nullptr, depth + 1);
                out += index + 1 < keys.size() ? ",\n" : "\n";
            }
            out += outer + '}';
        } else if (value.isArray()) {
            const auto array = value.toArray();
            if (array.isEmpty()) {
                out += "[]";
                return;
            }
            out += "[\n";
            for (qsizetype index = 0; index < array.size(); ++index) {
                out += inner;
                write(out, array.at(index), nullptr, depth + 1);
                out += index + 1 < array.size() ? ",\n" : "\n";
            }
            out += outer + ']';
        } else {
            out += scalar(value);
        }
    }

    // What the file holds, or why it cannot be read: a parse error, a value
    // that is not an object, or a version this build does not read.
    struct Contents {
        QJsonObject object;
        Order order;
        QString error;
    };

    Contents parse(const QByteArray &text)
    {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(text, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            const auto line
                = text.first(std::min<qsizetype>(parseError.offset, text.size())).count('\n') + 1;
            return {.object = {},
                .order = {},
                .error = SettingsFile::tr("%1 on line %2").arg(parseError.errorString()).arg(line)};
        }
        if (!document.isObject()) {
            return {.object = {},
                .order = {},
                .error = SettingsFile::tr("it holds no object of settings")};
        }
        const auto object = document.object();
        const auto version = object.value(QLatin1String(versionKey));
        if (!version.isUndefined() && version != QJsonValue(SettingsFile::version)) {
            return {.object = {},
                .order = {},
                .error = SettingsFile::tr("it is version %1, and this Omaweb reads version %2")
                    .arg(scalar(version), QString::number(SettingsFile::version))};
        }
        return {.object = object, .order = OrderReader(text).read(), .error = {}};
    }

} // namespace

SettingsFile::SettingsFile(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_configRoot(std::move(configRoot))
{
    if (!m_configRoot.isEmpty()) {
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &SettingsFile::reload);
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &SettingsFile::reload);
    }
    reload();
}

QString SettingsFile::fileName() { return QStringLiteral("settings.json"); }

QStringList SettingsFile::keys()
{
    QStringList names;
    for (const auto &key : schema()) {
        names.append(key.name);
    }
    return names;
}

QJsonValue SettingsFile::defaultValue(const QString &name)
{
    const auto *key = find(name);
    return key ? key->fallback : QJsonValue(QJsonValue::Undefined);
}

QString SettingsFile::path() const
{
    return m_configRoot.isEmpty() ? QString() : QDir(m_configRoot).filePath(fileName());
}

QString SettingsFile::displayPath() const
{
    const auto home = QDir::homePath();
    const auto full = path();
    return full.startsWith(home + u'/') ? u'~' + full.mid(home.size()) : full;
}

QJsonValue SettingsFile::value(const QString &key) const
{
    return m_values.contains(key) ? m_values.value(key) : defaultValue(key);
}

bool SettingsFile::isSet(const QString &key) const { return m_values.contains(key); }

bool SettingsFile::accepts(const QString &key, const QJsonValue &value)
{
    const auto *known = find(key);
    return known && (value.isUndefined() || value.isNull() || fits(*known, value));
}

QString SettingsFile::text(const QJsonValue &value)
{
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return isWholeNumber(value) ? QString::number(value.toInteger())
                                    : QString::number(value.toDouble(), 'g', 6);
    }
    return value.toString();
}

QJsonValue SettingsFile::fromText(const QString &key, const QString &text)
{
    const auto *known = find(key);
    if (text.isEmpty()) {
        return QJsonValue::Null;
    }
    if (known && known->kind == Kind::Switch) {
        if (text != QLatin1String("true") && text != QLatin1String("false")) {
            return QJsonValue::Undefined;
        }
        return text == QLatin1String("true");
    }
    if (known && known->kind == Kind::Fraction) {
        bool number = false;
        const auto value = text.toDouble(&number);
        return number ? QJsonValue(value) : QJsonValue(QJsonValue::Undefined);
    }
    if (known && known->kind == Kind::Number) {
        bool number = false;
        const auto value = text.toInt(&number);
        return number ? QJsonValue(value) : QJsonValue(QJsonValue::Undefined);
    }
    return text;
}

bool SettingsFile::set(const QString &key, const QJsonValue &value)
{
    return merge({{key, value.isUndefined() ? QJsonValue(QJsonValue::Null) : value}});
}

bool SettingsFile::setMember(const QString &key, const QString &member, const QJsonValue &value)
{
    const auto *known = find(key);
    if (!known || (known->kind != Kind::Fonts && known->kind != Kind::Extensions)) {
        return false;
    }
    return change([&](QJsonObject &object, Order *order) {
        // What the file holds for the key now, or nothing where it holds a
        // value that is not one: this write replaces a value nobody could use.
        auto members = object.value(key).toObject();
        if (value.isNull() || value.isUndefined()) {
            members.remove(member);
        } else {
            members.insert(member, value);
        }
        if (!fits(*known, members)) {
            return false;
        }
        if (members.isEmpty()) {
            object.remove(key);
        } else {
            object.insert(key, members);
            if (order) {
                order->add(key);
                order->child(key)->add(member);
            }
        }
        return true;
    });
}

bool SettingsFile::merge(const QJsonObject &values)
{
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!accepts(it.key(), it.value())) {
            return false;
        }
    }
    // A key goes in where the reader put it, or in the order Omaweb lists its
    // keys when it is new.
    return change([&values](QJsonObject &object, Order *order) {
        for (const auto &key : keys()) {
            if (!values.contains(key)) {
                continue;
            }
            const auto value = values.value(key);
            if (value.isNull() || value == defaultValue(key)
                || (value.isObject() && value.toObject().isEmpty())) {
                object.remove(key);
            } else {
                object.insert(key, value);
                if (order) {
                    order->add(key);
                }
            }
        }
        return true;
    });
}

bool SettingsFile::change(const std::function<bool(QJsonObject &, Order *)> &edit)
{
    if (m_configRoot.isEmpty()) {
        auto object = m_values;
        if (!edit(object, nullptr)) {
            return false;
        }
        apply(object);
        return true;
    }
    if (!QDir().mkpath(m_configRoot)) {
        return false;
    }
    // Read again under the lock rather than written from what this instance
    // last read, so a key another writer set in between is kept.
    QLockFile lock(path() + QStringLiteral(".lock"));
    // QLockFile's own wait doubles its sleep each time it finds the lock
    // taken, so in five seconds it looks only six times. Another writer that
    // keeps the lock busy, as the Settings page clicked through during a Sync
    // restore does on a slow disk, can hold it at every one of those looks.
    // Looking every millisecond takes the lock in the first gap.
    const QDeadlineTimer deadline(5000);
    while (!lock.tryLock(0)) {
        if (lock.error() != QLockFile::LockFailedError || deadline.hasExpired()) {
            qWarning("Omaweb could not lock %s to write a setting", qPrintable(path()));
            return false;
        }
        QThread::msleep(1);
    }
    Contents contents;
    QFile existing(path());
    if (existing.open(QIODevice::ReadOnly)) {
        contents = parse(existing.readAll());
        existing.close();
        if (!contents.error.isEmpty()) {
            reload();
            return false;
        }
    }
    if (!edit(contents.object, &contents.order)) {
        return false;
    }
    if (!contents.object.contains(QLatin1String(versionKey))) {
        contents.object.insert(QLatin1String(versionKey), version);
        contents.order.keys.prepend(QLatin1String(versionKey));
        contents.order.children.insert(contents.order.children.begin(), Order {});
    }
    QByteArray text;
    write(text, contents.object, &contents.order, 0);
    text += '\n';
    QSaveFile file(path());
    if (!file.open(QIODevice::WriteOnly) || file.write(text) != text.size() || !file.commit()) {
        qWarning(
            "Omaweb could not write %s: %s", qPrintable(path()), qPrintable(file.errorString()));
        return false;
    }
    apply(contents.object);
    return true;
}

void SettingsFile::reload()
{
    if (m_configRoot.isEmpty()) {
        return;
    }
    watch();
    QStringList leftovers;
    if (QFileInfo::exists(path())) {
        for (const auto &name : retiredFileNames()) {
            if (QFileInfo::exists(QDir(m_configRoot).filePath(name))) {
                leftovers.append(name);
            }
        }
    }
    if (leftovers != m_leftoverFiles) {
        m_leftoverFiles = leftovers;
        log();
        emit statusChanged();
    }
    QFile file(path());
    if (!file.open(QIODevice::ReadOnly)) {
        apply({});
        return;
    }
    const auto contents = parse(file.readAll());
    if (!contents.error.isEmpty()) {
        if (m_parseError != contents.error) {
            m_parseError = contents.error;
            m_readable = false;
            log();
            emit statusChanged();
        }
        return;
    }
    apply(contents.object);
}

bool SettingsFile::readable() const { return m_readable; }

QString SettingsFile::problem() const
{
    QStringList lines;
    if (!m_readable) {
        lines.append(tr("%1 could not be read: %2. Omaweb keeps the settings it read last, and "
                        "Settings changes nothing in the file until it is fixed.")
                .arg(fileName(), m_parseError));
    }
    for (const auto &key : m_invalidKeys) {
        lines.append(
            tr("\"%1\" holds a value Omaweb cannot use, so it is at its default.").arg(key));
    }
    for (const auto &name : m_leftoverFiles) {
        lines.append(tr("%1 is left from an earlier version and is not read, because %2 has "
                        "taken its place. It can be deleted.")
                .arg(name, fileName()));
    }
    if (!m_ignoredKeys.isEmpty()) {
        lines.append(tr("Ignored, because Omaweb knows no such setting: %1.")
                .arg(m_ignoredKeys.join(QStringLiteral(", "))));
    }
    return lines.join(u'\n');
}

bool SettingsFile::needsAttention() const { return !m_readable || !m_invalidKeys.isEmpty(); }

QStringList SettingsFile::invalidKeys() const { return m_invalidKeys; }

QStringList SettingsFile::ignoredKeys() const { return m_ignoredKeys; }

QStringList SettingsFile::leftoverFiles() const { return m_leftoverFiles; }

QStringList SettingsFile::retiredFileNames()
{
    return {QStringLiteral("interface.json"), QStringLiteral("downloads.json"),
        QStringLiteral("privacy.json")};
}

void SettingsFile::apply(const QJsonObject &stored)
{
    QJsonObject values;
    QStringList invalid;
    QStringList ignored;
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        const auto *key = find(it.key());
        if (it.key() == QLatin1String(versionKey)) {
            continue;
        }
        if (!key) {
            ignored.append(it.key());
        } else if (fits(*key, it.value())) {
            values.insert(it.key(), it.value());
        } else {
            invalid.append(it.key());
        }
    }
    QStringList changedKeys;
    for (const auto &key : keys()) {
        if (values.value(key) != m_values.value(key)) {
            changedKeys.append(key);
        }
    }
    m_values = values;
    const bool statusDiffers = !m_readable || invalid != m_invalidKeys || ignored != m_ignoredKeys;
    m_readable = true;
    m_parseError.clear();
    if (statusDiffers) {
        m_invalidKeys = invalid;
        m_ignoredKeys = ignored;
        log();
        emit statusChanged();
    }
    if (!changedKeys.isEmpty()) {
        emit changed(changedKeys);
    }
}

// Every instance on a file reads the same problems in it, so each is logged
// once for the process, when it first appears, rather than once an instance.
void SettingsFile::log() const
{
    static QMutex mutex;
    static QHash<QString, QStringList> logged;
    const QMutexLocker locker(&mutex);
    auto &previous = logged[path()];
    QStringList current;
    // True the first time a problem is seen since the file was last without it.
    const auto fresh = [&previous, &current](const QString &problem) {
        current.append(problem);
        return !previous.contains(problem);
    };
    if (!m_readable && fresh(QStringLiteral("unreadable:") + m_parseError)) {
        qWarning("Omaweb could not read %s, %s. It keeps the settings it read last and writes "
                 "none until the file is fixed.",
            qPrintable(path()), qPrintable(m_parseError));
    }
    for (const auto &key : m_invalidKeys) {
        if (fresh(QStringLiteral("invalid:") + key)) {
            qWarning("Omaweb reads \"%s\" in %s as its default: the value is not one it can use.",
                qPrintable(key), qPrintable(path()));
        }
    }
    for (const auto &key : m_ignoredKeys) {
        if (fresh(QStringLiteral("ignored:") + key)) {
            qWarning("Omaweb ignores \"%s\" in %s, which names no setting it knows.",
                qPrintable(key), qPrintable(path()));
        }
    }
    for (const auto &name : m_leftoverFiles) {
        if (fresh(QStringLiteral("leftover:") + name)) {
            qWarning("Omaweb ignores %s, which an earlier version kept settings in: %s has taken "
                     "its place.",
                qPrintable(QDir(m_configRoot).filePath(name)), qPrintable(path()));
        }
    }
    previous = current;
}

// A write that replaces the file, as QSaveFile and most editors do, ends the
// watch on it, so the directory is watched too and the file is watched again
// each time it comes back. A file linked in from a dotfiles repository is
// replaced in that repository, so the directory it really lives in is watched
// as well.
void SettingsFile::watch()
{
    QDir().mkpath(m_configRoot);
    QStringList paths {m_configRoot};
    const QFileInfo file(path());
    if (file.exists()) {
        paths.append(file.absoluteFilePath());
        if (file.isSymLink() && !file.canonicalFilePath().isEmpty()) {
            paths.append(file.canonicalFilePath());
            paths.append(QFileInfo(file.canonicalFilePath()).absolutePath());
        }
    }
    const auto watched = m_watcher.files() + m_watcher.directories();
    for (const auto &candidate : paths) {
        if (!watched.contains(candidate)) {
            m_watcher.addPath(candidate);
        }
    }
}

} // namespace omaweb
