#include "SiteStyles.h"

#include "LinkRules.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonObject>
#include <QSaveFile>

#include <utility>

namespace omaweb {

namespace {

    constexpr QLatin1StringView suffix(".css");
    // How long typing pauses before what was typed is written.
    constexpr int writePause = 400;
    constexpr QLatin1StringView switchesKey("site-styles");

    // A host as the DNS writes it, so a file named in the reader's own script
    // and an address in either form meet, and without the root's trailing
    // dot, which names the same host. An IP address stays as it is.
    QString canonicalHost(QString host)
    {
        host = host.toLower();
        if (host.endsWith(u'.')) {
            host.chop(1);
        }
        return QHostAddress(host).isNull() ? QString::fromLatin1(QUrl::toAce(host)) : host;
    }

    QString hostOf(const QString &fileName)
    {
        return canonicalHost(fileName.chopped(suffix.size()));
    }

    // A hidden element's rule, whole on its line so it can be read back and
    // taken out without touching a line the reader wrote.
    constexpr QLatin1StringView hiddenMark("/* Hidden by Omaweb */ ");
    constexpr QLatin1StringView hiddenRule(" { display: none !important; }");

    QString hiddenLine(const QString &selector) { return hiddenMark + selector + hiddenRule; }

} // namespace

SiteStyles::SiteStyles(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_configRoot(configRoot)
    , m_settings(std::move(configRoot))
{
    // A style switched on or off, here or in the file, restyles its pages.
    connect(&m_settings, &SettingsFile::changed, this, [this](const QStringList &keys) {
        if (keys.contains(switchesKey)) {
            emit changed();
        }
    });
    m_pause.setSingleShot(true);
    m_pause.setInterval(writePause);
    connect(&m_pause, &QTimer::timeout, this, &SiteStyles::writeUnsaved);
    if (m_configRoot.isEmpty()) {
        return;
    }
    // A save by rename drops the file's watch and adds an entry to the
    // directory, and a save in place changes the file alone, so both are
    // watched. The configuration directory is watched for `styles/` itself
    // being made or removed.
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &SiteStyles::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &SiteStyles::reload);
    reload();
}

QString SiteStyles::directoryName() { return QStringLiteral("styles"); }

QStringList SiteStyles::fileNames(const QString &directory)
{
    QStringList names;
    const auto entries = QDir(directory).entryList({QStringLiteral("*.css")}, QDir::Files);
    for (const auto &name : entries) {
        // Qt reads a path that starts with a colon as one of its resources,
        // so an IPv6 address cannot name a file.
        if (!name.startsWith(u'.') && !name.startsWith(u':') && !hostOf(name).isEmpty()) {
            names.append(name);
        }
    }
    return names;
}

QString SiteStyles::directory() const
{
    return m_configRoot.isEmpty() ? QString() : QDir(m_configRoot).filePath(directoryName());
}

QString SiteStyles::styleSheet(const QUrl &url) const
{
    const auto host = canonicalHost(url.host());
    if (host.isEmpty()) {
        return {};
    }
    // An IP address has no hosts above it: its last numbers are no domain.
    if (!QHostAddress(host).isNull()) {
        return enabled(host) ? m_sheets.value(host) : QString();
    }
    // The broadest host first: `com`, then `github.com`, then `gist.github.com`.
    QStringList sheets;
    const auto labels = host.split(u'.');
    for (auto first = labels.size() - 1; first >= 0; --first) {
        const auto name = labels.sliced(first).join(u'.');
        const auto sheet = m_sheets.constFind(name);
        if (sheet != m_sheets.cend() && enabled(name)) {
            sheets.append(*sheet);
        }
    }
    return sheets.join(u'\n');
}

SiteStyles::~SiteStyles() { writeUnsaved(); }

QString SiteStyles::source(const QString &host) const
{
    return m_sheets.value(canonicalHost(host));
}

void SiteStyles::edit(const QString &host, const QString &css)
{
    const auto key = canonicalHost(host);
    if (key.isEmpty() || m_sheets.value(key) == css) {
        return;
    }
    m_sheets.insert(key, css);
    if (!m_unsaved.contains(key)) {
        m_unsaved.insert(key);
        emit unsavedChanged();
    }
    m_pause.start();
    emit changed();
}

QStringList SiteStyles::unsaved() const
{
    auto hosts = QStringList(m_unsaved.cbegin(), m_unsaved.cend());
    hosts.sort();
    return hosts;
}

void SiteStyles::writeUnsaved()
{
    m_pause.stop();
    const auto hosts = unsaved();
    for (const auto &host : hosts) {
        write(host, m_sheets.value(host));
    }
}

QVariantList SiteStyles::styles() const
{
    QVariantList listed;
    for (auto it = m_sheets.cbegin(); it != m_sheets.cend(); ++it) {
        const auto &css = it.value();
        const auto lines = css.isEmpty() ? 0 : css.count(u'\n') + (css.endsWith(u'\n') ? 0 : 1);
        listed.append(QVariantMap {
            {QStringLiteral("host"), QUrl::fromAce(it.key().toLatin1())},
            {QStringLiteral("path"), path(it.key())},
            {QStringLiteral("lines"), static_cast<int>(lines)},
            {QStringLiteral("hidden"), static_cast<int>(hidden(it.key()).size())},
            {QStringLiteral("enabled"), enabled(it.key())},
        });
    }
    return listed;
}

QString SiteStyles::path(const QString &host) const
{
    const auto key = canonicalHost(host);
    if (key.isEmpty() || m_configRoot.isEmpty()) {
        return {};
    }
    return QDir(directory()).filePath(m_files.value(key, QUrl::fromAce(key.toLatin1()) + suffix));
}

bool SiteStyles::remove(const QString &host)
{
    const auto key = canonicalHost(host);
    if (!m_sheets.contains(key)) {
        return false;
    }
    const auto file = path(key);
    if (m_files.contains(key) && !QFile::remove(file)) {
        qWarning("Could not remove the site style %s", qPrintable(file));
        return false;
    }
    m_files.remove(key);
    m_sheets.remove(key);
    if (m_unsaved.remove(key)) {
        emit unsavedChanged();
    }
    m_settings.setMember(switchesKey, key, QJsonValue::Null);
    watch();
    emit changed();
    return true;
}

bool SiteStyles::enabled(const QString &host) const
{
    // Only an explicit `false` turns a style off.
    return m_settings.value(switchesKey).toObject().value(canonicalHost(host)).toBool(true);
}

bool SiteStyles::setEnabled(const QString &host, bool enabled)
{
    const auto key = canonicalHost(host);
    if (key.isEmpty()) {
        return false;
    }
    return m_settings.setMember(
        switchesKey, key, enabled ? QJsonValue(QJsonValue::Null) : QJsonValue(false));
}

bool SiteStyles::hide(const QString &host, const QString &selector)
{
    const auto key = canonicalHost(host);
    const auto trimmed = selector.trimmed();
    if (key.isEmpty() || trimmed.isEmpty()) {
        return false;
    }
    auto css = m_sheets.value(key);
    if (!css.isEmpty() && !css.endsWith(u'\n')) {
        css.append(u'\n');
    }
    return write(key, css + hiddenLine(trimmed) + u'\n');
}

bool SiteStyles::unhide(const QString &host, const QString &selector)
{
    const auto key = canonicalHost(host);
    const auto css = m_sheets.value(key);
    const auto line = hiddenLine(selector.trimmed());
    // Line by line, each with its end, so the rest is joined back as it was.
    qsizetype start = 0;
    while (start < css.size()) {
        auto end = css.indexOf(u'\n', start);
        end = end < 0 ? css.size() : end + 1;
        if (QStringView(css).sliced(start, end - start).trimmed() == line
            && !css.at(start).isSpace()) {
            return write(key, css.first(start) + css.sliced(end));
        }
        start = end;
    }
    return false;
}

QStringList SiteStyles::hidden(const QString &host) const
{
    QStringList selectors;
    const auto lines = m_sheets.value(canonicalHost(host)).split(u'\n');
    for (const auto &line : lines) {
        if (line.startsWith(hiddenMark) && line.endsWith(hiddenRule)) {
            selectors.append(line.sliced(hiddenMark.size()).chopped(hiddenRule.size()));
        }
    }
    return selectors;
}

bool SiteStyles::write(const QString &host, const QString &css)
{
    if (m_configRoot.isEmpty()) {
        return false;
    }
    const QDir styles(directory());
    QDir().mkpath(styles.path());
    // A file named in a host's own script keeps its name.
    const auto name = QFileInfo(path(host)).fileName();
    QSaveFile file(styles.filePath(name));
    if (!file.open(QIODevice::WriteOnly) || file.write(css.toUtf8()) < 0 || !file.commit()) {
        qWarning("Could not write the site style %s: %s", qPrintable(file.fileName()),
            qPrintable(file.errorString()));
        return false;
    }
    const auto wasUnsaved = m_unsaved.remove(host);
    const auto restyled = m_sheets.value(host) != css || !m_sheets.contains(host);
    m_files.insert(host, name);
    m_sheets.insert(host, css);
    watch();
    if (wasUnsaved) {
        emit unsavedChanged();
    }
    if (restyled) {
        emit changed();
    }
    return true;
}

QVariantList SiteStyles::hostsFor(const QUrl &url) const
{
    const auto host = canonicalHost(url.host());
    const auto site = siteFor(url);
    QVariantList hosts;
    if (host.isEmpty() || site.isEmpty()) {
        return hosts;
    }
    // The site is the host or one above it, so the walk ends on it.
    auto labels = host.split(u'.');
    const auto depth = site.count(u'.') + 1;
    while (labels.size() >= depth) {
        const auto name = labels.join(u'.');
        hosts.append(QVariantMap {{QStringLiteral("host"), QUrl::fromAce(name.toLatin1())},
            {QStringLiteral("exists"), m_sheets.contains(name)}});
        labels.removeFirst();
    }
    return hosts;
}

QString SiteStyles::siteFor(const QUrl &url) const
{
    const auto host = canonicalHost(url.host());
    if (host.isEmpty()) {
        return {};
    }
    const auto domain = linkrules::registrableDomain(url);
    return domain ? canonicalHost(*domain) : host;
}

void SiteStyles::reload()
{
    QMap<QString, QString> sheets;
    QMap<QString, QString> files;
    const QDir styles(directory());
    for (const auto &name : fileNames(styles.path())) {
        QFile file(styles.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning("Could not read the site style %s: %s", qPrintable(file.fileName()),
                qPrintable(file.errorString()));
            continue;
        }
        sheets.insert(hostOf(name), QString::fromUtf8(file.readAll()));
        files.insert(hostOf(name), name);
    }
    m_files = std::move(files);
    // What was typed and not yet written stands over the file it will replace.
    for (const auto &host : std::as_const(m_unsaved)) {
        sheets.insert(host, m_sheets.value(host));
    }
    watch();
    if (sheets != m_sheets) {
        m_sheets = std::move(sheets);
        emit changed();
    }
}

void SiteStyles::watch()
{
    QStringList paths {m_configRoot};
    const QDir styles(directory());
    if (styles.exists()) {
        paths.append(styles.absolutePath());
        for (const auto &name : fileNames(styles.path())) {
            paths.append(styles.absoluteFilePath(name));
        }
    }
    QDir().mkpath(m_configRoot);
    const auto watched = m_watcher.files() + m_watcher.directories();
    for (const auto &path : watched) {
        if (!paths.contains(path)) {
            m_watcher.removePath(path);
        }
    }
    for (const auto &path : std::as_const(paths)) {
        if (!watched.contains(path)) {
            m_watcher.addPath(path);
        }
    }
}

} // namespace omaweb
