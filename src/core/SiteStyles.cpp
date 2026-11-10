#include "SiteStyles.h"

#include <QDir>
#include <QFile>
#include <QHostAddress>

#include <utility>

namespace omaweb {

namespace {

    constexpr QLatin1StringView suffix(".css");

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

} // namespace

SiteStyles::SiteStyles(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_configRoot(std::move(configRoot))
{
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
        return m_sheets.value(host);
    }
    // The broadest host first: `com`, then `github.com`, then `gist.github.com`.
    QStringList sheets;
    const auto labels = host.split(u'.');
    for (auto first = labels.size() - 1; first >= 0; --first) {
        const auto sheet = m_sheets.constFind(labels.sliced(first).join(u'.'));
        if (sheet != m_sheets.cend()) {
            sheets.append(*sheet);
        }
    }
    return sheets.join(u'\n');
}

void SiteStyles::reload()
{
    QMap<QString, QString> sheets;
    const QDir styles(directory());
    for (const auto &name : fileNames(styles.path())) {
        QFile file(styles.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning("Could not read the site style %s: %s", qPrintable(file.fileName()),
                qPrintable(file.errorString()));
            continue;
        }
        sheets.insert(hostOf(name), QString::fromUtf8(file.readAll()));
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
